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
 *      WAD I/O functions.
 *      next-hack: important data internal flash caching functions.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __W_WAD__
#define __W_WAD__

#include "doomtype.h"
#include <stdint.h>
#include "st_stuff.h"
#include "r_defs.h"
#include "i_memory.h"
//
// TYPES
//

typedef struct
{
    char identification[4];                  // Should be "IWAD" or "PWAD".
    int numlumps;
    int infotableofs;
} wadinfo_t;

typedef struct
{
    int filepos;
    int size;
    char name[8];
} filelump_t;

// killough 4/17/98: if W_CheckNumForName() called with only
// one argument, pass ns_global as the default namespace
typedef struct
{
    uint32_t dataLength;
    union
    {
        struct
        {
            uint32_t map :16;
            uint32_t episode :16;
        };
        uint32_t mapEpisode;
    };
    void **lumpAddressTable;
    uint32_t numvertex;
    vertex_t *vertexes;
    uint32_t numsegs;
    seg_t *segs;
    int numnodes;
    mapnode_t *nodes;
    int numlines;
    line_t *lines;
    int numsides;
    short *blockmaplump;
    side_t *sides;
    byte *rejectmatrix;
    uint32_t data[0];       // textures, flats,
} wad_level_flash_data_t;
typedef struct
{
    uint32_t wadCRC;
    uint32_t wadSize;
    uint32_t immutableDataCRC;
    uint32_t immutableDataLength;  // including this structure
    wadinfo_t wadHeader;
    uint32_t numsprites;
    spritedef_t *sprites;
    uint8_t *palette_lump;
    lighttable_t *colormaps;
    uint32_t *soundLumps;
    uint32_t numtextures;
    fixed_t *textureheight;
    short *switchlist;
    patchsizeoffsets_t *patchLumpSizeOffsets;
    // 0-9, tall numbers
    const patch_t *tallnum[10];
    // 0-9, short, yellow (,different!) numbers
    const patch_t *shortnum[10];
    // tall % sign
    const patch_t *tallpercent;
    const patch_t *keys[NUMCARDS];
    // face status patches
    const patch_t *faces[ST_NUMFACES];      // >160 bytes wasted...
    // weapon ownership patches
    const patch_t *arms[6][2];
    wad_level_flash_data_t *levelData;
    // after this, immutable data:
    // playpal, and some TBD
    uint32_t data[0];
} wad_immutable_flash_data_t;
extern wad_immutable_flash_data_t *p_wad_immutable_flash_data;
extern wad_level_flash_data_t *p_wad_level_flash_data;
void W_Init(void); // CPhipps - uses the above array

int PUREFUNC W_CheckNumForName(const char *name);   // killough 4/17/98
int PUREFUNC W_GetNumForName(const char *name);
const char* PUREFUNC W_GetNameForNum(int lump);

int PUREFUNC W_LumpLength(int lump);

// CPhipps - modified for 'new' lump locking
const void* /*PUREFUNC*/W_CacheLumpNum(int lump);

// CPhipps - convenience macros
#define W_CacheLumpName(name) W_CacheLumpNum(W_GetNumForName(name))
/*static inline void *W_CacheLumpName(char * name)
 {
 printf("Caching name %s\r\n", name);
 return W_CacheLumpNum(W_GetNumForName(name));
 }*/
void ExtractFileBase(const char*, char*);       // killough
// next-hack additions
void initLumpPtrTable(void);
void* writeLumpToFlashRegion(int lumpnum, uint8_t flashRegion, boolean updateSize);
void* writeBufferToFlashRegion(void *buffer, int size, uint8_t flashRegion, boolean updateSize);
int getUserFlashRegionRemainingSpace();
void getFileLumpByNum(int n, filelump_t *fl);
void initImmutableFlashRegion();
void* getAddressOrCacheLumpNum(int lump, boolean storeInFlash, uint8_t flashRegion);
void** storeLumpArrayToFlash(boolean levelChanged);
void* storeFlashImmutableValues();
void* storeLevelValues(boolean levelChanged);
wad_level_flash_data_t* initLevelFlashRegion();
extern void **lumpPtrArray;
#endif
