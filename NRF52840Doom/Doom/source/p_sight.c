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
 *      LineOfSight/Visibility checks, uses REJECT Lookup Table.
 *
 *       next-hack: modified code to work with short pointers.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "r_main.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "m_bbox.h"
#include "lprintf.h"

#include "global_data.h"

//
// P_CheckSight
// Returns true
//  if a straight line between t1 and t2 is unobstructed.
// Uses REJECT.
//
// killough 4/20/98: cleaned up, made to use new LOS struct
los_t los; // cph - made static

boolean P_CrossBSPNode(int bspnum);

boolean P_CheckSight(mobj_t *t1, mobj_t *t2)
{
    const sector_t *s1 = getMobjSubsector(t1)->sector;
    const sector_t *s2 = getMobjSubsector(t2)->sector;
    int pnum = (s1 - _g->sectors) * _g->numsectors + (s2 - _g->sectors);

    // First check for trivial rejection.
    // Determine subsector entries in REJECT table.
    //
    // Check in REJECT table.

    if (_g->rejectmatrix[pnum >> 3] & (1 << (pnum & 7))) // can't possibly be connected
    {
        return false;
    }
    /* killough 11/98: shortcut for melee situations
     * same subsector? obviously visible
     * cph - compatibility optioned for demo sync, cf HR06-UV.LMP */
    if (t1->subsector_sptr == t2->subsector_sptr)
        return true;

    // An unobstructed LOS is possible.
    // Now look from eyes of t1 to any part of t2.

    _g->validcount++;

    los.topslope = (los.bottomslope = t2->z - (los.sightzstart = t1->z + getMobjHeight(t1) - (getMobjHeight(t1) >> 2))) + getMobjHeight(t2);
    los.strace.dx = (los.t2x = t2->x) - (los.strace.x = t1->x);
    los.strace.dy = (los.t2y = t2->y) - (los.strace.y = t1->y);

    if (t1->x > t2->x)
        los.bbox[BOXRIGHT] = t1->x, los.bbox[BOXLEFT] = t2->x;
    else
        los.bbox[BOXRIGHT] = t2->x, los.bbox[BOXLEFT] = t1->x;

    if (t1->y > t2->y)
        los.bbox[BOXTOP] = t1->y, los.bbox[BOXBOTTOM] = t2->y;
    else
    {
        los.bbox[BOXTOP] = t2->y, los.bbox[BOXBOTTOM] = t1->y;
    }
    los.maxz = INT_MAX;
    los.minz = INT_MIN;

    // the head node is the last node output
    return P_CrossBSPNode(numnodes - 1);
}
