/**
 *  Doom Port to the nRF52840 by Nicola Wrachien (next-hack in the comments)
 *
 *  This port is based on the excellent doomhack's GBA Doom Port,
 *  with Kippykip additions.
 *  
 *  Several data structures and functions have been optimized 
 *  to fit in only 256kB RAM of nRF52840 (GBA has 384 kB RAM). 
 *  Z-Depth Light has been restored with almost no RAM consumption!
 *  Tons of speed optimizations have been done, and the game now
 *  runs extremely fast, despite the much higher 3D resolution with
 *  respect to GBA.
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright (C) 2021 Nicola Wrachien (next-hack in the comments)
 *  on nRF52840 port.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Zone Memory Allocation. Neat.
 *
 * Neat enough to be rewritten by Lee Killough...
 *
 * Must not have been real neat :)
 *
 * Made faster and more general, and added wrappers for all of Doom's
 * memory allocation functions, including malloc() and similar functions.
 *
 *
 * next-hack
 * Changed back to old Zone.c because it makes no sense to use malloc, which
 * also requires additional memory to store malloc'ed block information!
 * Removed unused tags to keep the structure at a minimum size.
 * Used short pointers and bitfields to reduce overhead to 8 bytes/block instead
 * of 20 + malloc overhead (typically 8).
 *-----------------------------------------------------------------------------
 */
#ifdef CPLUSPLUS
#error
#endif

// use config.h if autoconf made one -- josh
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include "z_zone.h"
#include "doomstat.h"
#include "v_video.h"
#include "g_game.h"
#include "lprintf.h"
//
#include "global_data.h"
// Tunables

// Minimum chunk size at which blocks are allocated.
// note: having 8 bytes per block allows using 15:bit pointers
// even for 256kB ram. On one hand, on some blocks it will waste
// 4 bytes, on the other, we keey the overhead under 8 bytes/block
// instead of having to switch to 12 bytes/block.
#define CHUNK_SIZE 8
#define MAX_STATIC_ZONE (113600) // 113600
#define MEM_ALIGN CHUNK_SIZE
//#define ZMALLOC_STAT

// End Tunables

typedef struct memblock
{
    unsigned int next_sptrdw:15;
    unsigned int tag :2;
    unsigned int prev_sptrdw:15;
    unsigned short user_spptr;  // short pointer to pointer.
    unsigned short ssize;    // short size
} memblock_t;
/* size of block header
 * cph - base on sizeof(memblock_t), which can be larger than CHUNK_SIZE on
 * 64bit architectures */
static const int HEADER_SIZE = (sizeof(memblock_t) + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);

static int free_memory = 0;
static int largest_occupied = 0;

typedef struct
{
    // start / end cap for linked list
    memblock_t blocklist;

    // total bytes malloced, including header
    int size;

    memblock_t *rover;

} memzone_t;

__attribute__ ((section(".displayData_bss"), aligned (CHUNK_SIZE)))  uint8_t staticZone[MAX_STATIC_ZONE];

memzone_t *mainzone;
void* I_ZoneBase(int *size)
{
    *size = sizeof(staticZone);
    return staticZone;
}
static inline unsigned short getShortPtrDW(void *ptr)
{
    return ( (unsigned int) ptr >> 3) & 0x7FFF;
}
static inline void *getLongPtrDW(unsigned short ptrdw)
{
    if (!ptrdw)
        return 0;
    return (void*) (RAM_PTR_BASE | (  ptrdw << 3));
}
//
// Z_ClearZone
//
void Z_ClearZone(memzone_t *zone)
{
    memblock_t *block;

    // set the entire zone to one free block
    block = (memblock_t*) ((byte*) zone + sizeof(memzone_t));

    zone->blocklist.next_sptrdw = zone->blocklist.prev_sptrdw = getShortPtrDW(block);

    zone->blocklist.user_spptr = getShortPtr((void*) zone);
    zone->blocklist.tag = PU_STATIC;
    zone->rover = block;

    block->prev_sptrdw = block->next_sptrdw = getShortPtrDW(&zone->blocklist);

    // a free block.
    block->tag = PU_FREE;

    block->ssize = (zone->size - sizeof(memzone_t)) >> 2;
}

//
// Z_Init
//
void Z_Init(void)
{
    memblock_t *block;
    int size;

    mainzone = (memzone_t*) I_ZoneBase(&size);
    mainzone->size = size;

    // set the entire zone to one free block
    block = (memblock_t*) ((byte*) mainzone + sizeof(memzone_t));
    mainzone->blocklist.next_sptrdw = mainzone->blocklist.prev_sptrdw = getShortPtrDW(block);

    mainzone->blocklist.user_spptr = getShortPtr((void*) mainzone);
    mainzone->blocklist.tag = PU_STATIC;
    mainzone->rover = block;

    block->prev_sptrdw = block->next_sptrdw = getShortPtrDW(&mainzone->blocklist);

    // free block
    block->tag = PU_FREE;

    block->ssize = (mainzone->size - sizeof(memzone_t)) >> 2;
}

extern inline unsigned short getShortPtr(void *longPtr);
static inline memblock_t* getMemblockPrev(memblock_t *mb)
{
    return (memblock_t*) getLongPtrDW(mb->prev_sptrdw);
}
static inline void** getMemblockUser(memblock_t *mb)
{
    return (void**) getLongPtr(mb->user_spptr);
}
static inline memblock_t* getMemblockNext(memblock_t *mb)
{
    return (memblock_t*) getLongPtrDW(mb->next_sptrdw);
}
//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#define MINFRAGMENT     32

void* Z_Malloc2(int size, int tag, void **user, const char *sz)
{
    int extra;
    memblock_t *start;
    memblock_t *rover;
    memblock_t *newblock;
    memblock_t *base;
    void *result;

    if (!size)
        return user ? *user = NULL : NULL;           // malloc(0) returns NULL

    size = (size + MEM_ALIGN - 1) & ~(MEM_ALIGN - 1);

    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // account for size of block header
    size += sizeof(memblock_t);

    // if there is a free block behind the rover,
    //  back up over them
    base = mainzone->rover;

    if (getMemblockPrev(base)->tag == PU_FREE)
        base = getMemblockPrev(base);

    rover = base;
    start = getMemblockPrev(base);

    do
    {
        if (rover == start)
        {
            // scanned all the way around the list
            printf("Z_Malloc: failed on allocation of %i bytes", size);
            while (1);
        }
        if (rover->tag != PU_FREE)
        {
            if (rover->tag < PU_PURGELEVEL)
            {
                // hit a block that can't be purged,
                // so move base past it
                base = rover = getMemblockNext(rover);
            }
            else
            {
                // free the rover block (adding the size to base)

                // the rover can be the base block
                base = getMemblockPrev(base);
                Z_Free((byte*) rover + sizeof(memblock_t));
                base = getMemblockNext(base);
                rover = getMemblockNext(base);
            }
        }
        else
        {
            rover = getMemblockNext(rover);
        }

    } while (base->tag != PU_FREE || (base->ssize << 2) < size);

    // found a block big enough
    extra = (base->ssize << 2) - size;

    if (extra > MINFRAGMENT)
    {
        // there will be a free fragment after the allocated block
        newblock = (memblock_t*) ((byte*) base + size);
        newblock->ssize = extra >> 2;

        newblock->tag = PU_FREE;
        newblock->user_spptr = 0;
        newblock->prev_sptrdw = getShortPtrDW(base);
        newblock->next_sptrdw = base->next_sptrdw;
        getMemblockNext(newblock)->prev_sptrdw = getShortPtrDW(newblock);

        base->next_sptrdw = getShortPtrDW(newblock);
        base->ssize = size >> 2;
    }
    free_memory -= base->ssize << 2;
    // no need to write else base->ssize = base->ssize...
    //

    if (user == NULL && tag >= PU_PURGELEVEL)
    {
        printf("Z_Malloc: an owner is required for purgable blocks");
        while (1);
    }
    uint16_t userspptr = getShortPtr(user);

    base->user_spptr = userspptr;
    base->tag = tag;

    result = (void*) ((byte*) base + sizeof(memblock_t));

    if (user)                   // if there is a user
        *user = result;            // set user to point to new block

    // next allocation will start looking here
    mainzone->rover = getMemblockNext(base);

#ifdef ZMALLOC_STAT
    if (free_memory < largest_occupied)
        largest_occupied = free_memory;
    printf("Mall: occ. %d lrgst %d Addr %04x. BlkSz %d %s\r\n", -free_memory, -largest_occupied, getShortPtr(base), (base->ssize << 2), sz);
#endif

    return result;
}

void (Z_Free)(void *p)
{
    memblock_t *block = (memblock_t*) ((char*) p - HEADER_SIZE);

    if (!p)
        return;

    if (block->user_spptr && block->tag != PU_FREE) // Nullify user if one exists
    {
        *(getMemblockUser(block)) = NULL;
    }
    // free memory
    free_memory += (block->ssize << 2);

    // mark this block as free
    block->tag = PU_FREE;
    block->user_spptr = 0;

    //
    memblock_t *other = getMemblockPrev(block);
    //
    if (other->tag == PU_FREE)
    {
        // merge with previous free block
        other->ssize += block->ssize;
        other->next_sptrdw = block->next_sptrdw;
        getMemblockNext(other)->prev_sptrdw = getShortPtrDW(other);

        if (block == mainzone->rover)
            mainzone->rover = other;

        block = other;
    }

    other = getMemblockNext(block);
    if (other->tag == PU_FREE)
    {
        // merge the next free block onto the end
        block->ssize += other->ssize;
        block->next_sptrdw = other->next_sptrdw;
        getMemblockNext(block)->prev_sptrdw = getShortPtrDW(block);

        if (other == mainzone->rover)
            mainzone->rover = block;
    }
#ifdef ZMALLOC_STAT
    printf("Free: occ. %d lrgst %d Addr %04x. BlkSz %d\r\n", -free_memory, -largest_occupied, getShortPtr(block), (block->ssize << 2));
#endif
}

void Z_FreeTags(int lowtag, int hightag)
{
    memblock_t *block;
    memblock_t *next;

    for (block = getMemblockNext(&mainzone->blocklist);
            block != &mainzone->blocklist; block = next)
    {
        // get link before freeing
        next = getMemblockNext(block);

        // free block?
        if (block->tag == PU_FREE)
            continue;

        if (block->tag >= lowtag && block->tag <= hightag)
            Z_Free((byte*) block + sizeof(memblock_t));
    }
}

void* (Z_Realloc)(void *ptr, size_t n, int tag, void **user)
{
    void *p = Z_Malloc(n, tag, user DA(file, line));
    if (ptr)
    {
        memblock_t *block = (memblock_t*) ((char*) ptr - HEADER_SIZE);
        memcpy(p, ptr, n <= (block->ssize << 2) ? n : (block->ssize << 2));
        (Z_Free)(ptr DA(file, line));
        if (user) // in case Z_Free nullified same user
            *user = p;
    }
    return p;
}

void* (Z_Calloc)(size_t n1, size_t n2, int tag, void **user)
{
    return (n1 *= n2) ? memset(Z_Malloc(n1, tag, user DA(file, line)), 0, n1) : NULL;
}

char* (Z_Strdup)(const char *s, int tag, void **user)
{
    return strcpy(Z_Malloc(strlen(s) + 1, tag, user DA(file, line)), s);
}
