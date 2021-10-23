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
 * next-hack: originally doomhack, in his GBA port, put (almost) all the global
 * variables in a global structure, which was by the way Z_malloc'ed.
 * Here the structure is not z_malloc'ed but it is static.
 *
 *-----------------------------------------------------------------------------
 */
#include <stdlib.h>
#include "global_data.h"

globals_t global;
globals_t *_g;

void InitGlobals()
{
    _g = &global;
    memset(_g, 0, sizeof(globals_t));

    //******************************************************************************
    //am_map.c
    //******************************************************************************

    _g->scale_mtof = (fixed_t) INITSCALEMTOF;
    _g->stopped = true;

    _g->lastlevel = -1;
    _g->lastepisode = -1;

    _g->mtof_zoommul = FRACUNIT; // how far the window zooms each tic (map coords)
    _g->ftom_zoommul = FRACUNIT; // how far the window zooms each tic (fb coords)

    //******************************************************************************
    //d_main.c
    //******************************************************************************

    _g->wipegamestate = GS_DEMOSCREEN;
    _g->oldgamestate = -1;

    //******************************************************************************
    //doomstat.c
    //******************************************************************************

    // Game Mode - identify IWAD as shareware, retail etc.
    _g->gamemode = indetermined;
    _g->gamemission = doom;

    //******************************************************************************
    //m_menu.c
    //******************************************************************************

    _g->showMessages = 1;    // Show messages has default, 0 = off, 1 = on

    //******************************************************************************
    //p_setup.c
    //******************************************************************************

//    _g->rejectlump = -1;// cph - store reject lump num if cached

    //******************************************************************************
    //r_main.c
    //******************************************************************************

    _g->validcount = 1;         // increment every time a check is made

    _g->freehead = &_g->freetail;     // killough

    //******************************************************************************
    //s_sounds.c
    //******************************************************************************

    _g->snd_SfxVolume = 15;

    // Maximum volume of music. Useless so far.
    _g->snd_MusicVolume = 15;

    //******************************************************************************
    //st_stuff.c
    //******************************************************************************

    _g->st_oldhealth = -1;
    //
    _g->tex_lookup_valid = false;
}
