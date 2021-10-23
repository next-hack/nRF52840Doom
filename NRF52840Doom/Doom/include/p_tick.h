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
 *  Core thinker processing prototypes.
 *-----------------------------------------------------------------------------*/

#ifndef __P_TICK__
#define __P_TICK__

#include "d_think.h"

#ifdef __GNUG__
#pragma interface
#endif

/* Called by C_Ticker, can call G_PlayerExited.
 * Carries out all thinking of monsters and players. */

void P_Ticker(void);

void P_InitThinkers(void);
void P_AddThinker(thinker_t *thinker);
void P_RemoveThinker(thinker_t *thinker);
void P_RemoveStaticThinker(thinker_t *thinker);
void P_RemoveThinkerDelayed(thinker_t *thinker);    // killough 4/25/98
void P_RemoveStaticThinkerDelayed(thinker_t *thinker); // 2021-03-13 next-hack
void P_UpdateThinker(thinker_t *thinker);   // killough 8/29/98

void P_RunThinkers(void);

void P_SetTarget(mobj_t **mo, mobj_t *target);   // killough 11/98

/* killough 8/29/98: threads of thinkers, for more efficient searches
 * cph 2002/01/13: for consistency with the main thinker list, keep objects
 * pending deletion on a class list too
 */
typedef enum
{
    th_all = 0, /* For P_NextThinker, indicates "any class" */
} th_class;

#define thinkercap _g->thinkerclasscap[th_all]

/* cph 2002/01/13 - iterator for thinker lists */
thinker_t* P_NextThinker(thinker_t*);

#endif
