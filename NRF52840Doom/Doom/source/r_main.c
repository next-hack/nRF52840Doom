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
 *
 * DESCRIPTION:
 *      Rendering main loop and setup functions,
 *       utility functions (BSP, geometry, trigonometry).
 *      See tables.c, too.
 *
 * next-hack: put here where tabulated values come from!
 *            restored zlight.
 *-----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "d_net.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_things.h"
#include "r_plane.h"
#include "r_draw.h"
#include "m_bbox.h"
#include "r_sky.h"
#include "v_video.h"
#include "lprintf.h"
#include "st_stuff.h"
#include "i_main.h"
#include "i_system.h"
#include "g_game.h"
#include "tables.h"
#include "global_data.h"

// Fineangles in the SCREENWIDTH wide window.
// uint8_t scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
// uint8_t zlight[LIGHTLEVELS][MAXLIGHTZ];
#define FIELDOFVIEW 2048
#define DISTMAP 2
void R_InitTextureMapping(void)
{
#if TABULATED_VALUES
#else
    int i;
    int x;
    int t;
    fixed_t focallength;
    const int viewwidth = SCREENWIDTH;
    const int centerx = viewwidth / 2;
    const int centerxfrac = centerx << FRACBITS;
    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength = FixedDiv(centerxfrac, finetangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);

    for (i = 0; i < FINEANGLES / 2; i++)
    {
        if (finetangent[i] > FRACUNIT * 2)
            t = -1;
        else if (finetangent[i] < -FRACUNIT * 2)
            t = viewwidth + 1;
        else
        {
            t = FixedMul(finetangent[i], focallength);
            t = (centerxfrac - t + FRACUNIT - 1) >> FRACBITS;

            if (t < -1)
                t = -1;
            else if (t > viewwidth + 1)
                t = viewwidth + 1;
        }
        viewangletox[i] = t;
    }

    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.
    for (x = 0; x <= viewwidth; x++)
    {
        i = 0;
        while (viewangletox[i] > x)
            i++;
        xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i = 0; i < FINEANGLES / 2; i++)
    {
        t = FixedMul(finetangent[i], focallength);
        t = centerx - t;

        if (viewangletox[i] == -1)
            viewangletox[i] = 0;
        else if (viewangletox[i] == viewwidth + 1)
            viewangletox[i] = viewwidth;
    }
    printf("clip angle %d", xtoviewangle[0]);
    // clipangle = xtoviewangle[0];
    //
    const int detailshift = 0;
    // planes
    fixed_t dy;
    for (i = 0; i < viewheight; i++)
    {
        dy = ((i - viewheight / 2) << FRACBITS) + FRACUNIT / 2;
        dy = abs(dy);
        yslope[i] = FixedDiv(projectiony, dy);
    }
    //
    fixed_t cosadj;
    for (i = 0; i < viewwidth; i++)
    {
        cosadj = abs(finecosine[xtoviewangle[i] >> ANGLETOFINESHIFT]);
        distscale[i] = FixedDiv(FRACUNIT, cosadj);
    }
    // Calculate the light levels to use
    //  for each level / scale combination.

    int i;
    int j;
    int level;
    int startmap;
    int scale;

    for (i = 0; i < LIGHTLEVELS; i++)
    {
        startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTSCALE; j++)
        {
            level = startmap - j / DISTMAP;

            if (level < 0)
            {
                level = 0;
            }

            if (level >= NUMCOLORMAPS)
            {
                level = NUMCOLORMAPS - 1;
            }

            scalelight[i][j] = level; // colormaps + level*256;
        }
    }
    // zlight
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTZ; j++)
        {
            scale = FixedDiv((SCREENWIDTH / 2 * FRACUNIT), (j + 1) << LIGHTZSHIFT);
            scale >>= LIGHTSCALESHIFT;
            level = startmap - scale / DISTMAP;

            if (level < 0)
            {
                level = 0;
            }
            if (level >= NUMCOLORMAPS)
            {
                level = NUMCOLORMAPS - 1;
            }
            zlight[i][j] = level; // colormaps + level*256;
        }
    }
#endif
}

//
// R_Init
//

void R_Init(void)
{
    // trig tables in rom
    //  lprintf(LO_INFO, "R_LoadTrigTables");
    //  R_LoadTrigTables();
    // Data are tabulated. This call is kept to show where tabulated values come from
    R_InitTextureMapping();
    //
    lprintf(LO_INFO, "R_InitData");
    R_InitData();
    lprintf(LO_INFO, "R_InitBuffer");
    R_InitBuffer();
}

//
// R_SetupFrame
//

void R_SetupFrame(player_t * player)
{
    _g->viewplayer = player;

    viewx = player->mo->x;
    viewy = player->mo->y;
    viewz = player->viewz;
    viewangle = player->mo->angle;

    extralight = player->extralight;

    viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
    viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];

    fullcolormap = &p_wad_immutable_flash_data->colormaps[0];
    if (player->fixedcolormap)
    {
        fixedcolormap = fullcolormap // killough 3/20/98: use fullcolormap
                        + player->fixedcolormap * 256 * sizeof(lighttable_t);
    }
    else
        fixedcolormap = 0;

    _g->validcount++;
    //
}