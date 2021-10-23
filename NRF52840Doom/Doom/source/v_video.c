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
 *  Gamma correction LUT stuff.
 *  Color range translation support
 *  Functions to draw patches (by post) directly to screen.
 *  Functions to blit a block to the screen.
 *
 *  next-hack:
 *  restored gamma tables.
 *  Patches can be directly load from external flash.
 *  Adapted to 160x128 pix screen.
 *  Added support of partial framebuffer (Drawing limits)
 *
 *-----------------------------------------------------------------------------
 */
#pragma GCC optimize ("Ofast")  // we need to compile this code to be as fast as possible.

#include "doomdef.h"
#include "r_main.h"
#include "r_draw.h"
#include "m_bbox.h"
#include "w_wad.h"   /* needed for color translation lump lookup */
#include "v_video.h"
#include "i_video.h"
#include "lprintf.h"

#include "global_data.h"

#include "i_spi_support.h"

// Now where did these came from?
const byte gammatable[5][256] =
{
{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 },

{ 2, 4, 5, 7, 8, 10, 11, 12, 14, 15, 16, 18, 19, 20, 21, 23, 24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 36, 37, 38, 39, 40, 41, 42, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 214, 215, 216, 217, 218, 219, 220, 221, 222, 222, 223, 224, 225, 226, 227, 228, 229, 230, 230, 231, 232, 233, 234, 235, 236, 237, 237, 238, 239, 240, 241, 242, 243, 244, 245, 245, 246, 247, 248, 249, 250, 251, 252, 252, 253, 254, 255 },

{ 4, 7, 9, 11, 13, 15, 17, 19, 21, 22, 24, 26, 27, 29, 30, 32, 33, 35, 36, 38, 39, 40, 42, 43, 45, 46, 47, 48, 50, 51, 52, 54, 55, 56, 57, 59, 60, 61, 62, 63, 65, 66, 67, 68, 69, 70, 72, 73, 74, 75, 76, 77, 78, 79, 80, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 153, 154, 155, 156, 157, 158, 159, 160, 160, 161, 162, 163, 164, 165, 166, 166, 167, 168, 169, 170, 171, 172, 172, 173, 174, 175, 176, 177, 178, 178, 179, 180, 181, 182, 183, 183, 184, 185, 186, 187, 188, 188, 189, 190, 191, 192, 193, 193, 194, 195, 196, 197, 197, 198, 199, 200, 201, 201, 202, 203, 204, 205, 206, 206, 207, 208, 209, 210, 210, 211, 212, 213, 213, 214, 215, 216, 217, 217, 218, 219, 220, 221, 221, 222, 223, 224, 224, 225, 226, 227, 228, 228, 229, 230, 231, 231, 232, 233, 234, 235, 235, 236, 237, 238, 238, 239, 240, 241, 241, 242, 243, 244, 244, 245, 246, 247, 247, 248, 249, 250, 251, 251, 252, 253, 254, 254, 255 },

{ 8, 12, 16, 19, 22, 24, 27, 29, 31, 34, 36, 38, 40, 41, 43, 45, 47, 49, 50, 52, 53, 55, 57, 58, 60, 61, 63, 64, 65, 67, 68, 70, 71, 72, 74, 75, 76, 77, 79, 80, 81, 82, 84, 85, 86, 87, 88, 90, 91, 92, 93, 94, 95, 96, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 135, 136, 137, 138, 139, 140, 141, 142, 143, 143, 144, 145, 146, 147, 148, 149, 150, 150, 151, 152, 153, 154, 155, 155, 156, 157, 158, 159, 160, 160, 161, 162, 163, 164, 165, 165, 166, 167, 168, 169, 169, 170, 171, 172, 173, 173, 174, 175, 176, 176, 177, 178, 179, 180, 180, 181, 182, 183, 183, 184, 185, 186, 186, 187, 188, 189, 189, 190, 191, 192, 192, 193, 194, 195, 195, 196, 197, 197, 198, 199, 200, 200, 201, 202, 202, 203, 204, 205, 205, 206, 207, 207, 208, 209, 210, 210, 211, 212, 212, 213, 214, 214, 215, 216, 216, 217, 218, 219, 219, 220, 221, 221, 222, 223, 223, 224, 225, 225, 226, 227, 227, 228, 229, 229, 230, 231, 231, 232, 233, 233, 234, 235, 235, 236, 237, 237, 238, 238, 239, 240, 240, 241, 242, 242, 243, 244, 244, 245, 246, 246, 247, 247, 248, 249, 249, 250, 251, 251, 252, 253, 253, 254, 254, 255 },

{ 16, 23, 28, 32, 36, 39, 42, 45, 48, 50, 53, 55, 57, 60, 62, 64, 66, 68, 69, 71, 73, 75, 76, 78, 80, 81, 83, 84, 86, 87, 89, 90, 92, 93, 94, 96, 97, 98, 100, 101, 102, 103, 105, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 128, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 143, 144, 145, 146, 147, 148, 149, 150, 150, 151, 152, 153, 154, 155, 155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 163, 164, 165, 166, 166, 167, 168, 169, 169, 170, 171, 172, 172, 173, 174, 175, 175, 176, 177, 177, 178, 179, 180, 180, 181, 182, 182, 183, 184, 184, 185, 186, 187, 187, 188, 189, 189, 190, 191, 191, 192, 193, 193, 194, 195, 195, 196, 196, 197, 198, 198, 199, 200, 200, 201, 202, 202, 203, 203, 204, 205, 205, 206, 207, 207, 208, 208, 209, 210, 210, 211, 211, 212, 213, 213, 214, 214, 215, 216, 216, 217, 217, 218, 219, 219, 220, 220, 221, 221, 222, 223, 223, 224, 224, 225, 225, 226, 227, 227, 228, 228, 229, 229, 230, 230, 231, 232, 232, 233, 233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 239, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 254, 254, 255, 255 } };

/*
 * V_DrawBackground tiles a 64x64 patch over the entire screen, providing the
 * background for the Help and Setup screens, and plot text betwen levels.
 * cphipps - used to have M_DrawBackground, but that was used the framebuffer
 * directly, so this is my code from the equivalent function in f_finale.c
 */
void V_DrawBackground(const char *flatname)
{
    /* erase the entire screen to a tiled background */
    const byte *src;
    int lump;

    uint8_t *dest = _g->screens[0].data;

    // killough 4/17/98:
    src = W_CacheLumpNum(lump = _g->firstflat + R_FlatNumForName(flatname));

    for (unsigned int y = draw_starty; y <= draw_stopy; y++)
    {
        byte srcdata[64];
        spiFlashSetAddress((uint32_t) src + ((y & 63) * 64));
        spiFlashGetData(srcdata, sizeof(srcdata));
        for (unsigned int x = 0; x < SCREENWIDTH_PHYSICAL; x += 64)
        {
            uint8_t *d = &dest[ ScreenYToOffset(y) + x];
            const byte *s = &srcdata[x & 63]; //&src[((y & 63) * 64) + (x & 63)];

            unsigned int len = 64;

            if ((SCREENWIDTH_PHYSICAL - x) < 64)
                len = SCREENWIDTH_PHYSICAL - x;

            memcpy(d, s, len);
        }
    }
}

inline static void PatchDrawColPix(byte *dest, const byte *source, unsigned int frac)
{
    *dest = source[frac >> FRACBITS];
}

inline static void PatchDrawColumn(byte *dest, const byte *source, unsigned int frac, fixed_t fracstep, unsigned int count)
{
    unsigned int l = (count >> 4);

    while (l--)
    {
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        //
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        //
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        //
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        PatchDrawColPix(dest, source, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        //
    }

    unsigned int r = (count & 15);

    switch (r)
    {
        case 15:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 14:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 13:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 12:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 11:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 10:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 9:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 8:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 7:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 6:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 5:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 4:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 3:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 2:
            PatchDrawColPix(dest, source, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 1:
            PatchDrawColPix(dest, source, frac);
    }

}

/*
 * This function draws at full resoulution (ie. not pixel doubled)
 * so the st bar and menus don't look like garbage.
 */

void V_DrawPatch(int x, int y, int scrn, const patch_t* patch)
{
    y -= patch->topoffset;
    x -= patch->leftoffset;

    int   col = 0;

    const int   DX  = (SCREENWIDTH<<16) / 320;
    const int   DXI = (320<<16) / SCREENWIDTH;
    const int   DY  = ((SCREENHEIGHT<<16)) / 200;
    const int   DYI = (200<<16) / SCREENHEIGHT;

    byte* byte_topleft = (byte*)_g->screens[scrn].data;
    const int byte_pitch = (SCREENPITCH);

    const int left = ( x * DX ) >> FRACBITS;
    const int right =  ((x + patch->width) *  DX) >> FRACBITS;
    const int bottom = ((y + patch->height) * DY) >> FRACBITS;


    for (int dc_x=left; dc_x<right; dc_x++, col+=DXI)
    {
        int colindex = (col>>16);

        if(dc_x < 0)
            continue;

        const column_t* column = (const column_t *)((const byte*)patch + (0xFFFFFF & patch->columnofs[colindex]));

        if (dc_x >= 240)
            break;

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            const byte* source = (const byte*)column + 3;
            const int topdelta = column->topdelta;
            int dc_yl = (((y + topdelta) * DY) >> FRACBITS);
            int dc_yh = (((y + topdelta + column->length) * DY) >> FRACBITS);

            if ((dc_yl >= SCREENHEIGHT) || (dc_yl > bottom))
            {
                break;
            }
          int count = (dc_yh - dc_yl);

            byte* dest = byte_topleft + (dc_yl*byte_pitch) + dc_x;

            const fixed_t fracstep = DYI;
            fixed_t frac = 0;

            // Inner loop that does the actual texture mapping,
            //  e.g. a DDA-lile scaling.
            // This is as fast as it gets.

                if (count > 0)
                {
                    PatchDrawColumn(dest, source, frac, fracstep, count);
                }

            column = (const column_t *)((const byte *)column + column->length + 4 );
        }
    }
}

// next-hack: this is used to convert GBA-specific graphics or coordinates
// (240 pix wide) to 160 pixel wide screens. TODO: instead of using this, we
// might want to use already resized graphics and/or already recalculated x
// values. This would improve both WAD size and drawing speed.
void V_DrawPatchXScale(int x, int y, const patch_t *patch)
{
    V_DrawPatchNoScale(x, y, patch);
    return;
    //
    if (false == isOnExternalFlash(patch))
    {
        x -= patch->leftoffset;
        y -= patch->topoffset;

        int col = 0;

        const int DX = (SCREENWIDTH_PHYSICAL << 16) / 240;
        const int DXI = (240 << 16) / SCREENWIDTH_PHYSICAL;

        byte *byte_topleft = (byte*) _g->screens[0].data;
        const int byte_pitch = SCREENWIDTH_PHYSICAL;

        const int left = (x * DX) >> FRACBITS;
        const int right = ((x + patch->width) * DX) >> FRACBITS;
        const int bottom = (y + patch->height);

        for (int dc_x = left; dc_x < right; dc_x++, col += DXI)
        {

            if (dc_x < 0)
                continue;
            if (dc_x >= SCREENWIDTH_PHYSICAL)
                break;

            int colindex = (col >> 16);

            const column_t *column = (const column_t*) ((const byte*) patch + (patch->columnofs[colindex] & 0xFFFFFF) );

            // step through the posts in a column
            while (column->topdelta != 0xff)
            {
                const byte *source = (const byte*) column + 3;
                const int topdelta = column->topdelta;

                int dc_yl = (y + topdelta); // (((y + topdelta) * DY) >> FRACBITS);
                int dc_yh = (y + topdelta + column->length); //(((y + topdelta + column->length) * DY) >> FRACBITS);

                // 2021-03-15 next-hack clip to drawing buffer boundary
                if (dc_yh > draw_stopy + 1)
                {
                    dc_yh = draw_stopy + 1;
                }

                if ((dc_yl > draw_stopy + 1) || (dc_yl > bottom))
                    break;

                // and also update source
                if (dc_yl < draw_starty)
                {
                    int delta = draw_starty - dc_yl;
                    source += delta;
                    dc_yl = draw_starty;
                }
                //
                int count = (dc_yh - dc_yl);
                byte *dest = byte_topleft + (dc_yl * byte_pitch) + dc_x;

                // Inner loop that does the actual texture mapping,
                //  e.g. a DDA-lile scaling.
                // This is as fast as it gets
                if (count > 0)
                {
                    //   PatchDrawColumn(dest, source, frac, fracstep, count);
                    unsigned int l = (count >> 4);

                    while (l--)
                    {
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                    }
                    unsigned int r = (count & 15);

                    switch (r)
                    {
                        case 15:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 14:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 13:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 12:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 11:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 10:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 9:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 8:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 7:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 6:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 5:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 4:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 3:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 2:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 1:
                            *dest = *source++;
                    }

                }
                column = (const column_t*) ((const byte*) column + column->length + 4);
            }
        }
    }
    else
    {
        // data in spi flash. we need a different strategy
        //
        // get patch address;
        uint32_t address = spiFlashSetAddress((uint32_t) patch);
        // now we need to load patch width, offsets and columnoffset
        short width, height;
        width = spiFlashGetShort();
        height = spiFlashGetShort(); // height, not used, but getting 2 bytes is cheaper than set the address again.
        // remove offset
        x -= spiFlashGetShort();    // patch->leftoffset;
        y -= spiFlashGetShort(); // patch->topoffset;
        // actually we should create a much smaller size. But we reserved a good deed of stack for this

        int columnofs[MAX_PATCH_COLOFFS];
        // read column offs
        if (width <= MAX_PATCH_COLOFFS)
        {
            spiFlashGetData(columnofs, width * sizeof(columnofs[0]));
        }
        else
        {
            spiFlashGetData(columnofs, MAX_PATCH_COLOFFS * sizeof(columnofs[0]));
        }

        int col = 0;

        const int DX = (SCREENWIDTH_PHYSICAL << 16) / 240;
        const int DXI = (240 << 16) / SCREENWIDTH_PHYSICAL;

        byte *byte_topleft = (byte*) _g->screens[0].data;
        const int byte_pitch = SCREENWIDTH_PHYSICAL;

        const int left = (x * DX) >> FRACBITS;
        const int right = ((x + width) * DX) >> FRACBITS;
        const int bottom = (y + height);
        int coloffset = 0;
        for (int dc_x = left; dc_x < right; dc_x++, col += DXI)
        {

            if (dc_x < 0)
                continue;

            if (dc_x >= SCREENWIDTH_PHYSICAL)
                break;

            int colindex = (col >> 16);

            // workaround for too much wide patches
            if (colindex - coloffset >= MAX_PATCH_COLOFFS)
            {
                // reload columnofs at the right point
                coloffset = colindex;
                spiFlashSetAddress((uint32_t) patch + 8 + 4 * coloffset);
                if (width - coloffset <= MAX_PATCH_COLOFFS)
                {
                    spiFlashGetData(columnofs, (width - coloffset) * sizeof(columnofs[0]));
                }
                else
                {
                    spiFlashGetData(columnofs, MAX_PATCH_COLOFFS * sizeof(columnofs[0]));
                }
            }
            spiFlashSetAddress(address + (columnofs[colindex - coloffset] & 0xFFFFFF));
            //
            column_t columnInfo;
            // read column info
            spiFlashGetData(&columnInfo, sizeof(columnInfo));
            // reserve buffer in stack
            uint8_t columnData[MAX_COLUMN_DATA];

            // step through the posts in a column
            while (columnInfo.topdelta != 0xff)
            {
                // ignore first byte.
                spiFlashGetByte();
                // get SPI data based on length
                spiFlashGetData(columnData, columnInfo.length);
                //
                const byte *source = (const byte*) columnData;

                const int topdelta = columnInfo.topdelta;

                int dc_yl = (y + topdelta);
                int dc_yh = (y + topdelta + columnInfo.length);

                // 2021-03-15 next-hack clip to drawing buffer boundary
                if (dc_yh > draw_stopy + 1)
                {
                    dc_yh = draw_stopy + 1;
                }

                if ((dc_yl > draw_stopy + 1) || (dc_yl > bottom))
                    break;

                // and also update source
                if (dc_yl < draw_starty)
                {
                    int delta = draw_starty - dc_yl;
                    source += delta;
                    dc_yl = draw_starty;
                }
                //
                int count = (dc_yh - dc_yl);
                byte *dest = byte_topleft + (dc_yl * byte_pitch) + dc_x;

                // Inner loop that does the actual texture mapping,
                //  e.g. a DDA-lile scaling.
                // This is as fast as it gets
                if (count > 0)
                {
                    unsigned int l = (count >> 4);
                    while (l--)
                    {
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        *dest = *source++;
                        dest += SCREENWIDTH_PHYSICAL;
                        //
                    }
                    unsigned int r = (count & 15);

                    switch (r)
                    {
                        case 15:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 14:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 13:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 12:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 11:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 10:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 9:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 8:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 7:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 6:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 5:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 4:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 3:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 2:
                            *dest = *source++;
                            dest += SCREENWIDTH_PHYSICAL;
                            // fall through, no break
                        case 1:
                            *dest = *source++;
                    }

                }
                // read again the column information
                // ignore last post byte.
                spiFlashGetByte();
                // read next info data (topdelta and length)
                spiFlashGetData(&columnInfo, sizeof(columnInfo));
            }
        }
    }
}

// CPhipps - some simple, useful wrappers for that function, for drawing patches from wads

// CPhipps - GNU C only suppresses generating a copy of a function if it is
// static inline; other compilers have different behaviour.
// This inline is _only_ for the function below

void V_DrawNumPatch(int x, int y, int scrn, int lump, int cm, enum patch_translation_e flags)
{
    V_DrawPatch(x, y, scrn, W_CacheLumpNum(lump));
}

//
// V_SetPalette
//
// CPhipps - New function to set the palette to palette number pal.
// Handles loading of PLAYPAL and calls I_SetPalette

void V_SetPalette(int pal)
{
    I_SetPalette(pal);
}

//Colour corrected PLAYPAL lumps ~ Kippykip
void V_SetPalLump(int index)
{
    if (index < 0)
        index = 0;
    else if (index > MAX_GAMMA)
        index = MAX_GAMMA;
#if 0
    char lumpName[9] = "PLAYPAL0";

    if(index == 0)
        lumpName[7] = 0;
    else
        lumpName[7] = '0' + index;

    _g->palette_lump = W_CacheLumpName(lumpName);
#endif
}

//
// V_FillRect
//
// CPhipps - New function to fill a rectangle with a given colour
void V_FillRect(int x, int y, int width, int height, byte colour)
{
    byte *fb = (byte*) _g->screens[0].data;

    byte *dest = &fb[(ScreenYToOffset(y) << 1) + x];

    // 2021-03-15 next-hack: clip to boundary

    int ystop = y + height - 1;

    if (y < draw_starty)
    {
        // adjust height too;
        height -= draw_starty - y;
        y = draw_starty;
    }
    if (ystop > draw_stopy)
    {
        height -= ystop - draw_stopy;
    }
    if (height <= 0)
        return;
    while (height--)
    {
        memset(dest, colour, width);
#ifndef HIGHRES
        dest += (SCREENPITCH << 1);
#else
        dest += SCREENPITCH;
#endif
    }
}

static void V_PlotPixel(int x, int y, int color)
{
    byte *fb = (byte*) _g->screens[0].data;
#ifndef HIGHRES
    uint16_t* dest = (uint16_t *) &fb[y * SCREENWIDTH_PHYSICAL + x * 2];
    *dest = color | ((uint16_t)color << 8);
#else
    if (y > draw_stopy || y < draw_starty)
        return;
    byte *dest = &fb[y * SCREENWIDTH_PHYSICAL + x];
    *dest = color;
#endif
}

//
// WRAP_V_DrawLine()
//
// Draw a line in the frame buffer.
// Classic Bresenham w/ whatever optimizations needed for speed
//
// Passed the frame coordinates of line, and the color to be drawn
// Returns nothing
//
void V_DrawLine(fline_t *fl, int color)
{

    int x0 = fl->a.x;
    int x1 = fl->b.x;

    int y0 = fl->a.y;
    int y1 = fl->b.y;

    int dx = D_abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;

    int dy = -D_abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;

    int err = dx + dy;

    while (true)
    {
        V_PlotPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}
