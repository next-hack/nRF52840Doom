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
 *      Renderer main interface.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __R_MAIN__
#define __R_MAIN__

#include "d_player.h"
#include "r_data.h"

#ifdef __GNUG__
#pragma interface
#endif

//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

extern const int viewheight;

extern const int centery;

extern const fixed_t projection;
extern const fixed_t iprojection;

//Global vars.

extern short numnodes;
extern const mapnode_t *nodes;

extern fixed_t viewx, viewy, viewz;

extern angle_t viewangle;

//extern short *floorclip, *ceilingclip;

extern const lighttable_t *fullcolormap;
//extern const lighttable_t *colormaps;
extern const lighttable_t *fixedcolormap;

extern byte extralight;                          // bumped light from gun blasts

extern const texture_t **textures; // proff - 04/05/2000 removed static for OpenGL
extern fixed_t *textureheight; //needed for texture pegging (and TFE fix - killough)

extern short *flattranslation;             // for global animation
extern short *texturetranslation;

extern fixed_t basexscale, baseyscale;

extern fixed_t viewcos, viewsin;

//
// Utility functions.
//

angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
subsector_t* R_PointInSubsector(fixed_t x, fixed_t y);

//
// REFRESH - the actual rendering functions.
//

void R_RenderPlayerView(player_t *player);   // Called by G_Drawer.
void R_Init(void);                           // Called by startup code.
void R_SetupFrame(player_t *player);

#endif
