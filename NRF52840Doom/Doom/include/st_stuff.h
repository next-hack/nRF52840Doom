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
 *      Status bar code.
 *      Does the face/direction indicator animatin.
 *      Does palette indicators as well (red pain/berserk, bright pickup)
 *
 * next-hack:
 *      pointers to patches now stored in immutable flash region, saving a few
 *      hundred bytes of RAM.
 *      modified so that instead of re-rendering, if no
 *      changes are required, and the status bar is already rendered in
 *      the previous frame buffer (but not on the current one), then the 
 *      previous frame is copied to the current frame buffer.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#include "doomtype.h"
#include "d_event.h"

// Size of statusbar.
// Now sensitive for scaling.

// proff 08/18/98: Changed for high-res
#define ST_HEIGHT 32
#define ST_WIDTH  320
#define ST_Y      (SCREENHEIGHT - ST_HEIGHT)
#define ST_SCALED_HEIGHT ST_HEIGHT
#define ST_SCALED_WIDTH  SCREENWIDTH
#define ST_SCALED_Y      (SCREENHEIGHT - ST_SCALED_HEIGHT)
// nh: reduction constant from GBA port to ours
#define ST_C 160/160
//
// STATUS BAR DATA
//

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS            1
#define STARTBONUSPALS          9
#define NUMREDPALS              8
#define NUMBONUSPALS            4
// Radiation suit, green shift.
#define RADIATIONPAL            13

// Location of status bar
#define ST_X                    0
#define ST_X2                   104

// proff 08/18/98: Changed for high-res
#define ST_FX                   (ST_X+143 * ST_C)
#define ST_FY                   (ST_Y+1)

// Should be set to patch width
//  for tall numbers later on
#define ST_TALLNUMWIDTH         (tallnum[0]->width)

// Number of status faces.
#define ST_NUMPAINFACES         5
#define ST_NUMSTRAIGHTFACES     3
#define ST_NUMTURNFACES         2
#define ST_NUMSPECIALFACES      3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES        2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET           (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET           (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET       (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET        (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE              (ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE             (ST_GODFACE+1)

// proff 08/18/98: Changed for high-res
#define ST_FACESX               (ST_X+104 * ST_C)
#define ST_FACESY               (ST_Y)

#define ST_EVILGRINCOUNT        (2*TICRATE)
//Fix Status bar face timing ~Kippykip
//#define ST_STRAIGHTFACECOUNT    (TICRATE*2)
#define ST_STRAIGHTFACECOUNT    (18)
#define ST_TURNCOUNT            (1*TICRATE)
#define ST_OUCHCOUNT            (1*TICRATE)
#define ST_RAMPAGEDELAY         (2*TICRATE)

#define ST_MUCHPAIN             20

// Location and size of statistics,
//  justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?
// I dunno, why don't you go and find out!!!  killough
#if 1
// AMMO number pos.
#define ST_AMMOWIDTH            3
// proff 08/18/98: Changed for high-res
#define ST_AMMOX                (ST_X+32 * ST_C)
#define ST_AMMOY                (ST_Y+6)
//#define ST_AMMOX                44
//#define ST_AMMOY                171

// HEALTH number pos.
#define ST_HEALTHWIDTH          2
// proff 08/18/98: Changed for high-res
#define ST_HEALTHX              (ST_X+66 * ST_C)
#define ST_HEALTHY              (ST_Y+6)

// Weapon pos.
// proff 08/18/98: Changed for high-res
#define ST_ARMSX                (ST_X+82 * ST_C)
#define ST_ARMSY                (ST_Y+4)
#define ST_ARMSBGX              (ST_X+104 * ST_C)
#define ST_ARMSBGY              (ST_Y)

#define ST_ARMSXSPACE           8
#define ST_ARMSYSPACE           10

// Frags pos.
// proff 08/18/98: Changed for high-res
#define ST_FRAGSX               (ST_X+138 * ST_C)
#define ST_FRAGSY               (ST_Y+3)
//#define ST_FRAGSX               138
//#define ST_FRAGSY               171
#define ST_FRAGSWIDTH           2

// ARMOR number pos.
#define ST_ARMORWIDTH           3
// proff 08/18/98: Changed for high-res
#define ST_ARMORX               (ST_X+166 * ST_C)
#define ST_ARMORY               (ST_Y+6)

// Key icon positions.
#define ST_KEY0WIDTH            8
#define ST_KEY0HEIGHT           5
// proff 08/18/98: Changed for high-res
#define ST_KEY0X                (ST_X+178 * ST_C)
#define ST_KEY0Y                (ST_Y+3)

#define ST_KEY1WIDTH            ST_KEY0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_KEY1X                (ST_X+178 * ST_C)
#define ST_KEY1Y                (ST_Y+13)

#define ST_KEY2WIDTH            ST_KEY0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_KEY2X                (ST_X+178 * ST_C)
#define ST_KEY2Y                (ST_Y+23)

// Ammunition counter.
#define ST_AMMO0WIDTH           3
#define ST_AMMO0HEIGHT          6
// proff 08/18/98: Changed for high-res
#define ST_AMMO0X               (ST_X+220 * ST_C)
#define ST_AMMO0Y               (ST_Y+5)

#define ST_AMMO1WIDTH           ST_AMMO0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_AMMO1X               (ST_X+220 * ST_C)
#define ST_AMMO1Y               (ST_Y+11)

#define ST_AMMO2WIDTH           ST_AMMO0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_AMMO2X               (ST_X+220 * ST_C)
#define ST_AMMO2Y               (ST_Y+23)

#define ST_AMMO3WIDTH           ST_AMMO0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_AMMO3X               (ST_X+220 * ST_C)
#define ST_AMMO3Y               (ST_Y+17)

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH        3
#define ST_MAXAMMO0HEIGHT       5
// proff 08/18/98: Changed for high-res
#define ST_MAXAMMO0X            (ST_X+238 * ST_C)
#define ST_MAXAMMO0Y            (ST_Y+5)

#define ST_MAXAMMO1WIDTH        ST_MAXAMMO0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_MAXAMMO1X            (ST_X+238 * ST_C)
#define ST_MAXAMMO1Y            (ST_Y+11)

#define ST_MAXAMMO2WIDTH        ST_MAXAMMO0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_MAXAMMO2X            (ST_X+238 * ST_C)
#define ST_MAXAMMO2Y            (ST_Y+23)

#define ST_MAXAMMO3WIDTH        ST_MAXAMMO0WIDTH
// proff 08/18/98: Changed for high-res
#define ST_MAXAMMO3X            (ST_X+238 * ST_C)
#define ST_MAXAMMO3Y            (ST_Y+17)

#else
#define ST_YC 1
// AMMO number pos.
#define ST_AMMOWIDTH    3
#define ST_AMMOX      44
#define ST_AMMOY      171 * ST_YC

// HEALTH number pos.
#define ST_HEALTHWIDTH    3
#define ST_HEALTHX      90
#define ST_HEALTHY      171  * ST_YC

// Weapon pos.
#define ST_ARMSX      111
#define ST_ARMSY      172  * ST_YC
#define ST_ARMSBGX      104
#define ST_ARMSBGY      168  * ST_YC
#define ST_ARMSXSPACE   12
#define ST_ARMSYSPACE   10

// Frags pos.
#define ST_FRAGSX     138
#define ST_FRAGSY     171  * ST_YC
#define ST_FRAGSWIDTH   2

// ARMOR number pos.
#define ST_ARMORWIDTH   3
#define ST_ARMORX     221
#define ST_ARMORY     171  * ST_YC

// Key icon positions.
#define ST_KEY0WIDTH    8
#define ST_KEY0HEIGHT   5
#define ST_KEY0X      239
#define ST_KEY0Y      171  * ST_YC
#define ST_KEY1WIDTH    ST_KEY0WIDTH
#define ST_KEY1X      239
#define ST_KEY1Y      181  * ST_YC
#define ST_KEY2WIDTH    ST_KEY0WIDTH
#define ST_KEY2X      239
#define ST_KEY2Y      191  * ST_YC

// Ammunition counter.
#define ST_AMMO0WIDTH   3
#define ST_AMMO0HEIGHT    6
#define ST_AMMO0X     288
#define ST_AMMO0Y     173  * ST_YC
#define ST_AMMO1WIDTH   ST_AMMO0WIDTH
#define ST_AMMO1X     288
#define ST_AMMO1Y     179  * ST_YC
#define ST_AMMO2WIDTH   ST_AMMO0WIDTH
#define ST_AMMO2X     288
#define ST_AMMO2Y     191  * ST_YC
#define ST_AMMO3WIDTH   ST_AMMO0WIDTH
#define ST_AMMO3X     288
#define ST_AMMO3Y     185  * ST_YC

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH    3
#define ST_MAXAMMO0HEIGHT   5
#define ST_MAXAMMO0X    314
#define ST_MAXAMMO0Y    173  * ST_YC
#define ST_MAXAMMO1WIDTH    ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X    314
#define ST_MAXAMMO1Y    179  * ST_YC
#define ST_MAXAMMO2WIDTH    ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X    314
#define ST_MAXAMMO2Y    191  * ST_YC
#define ST_MAXAMMO3WIDTH    ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X    314
#define ST_MAXAMMO3Y    185  * ST_YC

// pistol
#define ST_WEAPON0X     110
#define ST_WEAPON0Y     172  * ST_YC

// shotgun
#define ST_WEAPON1X     122
#define ST_WEAPON1Y     172  * ST_YC

// chain gun
#define ST_WEAPON2X     134
#define ST_WEAPON2Y     172  * ST_YC

// missile launcher
#define ST_WEAPON3X     110
#define ST_WEAPON3Y     181  * ST_YC

// plasma gun
#define ST_WEAPON4X     122
#define ST_WEAPON4Y     181  * ST_YC

 // bfg
#define ST_WEAPON5X     134
#define ST_WEAPON5Y     181  * ST_YC

// WPNS title
#define ST_WPNSX      109
#define ST_WPNSY      191  * ST_YC

 // DETH title
#define ST_DETHX      109
#define ST_DETHY      191  * ST_YC
#endif

//
// STATUS BAR
//

// Called by main loop.
boolean ST_Responder(const event_t *ev);

// Called by main loop.
void ST_Ticker(void);

// Called by main loop.
void ST_Drawer(boolean st_statusbaron, boolean refresh);

// Called when the console player is spawned on each level.
void ST_Start(void);

// Called by startup code.
void ST_Init(void);

// States for status bar code.
typedef enum
{
    AutomapState, FirstPersonState
} st_stateenum_t;

// killough 5/2/98: moved from m_misc.c:

#endif
