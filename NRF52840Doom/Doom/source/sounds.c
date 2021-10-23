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
 *      Created by a sound utility.
 *      Kept as a sample, DOOM2 sounds.
 *
 *      next-hack: added to each sound the DS. Actually, now that sound lumps
 *      are pre-cached, we might restore old num and add "DS" at runtime during
 *      pre-caching, but whatever.
 *
 *-----------------------------------------------------------------------------*/

// killough 5/3/98: reformatted
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomtype.h"
#include "sounds.h"

//
// Information about all the sfx
//  next-hack: upper cased and added DS

const sfxinfo_t S_sfx[] =
{
    // S_sfx[0] needs to be a dummy for odd reasons.
    { "NONE", false, 0, 0, -1, 0 },
    { "DSPISTOL", false, 64, 0, -1, 18 },
    { "DSSHOTGN", false, 64, 0, -1, 30 },
    { "DSSGCOCK", false, 64, 0, -1, 19 },
    { "DSDSHTGN", false, 64, 0, -1, 28 },
    { "DSDBOPN", false, 64, 0, -1, 6 },
    { "DSDBCLS", false, 64, 0, -1, 7 },
    { "DSDBLOAD", false, 64, 0, -1, 8 },
    { "DSPLASMA", false, 64, 0, -1, 18 },
    { "DSBFG", false, 64, 0, -1, 58 },
    { "DSSAWUP", false, 64, 0, -1, 52 },
    { "DSSAWIDL", false, 118, 0, -1, 24 },
    { "DSSAWFUL", false, 64, 0, -1, 58 },
    { "DSSAWHIT", false, 64, 0, -1, 26 },
    { "DSRLAUNC", false, 64, 0, -1, 49 },
    { "DSRXPLOD", false, 70, 0, -1, 46 },
    { "DSFIRSHT", false, 70, 0, -1, 47 },
    { "DSFIRXPL", false, 70, 0, -1, 38 },
    { "DSPSTART", false, 100, 0, -1, 26 },
    { "DSPSTOP", false, 100, 0, -1, 21 },
    { "DSDOROPN", false, 100, 0, -1, 44 },
    { "DSDORCLS", false, 100, 0, -1, 45 },
    { "DSSTNMOV", false, 119, 0, -1, 10 },
    { "DSSWTCHN", false, 78, 0, -1, 20 },
    { "DSSWTCHX", false, 78, 0, -1, 18 },
    { "DSPLPAIN", false, 96, 0, -1, 48 },
    { "DSDMPAIN", false, 96, 0, -1, 31 },
    { "DSPOPAIN", false, 96, 0, -1, 28 },
    { "DSVIPAIN", false, 96, 0, -1, 34 },
    { "DSMNPAIN", false, 96, 0, -1, 37 },
    { "DSPEPAIN", false, 96, 0, -1, 26 },
    { "DSSLOP", false, 78, 0, -1, 36 },
    { "DSITEMUP", true, 78, 0, -1, 7 },
    { "DSWPNUP", true, 78, 0, -1, 19 },
    { "DSOOF", false, 96, 0, -1, 13 },
    { "DSTELEPT", false, 32, 0, -1, 49 },
    { "DSPOSIT1", true, 98, 0, -1, 17 },
    { "DSPOSIT2", true, 98, 0, -1, 36 },
    { "DSPOSIT3", true, 98, 0, -1, 35 },
    { "DSBGSIT1", true, 98, 0, -1, 44 },
    { "DSBGSIT2", true, 98, 0, -1, 52 },
    { "DSSGTSIT", true, 98, 0, -1, 36 },
    { "DSCACSIT", true, 98, 0, -1, 55 },
    { "DSBRSSIT", true, 94, 0, -1, 44 },
    { "DSCYBSIT", true, 92, 0, -1, 42 },
    { "DSSPISIT", true, 90, 0, -1, 40 },
    { "DSBSPSIT", true, 90, 0, -1, 35 },
    { "DSKNTSIT", true, 90, 0, -1, 44 },
    { "DSVILSIT", true, 90, 0, -1, 57 },
    { "DSMANSIT", true, 90, 0, -1, 42 },
    { "DSPESIT", true, 90, 0, -1, 39 },
    { "DSSKLATK", false, 70, 0, -1, 28 },
    { "DSSGTATK", false, 70, 0, -1, 30 },
    { "DSSKEPCH", false, 70, 0, -1, 10 },
    { "DSVILATK", false, 70, 0, -1, 54 },
    { "DSCLAW", false, 70, 0, -1, 21 },
    { "DSSKESWG", false, 70, 0, -1, 8 },
    { "DSPLDETH", false, 32, 0, -1, 35 },
    { "DSPDIEHI", false, 32, 0, -1, 35 },
    { "DSPODTH1", false, 70, 0, -1, 41 },
    { "DSPODTH2", false, 70, 0, -1, 30 },
    { "DSPODTH3", false, 70, 0, -1, 35 },
    { "DSBGDTH1", false, 70, 0, -1, 23 },
    { "DSBGDTH2", false, 70, 0, -1, 30 },
    { "DSSGTDTH", false, 70, 0, -1, 39 },
    { "DSCACDTH", false, 70, 0, -1, 33 },
    { "DSSKLDTH", false, 70, 0, -1, 13 },
    { "DSBRSDTH", false, 32, 0, -1, 35 },
    { "DSCYBDTH", false, 32, 0, -1, 53 },
    { "DSSPIDTH", false, 32, 0, -1, 108 },
    { "DSBSPDTH", false, 32, 0, -1, 58 },
    { "DSVILDTH", false, 32, 0, -1, 40 },
    { "DSKNTDTH", false, 32, 0, -1, 23 },
    { "DSPEDTH", false, 32, 0, -1, 53 },
    { "DSSKEDTH", false, 32, 0, -1, 47 },
    { "DSPOSACT", true, 120, 0, -1, 34 },
    { "DSBGACT", true, 120, 0, -1, 32 },
    { "DSDMACT", true, 120, 0, -1, 38 },
    { "DSBSPACT", true, 100, 0, -1, 42 },
    { "DSBSPWLK", true, 100, 0, -1, 17 },
    { "DSVILACT", true, 100, 0, -1, 42 },
    { "DSNOWAY", false, 78, 0, -1, 13 },
    { "DSBAREXP", false, 60, 0, -1, 59 },
    { "DSPUNCH", false, 64, 0, -1, 8 },
    { "DSHOOF", false, 70, 0, -1, 13 },
    { "DSMETAL", false, 70, 0, -1, 26 },
    { "DSCHGUN", false, 64, &S_sfx[sfx_pistol], 150, 18 },
    { "DSTINK", false, 60, 0, -1, 1 },
    { "DSBDOPN", false, 100, 0, -1, 14 },
    { "DSBDCLS", false, 100, 0, -1, 14 },
    { "DSITMBK", false, 100, 0, -1, 18 },
    { "DSFLAME", false, 32, 0, -1, 36 },
    { "DSFLAMST", false, 32, 0, -1, 18 },
    { "DSGETPOW", false, 60, 0, -1, 26 },
    { "DSBOSPIT", false, 70, 0, -1, 139 },
    { "DSBOSCUB", false, 70, 0, -1, 39 },
    { "DSBOSSIT", false, 70, 0, -1, 181 },
    { "DSBOSPN", false, 70, 0, -1, 137 },
    { "DSBOSDTH", false, 70, 0, -1, 132 },
    { "DSMANATK", false, 70, 0, -1, 38 },
    { "DSMANDTH", false, 70, 0, -1, 69 },
    { "DSSSSIT", false, 70, 0, -1, 23 },
    { "DSSSDTH", false, 70, 0, -1, 26 },
    { "DSKEENPN", false, 70, 0, -1, 14 },
    { "DSKEENDT", false, 70, 0, -1, 28 },
    { "DSSKEACT", false, 70, 0, -1, 34 },
    { "DSSKESIT", false, 70, 0, -1, 38 },
    { "DSSKEATK", false, 70, 0, -1, 47 },
    { "DSRADIO", false, 60, 0, -1, 8 }, 
};
