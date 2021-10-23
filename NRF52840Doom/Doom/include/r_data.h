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
 *  Refresh module, data I/O, caching, retrieval of graphics
 *  by name.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"
#include "r_patch.h"

#ifdef __GNUG__
#pragma interface
#endif

// A single patch from a texture definition, basically
// a rectangular area within the texture rectangle.
typedef struct
{
    short originx, originy;  // Block origin, which has already accounted
    const patch_t *patch;    // for the internal origin of the patch.
} texpatch_t;

//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef struct
{
    // 2021/03/06: next-hack removed name, useless
    //const char*  name;         // Keep name for switch changing, etc.
    //int   next, index;     // killough 1/31/98: used in hashing algorithm
    // CPhipps - moved arrays with per-texture entries to elements here
    unsigned short widthmask;
    // CPhipps - end of additions
    short width, height;

    unsigned char overlapped;
    unsigned char patchcount;      // All the patches[patchcount] are drawn
    texpatch_t patches[1]; // back-to-front into the cached texture.
} texture_t;

// I/O, setting up the stuff.
void R_InitData(void);

// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
int R_FlatNumForName(const char *name);   // killough -- const added

// R_*TextureNumForName returns the texture number for the texture name, or NO_TEXTURE if 
//  there is no texture (i.e. "-") specified.
/* cph 2006/07/23 - defined value for no-texture marker (texture "-" in the WAD file) */
#define NO_TEXTURE 0
int R_CheckTextureNumForName(const char *name);

const texture_t* R_GetTexture(int texture, boolean cacheToFlash, uint32_t *spaceRequired);
int R_LoadTextureByName(const char *tex_name, boolean cacheToFlash);
uint32_t getTextureStructSize(int texture_num);
void R_FillTextureHeightData();

#endif
