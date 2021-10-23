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
 *  Refresh of things, i.e. objects represented by sprites.
 *
 * next-hack:
 * - init sprite def now uses stack, instead of a global variable,
 * saving few hundred bytes of RAM.
 * - init sprite calculate the sprites arrays and save it to immutable flash
 * region, saving several kB of RAM
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_segs.h"
#include "r_draw.h"
#include "r_things.h"
#include "v_video.h"
#include "lprintf.h"
#include "z_zone.h"

#include "global_data.h"
#include "i_spi_support.h"

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//

//
// INITIALIZATION FUNCTIONS
//

//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//

static void R_InstallSpriteLump(int lump, unsigned frame, unsigned rotation, boolean flipped, spriteframe_t *sprtemp) // 2021/03/20 next-hack: the sprtemp is declared on the calling function, no need to use a global static var
{
    if (frame >= MAX_SPRITE_FRAMES || rotation > 8)
        I_Error("R_InstallSpriteLump: Bad frame characters in lump %i", lump);

    if ((int) frame > _g->maxframe)
        _g->maxframe = frame;

    if (rotation == 0)
    {    // the lump should be used for all rotations
        int r;

        sprtemp[frame].flipmask = 0;

        for (r = 0; r < 8; r++)
        {
            if (sprtemp[frame].lump[r] == -1)
            {
                sprtemp[frame].lump[r] = lump - _g->firstspritelump;

                if (flipped)
                    sprtemp[frame].flipmask |= (1 << r);

                sprtemp[frame].rotate = false; //jff 4/24/98 if any subbed, rotless
            }
        }
        return;
    }

    // the lump is only used for one rotation

    if (sprtemp[frame].lump[--rotation] == -1)
    {
        sprtemp[frame].lump[rotation] = lump - _g->firstspritelump;

        if (flipped)
            sprtemp[frame].flipmask |= (1 << rotation);
        else
            sprtemp[frame].flipmask &= (~(1 << rotation));

        sprtemp[frame].rotate = true; //jff 4/24/98 only change if rot used
    }
}

//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
// (4 chars exactly) to be used.
//
// Builds the sprite rotation matrixes to account
// for horizontally flipped sprites.
//
// Will report an error if the lumps are inconsistent.
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
//
// A sprite that is flippable will have an additional
//  letter/number appended.
//
// The rotation character can be 0 to signify no rotations.
//
// 1/25/98, 1/31/98 killough : Rewritten for performance
//
// Empirically verified to have excellent hash
// properties across standard Doom sprites:

#define R_SpriteNameHash(s) ((unsigned)((s)[0]-((s)[1]*3-(s)[3]*2-(s)[2])*2))

static void R_InitSpriteDefs(const char *const*namelist)
{
    // 2021/03/20 next-hack. Instead of statically using _g->sprtemp[], that we only use here, we use the stack
    spriteframe_t sprtemp[MAX_SPRITE_FRAMES];
    size_t numentries = _g->lastspritelump - _g->firstspritelump + 1;
    struct
    {
        int index, next;
    } *hash;
    int i, numsprites;

    if (!numentries || !*namelist)
        return;

    // count the number of sprite names
    for (i = 0; namelist[i]; i++);
    //
    numsprites = i;
    //
    p_wad_immutable_flash_data->numsprites = i;
    //
    spritedef_t *sprites = Z_Malloc(numsprites * sizeof(*sprites), PU_STATIC, NULL); // sprite_data_ptr;

    memset(sprites, 0, numsprites * sizeof(*sprites));

    // Create hash table based on just the first four letters of each sprite
    // killough 1/31/98

    hash = malloc(sizeof(*hash) * numentries); // allocate hash table

    for (i = 0; (size_t) i < numentries; i++)  // initialize hash table as empty
        hash[i].index = -1;

    for (i = 0; (size_t) i < numentries; i++) // Prepend each sprite to hash chain
    {                                      // prepend so that later ones win
        char sn[8];
        spiFlashSetAddress((uint32_t) W_GetNameForNum(i + _g->firstspritelump));
        spiFlashGetData(sn, sizeof(sn));
        int j = R_SpriteNameHash(sn) % numentries;
        hash[i].next = hash[j].index;
        hash[j].index = i;
    }

    // scan all the lump names for each of the names,
    //  noting the highest frame letter.

    for (i = 0; i < numsprites; i++)
    {
        const char *spritename = namelist[i];
        int j = hash[R_SpriteNameHash(spritename) % numentries].index;

        if (j >= 0)
        {
            memset(sprtemp, -1, sizeof(sprtemp));
            _g->maxframe = -1;
            do
            {
                char sn[8];
                spiFlashSetAddress((uint32_t) W_GetNameForNum(j + _g->firstspritelump));
                spiFlashGetData(sn, sizeof(sn));
                // Fast portable comparison -- killough
                // (using int pointer cast is nonportable):

                if (!((sn[0] ^ spritename[0]) | (sn[1] ^ spritename[1]) | (sn[2] ^ spritename[2]) | (sn[3] ^ spritename[3])))
                {
                    R_InstallSpriteLump(j + _g->firstspritelump, sn[4] - 'A', sn[5] - '0',
                    false, sprtemp);
                    if (sn[6])
                        R_InstallSpriteLump(j + _g->firstspritelump, sn[6] - 'A', sn[7] - '0',
                        true, sprtemp);
                }
            } while ((j = hash[j].next) >= 0);

            // check the frames that were found for completeness
            if ((sprites[i].numframes = ++_g->maxframe))  // killough 1/31/98
            {
                int frame;
                for (frame = 0; frame < _g->maxframe; frame++)
                {
                    switch ((int) sprtemp[frame].rotate)
                    {
                        case -1:
                            // no rotations were found for that frame at all
                            I_Error("R_InitSprites: No patches found "
                                    "for %.8s frame %c", namelist[i], frame + 'A');
                            break;

                        case 0:
                            // only the first rotation is needed
                            break;

                        case 1:
                            // must have all 8 frames
                        {
                            int rotation;
                            for (rotation = 0; rotation < 8; rotation++)
                                if (sprtemp[frame].lump[rotation] == -1)
                                    I_Error("R_InitSprites: Sprite %.8s frame %c "
                                            "is missing rotations", namelist[i], frame + 'A');
                            break;
                        }
                    }
                }

                sprites[i].spriteframes = writeBufferToFlashRegion(sprtemp, _g->maxframe * sizeof(spriteframe_t), FLASH_IMMUTABLE_REGION, true);
            }
        }
    }
    // copy array to flash
    p_wad_immutable_flash_data->sprites = writeBufferToFlashRegion(sprites, numsprites * sizeof(*sprites), FLASH_IMMUTABLE_REGION, true);
    Z_Free(sprites);
    printf("Saved  %d sprites\r\n", numsprites);
    free(hash);             // free hash table
}

//
// GAME FUNCTIONS
//

//
// R_InitSprites
// Called at program start.
//

void R_InitSprites(const char *const*namelist)
{
    R_InitSpriteDefs(namelist);
}

