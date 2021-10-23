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
 *   All the global variables that store the internal state.
 *   Theoretically speaking, the internal state of the engine
 *    should be found by looking at the variables collected
 *    here, and every relevant module will have to include
 *    this header file.
 *   In practice, things are a bit messy.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __D_STATE__
#define __D_STATE__

// We need the playr data structure as well.

#include "d_player.h"

#ifdef __GNUG__
#pragma interface
#endif

// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern const skill_t startskill;
extern const int startepisode;
extern const int startmap;

// -------------------------
// Status flags for refresh.
//

enum automapmode_e
{
    am_active = 1,  // currently shown
    am_overlay = 2,  // covers the screen, i.e. not overlay mode
    am_rotate = 4,  // rotates to the player facing direction
    am_follow = 8,  // keep the player centred
    am_grid = 16,  // show grid
};
extern enum automapmode_e automapmode; // Mode that the automap is in

//-----------------------------------------
// Internal parameters, used for engine.
//

//-----------------------------------------------------------------------------

#endif
