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
 * This is designed to be a fast allocator for small, regularly used block sizes
 *
 * next-hack: modified to have low memory-overhead. Also removed block size
 * (useless, as the zone already has the block size in its structure) and used
 * bitwise flags to assess if a block is allocated)
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomtype.h"
#include "z_zone.h"
#include "z_bmalloc.h"
#include "lprintf.h"

typedef struct bmalpool_s
{
    struct bmalpool_s *nextpool;
    byte used[0];
} bmalpool_t;

#define numberOfBlocksToNumberOfBytes(n) (4 * ((n + 31)/32))
__inline static void* getelem(bmalpool_t *p, size_t size, size_t n, struct block_memory_alloc_s *pzone)
{
    return (((byte*) p) + sizeof(bmalpool_t) + sizeof(byte) * numberOfBlocksToNumberOfBytes(pzone->perpool) + size * n);

}

__inline static PUREFUNC int iselem(const bmalpool_t *pool, size_t size, const void *p, struct block_memory_alloc_s *pzone)
{
    // CPhipps - need portable # of bytes between pointers
    int dif = (const char*) p - (const char*) pool;

    dif -= sizeof(bmalpool_t);
    dif -= sizeof(byte) * numberOfBlocksToNumberOfBytes(pzone->perpool);
    if (dif < 0)
        return -1;
    dif /= size;
    return (((size_t) dif >= pzone->perpool) ? -1 : dif);

}

enum
{
    unused_block = 0, used_block = 1
};
void setBlockUsageStatus(byte *usedArray, int n, int state)
{
    if (state == unused_block)
    {
        usedArray[n / 8] &= ~(1 << (n % 8));

    }
    else
    {
        usedArray[n / 8] |= (1 << (n % 8));
    }
}
int checkForBlockUsageStatus(byte *usedArray, int state, int number_to_check)
{
    for (int i = 0; i < number_to_check; i++)
    {
        int bit = usedArray[i / 8] & (1 << (i % 8));
        if ((state && bit) || (state == 0 && bit == 0))
        {
            return i;
        }
    }
    // not found
    return -1;
}
void* Z_BMalloc(struct block_memory_alloc_s *pzone)
{
    register bmalpool_t **pool = (bmalpool_t**) &(pzone->firstpool);
    while (*pool != NULL)
    {
        int n = checkForBlockUsageStatus((*pool)->used, unused_block, pzone->perpool); // Scan for unused marker
        if (n != -1)
        {
            setBlockUsageStatus((*pool)->used, n, used_block);
            return getelem(*pool, pzone->size, n, pzone);
        }
        else
            pool = &((*pool)->nextpool);
    }
    {
        // Nothing available, must allocate a new pool
        bmalpool_t *newpool;

        // CPhipps: Allocate new memory, initialised to 0
        *pool = newpool = Z_Calloc(sizeof(*newpool) + (pzone->size) * (pzone->perpool) + numberOfBlocksToNumberOfBytes(pzone->perpool), 1, pzone->tag, NULL);
        newpool->nextpool = NULL;

        // Return element 0 from this pool to satisfy the request
        setBlockUsageStatus(newpool->used, 0, used_block);
        return getelem(newpool, pzone->size, 0, pzone);
    }
}

boolean Z_BFree(struct block_memory_alloc_s *pzone, void *p)
{
    register bmalpool_t **pool = (bmalpool_t**) &(pzone->firstpool);

    while (*pool != NULL)
    {
        int n = iselem(*pool, pzone->size, p, pzone);
        if (n >= 0)
        {
            setBlockUsageStatus((*pool)->used, n, unused_block);
            if (-1 == checkForBlockUsageStatus(((*pool)->used), used_block, pzone->perpool))
            {
                // Block is all unused, can be freed
                bmalpool_t *oldpool = *pool;
                *pool = (*pool)->nextpool;
                Z_Free(oldpool);
            }
            return true;
        }
        else
            pool = &((*pool)->nextpool);
    }
    return 0;
}
