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
 *     next-hack: originally doomhack moved in this file (he named it
 *     r_hotpath.iwram.c) many functions taken from various files (r_draw.c,
 *     r_main.c, p_tick.c etc.) to be put on GBA IWRAM for speed.
 *     Now, there is no IWRAM on this system, so this file has been renamed
 *     to r_fast_Stuff.c. Progressively all the functions and corresponding
 *     variables will be brought back home (to their original positions).
 *     modifications:
 *     - Ton of modified code to work with short pointers.
 *     - removed texture cache (occupied 16kB)
 *     - Added back Z-depth Light (almost no ram penalty)
 *     - Added support for partial framebuffer.
 *     - Adapted to 240x240 display
 *     - Added support to fetch graphics data from SPI (sprites and patches)
 *     - Data from QSPI is fetched by DMA and this is done while the previous 
 *       column is being drawn, speeding up everything.
 *     - Openings are copied by words and not bytes.
 *     - Removed composite textures: the wad converter already prerenders 
 *       the composite texture to a single patch.
 *     - Current color map is copied to ram if the number of pixel being
 *       drawn is larger than 16. If the next column/span to be drawn has the
 *       same flash address as before, then the cached palette on RAM is used
 *       without copying and regardless the drawing size.
 *     - flats drawn to buffer at 4 pixels at once.
 *     - better ASM code when drawing column, to avoid unaligned 32 bit instructions.
 *     - and more.
 *
 *-----------------------------------------------------------------------------*/

#pragma GCC optimize ("Ofast")  // we need to compile this code to be as fast as possible.
#pragma GCC optimize ("align-functions=16")
#include "main.h"
#define D_SHRT_MAX 0x7FFF

#define FASTFUN  //__attribute__((used, section(".data")))

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CURRENT_COLOR_MAP_IDX 253
#define FIXED_COLOR_MAP_IDX 254
#define SHADOW_COLOR_MAP_IDX 255
#define FULL_COLOR_MAP_IDX 0

#include "i_spi_support.h"
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
#include "m_random.h"

#include "global_data.h"
#include "printf.h"
#include "i_memory.h"
// Note about openings. These are also used to store the column of the texture.
// We cannot use a byte, because it would give only 0 trough 255, but textures
// as wide as 256 exist, and we need an n+1 value to mark the opening as
// already checked.
#define NEGONEARRAY_PTR 1
#define SCREENHEIGHTARRAY_PTR 2
// let's put openings-related data in .openings_bss section
// this to make sure that openings are in the lower 128k so we can use short ptr
__attribute__ ((section(".openings_bss"))) short openings[MAXOPENINGS];
__attribute__ ((section(".openings_bss"))) short floorclip[SCREENWIDTH]; 
__attribute__ ((section(".openings_bss"))) short ceilingclip[SCREENWIDTH]; 
//
#define MAX_PATCH_WIDTH 320
// colormap cching
// note: for a single column or span, 16 pixel is sub optimal. But the likelyhood that the same color map will be used again is high
// therefore we sacrifice the first drawing, for the sake of the next ones.
#define MIN_PIXEL_TO_DRAW_FOR_CACHING_COLORMAP 16 
static uint8_t colorMap[256];  // for faster drawing
static uint8_t *currentCachedColorMap = NULL;
static uint8_t columnBuffer[2][MAX_COLUMN_DATA];
//
patch_t * cachedColumnOffsetDataPatch = NULL;
static uint32_t columnOffsetData[MAX_PATCH_WIDTH];


#define MAX_BSP_DEPTH 128
short bspstack[MAX_BSP_DEPTH]; // see R_RenderBSPNode for description

#if MMULTIPATCH_TEXTURE_SUPPORTULT 
uint8_t columnCacheBuffer[128];
#endif
// next-hack
short draw_starty;    // inclusive
short draw_stopy;     // inclusive
//


//*****************************************
//Globals.
//*****************************************
typedef int16_t small_int_t;   //to optimize.
short numnodes;
const mapnode_t *nodes;

fixed_t viewx, viewy, viewz;

angle_t viewangle;

static byte solidcol[MAX_SCREENWIDTH];

static byte spanstart[MAX_SCREENHEIGHT - ST_SCALED_HEIGHT];   // killough 2/8/98

static const seg_t *curline;
static side_t *sidedef;
static const line_t *linedef;
static sector_t *frontsector;
static sector_t *backsector;
static drawseg_t *ds_p;

static visplane_t *floorplane, *ceilingplane;
static int rw_angle1;

static angle_t rw_normalangle; // angle to line origin
static fixed_t rw_distance;

static small_int_t rw_stopx;

static fixed_t rw_scale;
static fixed_t rw_scalestep;

static int worldtop;
static int worldbottom;

static boolean didsolidcol; /* True if at least one column was marked solid */

// True if any of the segs textures might be visible.
static boolean segtextured;
static boolean markfloor;      // False if the back side is the same plane.
static boolean markceiling;
static boolean maskedtexture;
static small_int_t toptexture;
static small_int_t bottomtexture;
static small_int_t midtexture;

static fixed_t rw_midtexturemid;
static fixed_t rw_toptexturemid;
static fixed_t rw_bottomtexturemid;

const lighttable_t *fullcolormap;
const lighttable_t *fixedcolormap;

byte extralight;                           // bumped light from gun blasts
draw_vars_t drawvars;

static ycoord_t *mfloorclip;   // dropoff overflow
static ycoord_t *mceilingclip; // dropoff overflow
static volatile uint8_t mfloorceiling_ptr_size = sizeof(ycoord_t);
static fixed_t spryscale;
static fixed_t sprtopscreen;

static angle_t rw_centerangle;
static fixed_t rw_offset;
static small_int_t rw_lightlevel;

static short *maskedtexturecol; // dropoff overflow

const texture_t **textures; // proff - 04/05/2000 removed static for OpenGL
fixed_t *textureheight; //needed for texture pegging (and TFE fix - killough)

short *flattranslation;             // for global animation
short *texturetranslation;

fixed_t basexscale, baseyscale;

fixed_t viewcos, viewsin;

static fixed_t topfrac;
static fixed_t topstep;
static fixed_t bottomfrac;
static fixed_t bottomstep;

static fixed_t pixhigh;
static fixed_t pixlow;

static fixed_t pixhighstep;
static fixed_t pixlowstep;

static int worldhigh;
static int worldlow;

//static lighttable_t current_colormap[256]; // why do we need this anyway?
static const lighttable_t *current_colormap_ptr;

static uint8_t planezlight;
static uint8_t spritelights;
static uint8_t walllights;

static fixed_t planeheight;

size_t num_vissprite;

extern los_t los;

//*****************************************
// Constants
//*****************************************

const int viewheight = SCREENHEIGHT -   ST_SCALED_HEIGHT;
const int centery = (SCREENHEIGHT - ST_SCALED_HEIGHT) / 2;
#define CHEIGHT (SCREENHEIGHT / SCREENHEIGHT   * SCREENWIDTH * 3  / 4 ) // yes, I know, it makes SCREENWIDTH*3/4, just to remember.
#define BASEYCENTER (100 * CHEIGHT / SCREENHEIGHT) 

static const int centerxfrac = (SCREENWIDTH / 2) << FRACBITS;

static const int centeryfrac = ((SCREENHEIGHT - ST_SCALED_HEIGHT) / 2) << FRACBITS;

const fixed_t projection = (SCREENWIDTH / 2) << FRACBITS;
const fixed_t iprojection = ( FRACUNIT / (SCREENWIDTH / 2));

static const fixed_t projectiony = (((int64_t)(CHEIGHT * (SCREENWIDTH / 2) * 320) / 200) * FRACUNIT / SCREENWIDTH);

static const fixed_t pspritescale = FRACUNIT * SCREENWIDTH / 320;
static const fixed_t pspriteiscale = FRACUNIT * 320 / SCREENWIDTH;

static const fixed_t pspriteyscale = (((CHEIGHT * SCREENWIDTH) / SCREENWIDTH) << FRACBITS) / 200;

static const angle_t clipangle = 537395200; //xtoviewangle[0];

static const int skytexturemid = 100 * FRACUNIT;
static const fixed_t skyiscale = (FRACUNIT * 200) / (SCREENHEIGHT - ST_SCALED_HEIGHT);

#ifdef HIGHRES
typedef byte pixel;
#else
typedef short pixel;
#endif
//

#define check_limits(ptr) (((uint32_t) ptr) > RAM_PTR_BASE && ((uint32_t)ptr) < RAM_PTR_BASE + 524288)
#define check_rom_limits(ptr) (((uint32_t) ptr) > EXT_FLASH_BASE && ((uint32_t)ptr) < EXT_FLASH_BASE + 1024*1024*4)
static inline void fastOpeningsCopy(short *dest, short * source, int num);

FASTFUN inline static void* ByteFind(byte *mem, byte val, unsigned int count)
{
    do
    {
        if (*mem == val)
            return mem;

        mem++;
    } while (--count);

    return NULL;
}

FASTFUN inline fixed_t CONSTFUNC FixedMul(fixed_t a, fixed_t b)
{
    return (fixed_t) ((int_64_t) a * b >> FRACBITS);
}

// killough 5/3/98: reformatted

FASTFUN static CONSTFUNC int SlopeDiv(unsigned num, unsigned den)
{
    unsigned int ans;

    if (den < 512)
        return SLOPERANGE;

    ans = (num << 3) / (den >> 8);

    return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//

FASTFUN static PUREFUNC int R_PointOnSide(fixed_t x, fixed_t y, const mapnode_t *node)
{
    fixed_t dx = (fixed_t) node->dx << FRACBITS;
    fixed_t dy = (fixed_t) node->dy << FRACBITS;

    fixed_t nx = (fixed_t) node->x << FRACBITS;
    fixed_t ny = (fixed_t) node->y << FRACBITS;

    if (!dx)
        return x <= nx ? node->dy > 0 : node->dy < 0;

    if (!dy)
        return y <= ny ? node->dx < 0 : node->dx > 0;

    x -= nx;
    y -= ny;

    // Try to quickly decide by looking at sign bits.
    if ((dy ^ dx ^ x ^ y) < 0)
        return (dy ^ x) < 0;  // (left is negative)

    return FixedMul(y, node->dx) >= FixedMul(node->dy, x);
}
//
// 2021-03-13: forgot that switches have changeable textures...
FASTFUN static inline short getSideTopTexture(const line_t *line, const side_t *side)
{
    if (!line->const_special || side == &_g->sides[line->sidenum[1]])
        return side->toptexture;
    // otherwise we are asking for side 0 of a special texture.
    return _g->switch_texture_top[_g->linesChangeableTextureIndex[line->lineno]];
}
FASTFUN static inline short getSideMidTexture(const line_t *line, const side_t *side)
{
    if (!line->const_special || side == &_g->sides[line->sidenum[1]])
        return side->midtexture;
    // otherwise we are asking for side 0 of a special texture.
    return _g->switch_texture_mid[_g->linesChangeableTextureIndex[line->lineno]];
}
FASTFUN static inline short getSideBottomTexture(const line_t *line, const side_t *side)
{
    if (!line->const_special || side == &_g->sides[line->sidenum[1]])
        return side->bottomtexture;
    // otherwise we are asking for side 0 of a special texture.
    return _g->switch_texture_bot[_g->linesChangeableTextureIndex[line->lineno]];
}
//
// R_PointInSubsector
//
// killough 5/2/98: reformatted, cleaned up
FASTFUN subsector_t* R_PointInSubsector(fixed_t x, fixed_t y)
{
    int nodenum = numnodes - 1;

    // special case for trivial maps (single subsector, no nodes)
    if (numnodes == 0)
        return _g->subsectors;

    while (!(nodenum & NF_SUBSECTOR))
        nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes + nodenum)];
    return &_g->subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.
//

FASTFUN CONSTFUNC angle_t R_PointToAngle2(fixed_t vx, fixed_t vy, fixed_t x, fixed_t y)
{
    x -= vx;
    y -= vy;

    if ((!x) && (!y))
        return 0;

    if (x >= 0)
    {
        // x >=0
        if (y >= 0)
        {
            // y>= 0

            if (x > y)
            {
                // octant 0
                return tantoangle[SlopeDiv(y, x)];
            }
            else
            {
                // octant 1
                return ANG90 - 1 - tantoangle[SlopeDiv(x, y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x > y)
            {
                // octant 8
                return -tantoangle[SlopeDiv(y, x)];
            }
            else
            {
                // octant 7
                return ANG270 + tantoangle[SlopeDiv(x, y)];
            }
        }
    }
    else
    {
        // x<0
        x = -x;

        if (y >= 0)
        {
            // y>= 0
            if (x > y)
            {
                // octant 3
                return ANG180 - 1 - tantoangle[SlopeDiv(y, x)];
            }
            else
            {
                // octant 2
                return ANG90 + tantoangle[SlopeDiv(x, y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x > y)
            {
                // octant 4
                return ANG180 + tantoangle[SlopeDiv(y, x)];
            }
            else
            {
                // octant 5
                return ANG270 - 1 - tantoangle[SlopeDiv(x, y)];
            }
        }
    }
}

FASTFUN static CONSTFUNC angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    return R_PointToAngle2(viewx, viewy, x, y);
}

// killough 5/2/98: move from r_main.c, made static, simplified

FASTFUN static CONSTFUNC fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    fixed_t dx = D_abs(x - viewx);
    fixed_t dy = D_abs(y - viewy);

    if (dy > dx)
    {
        fixed_t t = dx;
        dx = dy;
        dy = t;
    }

    return FixedApproxDiv(dx, finesine[(tantoangle[FixedApproxDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT]);
}

FASTFUN static const lighttable_t* R_ColourMap(int lightlevel)
{
    if (fixedcolormap)
        return fixedcolormap;
    else
    {
        if (curline)
        {
            if (curline->v1.y == curline->v2.y)
                lightlevel -= 1 << LIGHTSEGSHIFT;
            else if (curline->v1.x == curline->v2.x)
                lightlevel += 1 << LIGHTSEGSHIFT;
        }

        lightlevel += (extralight + _g->gamma) << LIGHTSEGSHIFT;

        int cm = ((256 - lightlevel) >> 2) - 24;

        if (cm >= NUMCOLORMAPS)
            cm = NUMCOLORMAPS - 1;
        else if (cm < 0)
            cm = 0;

        return fullcolormap + cm * 256;
    }
}

/* 2021/05/11 next-hack removed, not used
 static const lighttable_t* R_LoadColorMap(int lightlevel)
 {
 const lighttable_t* lm = R_ColourMap(lightlevel);
 current_colormap_ptr = lm;
 return current_colormap_ptr;
 }*/

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
//
#define COLEXTRABITS 9
#define COLBITS (FRACBITS + COLEXTRABITS)

FASTFUN inline static void R_DrawColumnPixel(pixel *dest, const byte *source, const byte *colormap, unsigned int frac)
{
    *dest = colormap[source[frac >> COLBITS]];
}

FASTFUN /*__attribute__((used, section(".data")))*/ static void R_DrawColumn(const draw_column_vars_t *dcvars)
{
#if 0
    int clip_yh = dcvars->yh > draw_stopy ? draw_stopy : dcvars->yh;
    int clip_yl = dcvars->yl < draw_starty ? draw_starty : dcvars->yl;
    //
    // int count = (dcvars->yh - dcvars->yl) + 1;
    int count = (clip_yh - clip_yl) + 1;
    // Zero length, column does not exceed a pixel.
    if (count <= 0)
        return;
    if (isOnExternalFlash(dcvars->source))
    {
        printf("Error address on external flash\r\n");
    }
    const byte *source = dcvars->source;
    const byte *colormap = dcvars->colormap;

    pixel* dest = (( pixel*) drawvars.byte_topleft) + ScreenYToOffset(clip_yl) + dcvars->x;

    const unsigned int      fracstep = (dcvars->iscale << COLEXTRABITS);
    unsigned int frac = (dcvars->texturemid + (dcvars->yl - centery)*dcvars->iscale) << COLEXTRABITS;
    //
    frac += fracstep * (clip_yl - dcvars->yl);
#else
    //
    int count = (dcvars->yh - dcvars->yl) + 1;
    // Zero length, column does not exceed a pixel.
 
    if (count <= 0)
        return;
    const byte *colormap;
#if CACHE_ALL_COLORMAP_TO_RAM
    colormap = dcvars->colormap;
#else
    // 2021-09-18 next-hack: faster ram access time 
    if (currentCachedColorMap == dcvars->colormap)
    {
        colormap =colorMap;
    }
    else if (count > MIN_PIXEL_TO_DRAW_FOR_CACHING_COLORMAP)
    {
        memcpy(colorMap, dcvars->colormap, 256);
        currentCachedColorMap = (uint8_t*) dcvars->colormap;
        colormap = colorMap;
    }
    else
    {
        colormap = dcvars->colormap;
    }
#endif    
#if CHECK_CACHING
    if (isOnExternalFlash(dcvars->source))
    {
        printf("Error address on external flash\r\n");
    }
#endif
    const byte *source = dcvars->source;
   

    pixel *dest = ((pixel*) drawvars.byte_topleft) + ScreenYToOffset(dcvars->yl) + dcvars->x;

    unsigned int fracstep = (dcvars->iscale << COLEXTRABITS);
    unsigned int frac = (dcvars->texturemid + (dcvars->yl - centery) * dcvars->iscale) << COLEXTRABITS;
    //

#endif
    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.

    unsigned int l = (count >> 4);
#define BETTER_ASM_CODE 1
 #if BETTER_ASM_CODE
 while (l--)
 {
   uint32_t fracShift;
   uint32_t dummyTmp;
   asm volatile 
   (
      ".align(2)\n\t"
      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"

      "lsrs %[dummyTmp], %[frac], #25\n\t"
      "add %[frac], %[fracStep]\n\t"
      "ldrb %[dummyTmp], [%[source], %[dummyTmp]]\n\t"
      "ldrb %[dummyTmp], [%[colormap], %[dummyTmp]]\n\t"
      "strb %[dummyTmp], [%[dest]], #0xF0\n\t"


     :
     [fracShift] "+l" (fracShift),
     [frac] "+l" (frac),
     [fracStep] "+r" (fracstep),
     [dummyTmp] "+l" (dummyTmp),
     [dest] "+l" (dest),
     [colormap] "+l" (colormap),
     [source] "+l" (source)
     : : "cc"   // TODO: we might avoid using -s istructions, and remove this.
   );
}
 #else
    while (l--)
    {
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;

        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;

        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;

        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
        R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
        dest += SCREENWIDTH;
        frac += fracstep;
    }
#endif
    unsigned int r = (count & 15);

    switch (r)
    {
        case 15:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 14:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 13:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 12:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 11:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 10:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 9:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 8:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 7:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 6:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 5:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 4:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 3:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 2:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
            dest += SCREENWIDTH;
            frac += fracstep;
            // fall through, no break
        case 1:
            R_DrawColumnPixel((pixel*) dest, source, colormap, frac);
    }
}

#define FUZZOFF (SCREENWIDTH)
#define FUZZTABLE 50

static const int fuzzoffset[FUZZTABLE] =
{
    FUZZOFF, -FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
    FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
    FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF,
    FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
    FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF,
    FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF,
    FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF 
};

//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
FASTFUN static void R_DrawFuzzColumn(const draw_column_vars_t *dcvars)
{
    int dc_yl = dcvars->yl;
    int dc_yh = dcvars->yh;

    // Adjust borders. Low...
    if (dc_yl <= 0)
        dc_yl = 1;

    // .. and high.
    if (dc_yh >= viewheight - 1)
        dc_yh = viewheight - 2;

    int count = (dc_yh - dc_yl) + 1;

    // Zero length, column does not exceed a pixel.
    if (count <= 0)
        return;

    const byte *colormap = &fullcolormap[6 * 256];

    pixel *dest = ((pixel*) drawvars.byte_topleft) + ScreenYToOffset(dc_yl) + dcvars->x;

    unsigned int fuzzpos = _g->fuzzpos;

    do
    {
        R_DrawColumnPixel((pixel*) dest, (const byte*) &dest[fuzzoffset[fuzzpos]], colormap, 0);
        dest += SCREENWIDTH;
        fuzzpos++;

        if (fuzzpos >= 50)
            fuzzpos = 0;

    } while (--count);

    _g->fuzzpos = fuzzpos;
}

//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
FASTFUN static inline void R_DrawMaskedColumn(R_DrawColumn_f colfunc, draw_column_vars_t *dcvars, const column_t *column)
{
#if CHECK_CACHING
    if (isOnExternalFlash(column))
    {
        printf("Column in ext flash\r\n");
        while(1);
    }
#endif
    const fixed_t basetexturemid = dcvars->texturemid;
    int fclip_x; //= ((byte*)mfloorclip)[dcvars->x];
    int cclip_x; //= ((byte*)mceilingclip)[dcvars->x];
/*    if (mfloorceiling_ptr_size == 1)
    {
      printf("pter size 1 can't clip. Blocking");
      while (1);
    }
    else*/
    {
        fclip_x = mfloorclip[dcvars->x];
        cclip_x = mceilingclip[dcvars->x];
    }
    while (column->topdelta != 0xff)
    {
        // calculate unclipped screen coordinates for post
        const int topscreen = sprtopscreen + spryscale * column->topdelta;
        const int bottomscreen = topscreen + spryscale * column->length;

        int yh = (bottomscreen - 1) >> FRACBITS;
        int yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
//        if (!((yh < draw_starty ) || (yl > draw_stopy))) // if at least a part is within range of current buffer
        {
            if (yh >= fclip_x)
                yh = fclip_x - 1;

            if (yl <= cclip_x)
                yl = cclip_x + 1;

            // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
            if (yh < viewheight && yl <= yh)
            {
                dcvars->source = (const byte*) column + 3;

                dcvars->texturemid = basetexturemid - (column->topdelta << FRACBITS);

                dcvars->yh = yh;
                dcvars->yl = yl;

                // Drawn by either R_DrawColumn
                //  or (SHADOW) R_DrawFuzzColumn.
                colfunc(dcvars);
            }
        }
        column = (const column_t*) ((const byte*) column + column->length + 4);
    }

    dcvars->texturemid = basetexturemid;
}
static inline uint32_t getColumnOffsetData(patch_t *patch, int colindex)
{
    if (cachedColumnOffsetDataPatch == patch)
    {
        return columnOffsetData[colindex];
    }
    else
    {
        return *((uint32_t*) ((uint32_t) patch + 8 + 4 * colindex));
    }
}
static inline void getColumnDataNonBlocking(const patch_t *patch, int colindex, uint8_t *columnData)
{
    uint32_t co = getColumnOffsetData((patch_t*) patch, colindex);
    uint32_t cnt = co >> 24;
    co = co & 0xFFFFFF;
    NRF_QSPI->READ.CNT = cnt;
    NRF_QSPI->READ.SRC = (uint32_t) patch + co;
    NRF_QSPI->READ.DST = (uint32_t) columnData;
    NRF_QSPI->EVENTS_READY = 0;
    NRF_QSPI->TASKS_READSTART = 1;
}
FASTFUN static inline column_t* getColumnData(const patch_t *patch, int colindex, uint8_t *columnData)
{


#if 1
    uint32_t co = getColumnOffsetData((patch_t *) patch, colindex);
    uint32_t cnt = co >> 24;
    co = co & 0xFFFFFF;
    NRF_QSPI->READ.CNT = cnt;
    NRF_QSPI->READ.SRC = ((uint32_t) patch + co);
    NRF_QSPI->READ.DST = (uint32_t) columnData;
    NRF_QSPI->EVENTS_READY = 0;
    NRF_QSPI->TASKS_READSTART = 1;
    while (NRF_QSPI->EVENTS_READY == 0);
    NRF_QSPI->EVENTS_READY = 0;    
    return (column_t*) columnData;
#elif 1
    uint32_t co = getColumnOffsetData((patch_t *) patch, colindex);;
    uint32_t cnt = co >> 24;
    co = co & 0xFFFFFF;
    memcpy(columnData, (uint8_t *) patch + co, cnt); 
#else
    // there is no major benefit using this
    uint32_t co = getColumnOffsetData((patch_t *) patch, colindex);;
    //spiFlashSetAddress((uint32_t) patch + 8 + 4 * colindex);
    //uint32_t co = (uint16_t) spiFlashGetShort();
    uint32_t cnt = co >> 24;
    co = co & 0xFFFFFF;
    if (cnt <= 136)
    {
        memcpy(columnData, (uint8_t *) patch + co, cnt); 
    }
    else
    {
        NRF_QSPI->READ.CNT = cnt;
        NRF_QSPI->READ.SRC = ((uint32_t) patch + co);
        NRF_QSPI->READ.DST = (uint32_t) columnData;
        NRF_QSPI->EVENTS_READY = 0;
        NRF_QSPI->TASKS_READSTART = 1;
        while (NRF_QSPI->EVENTS_READY == 0);
        NRF_QSPI->EVENTS_READY = 0;    
    }
    return (column_t*) columnData;
#endif
}
static inline void cachePatchColumnOffsetData(patch_t *patch, int width, int startX, int stopX)
{
    // this function will cache the whole coloffset data, but only when needed
    if (!isOnExternalFlash(patch))
        return;     // do not cache data already in flash
    if (patch == cachedColumnOffsetDataPatch)
        return;    // do not recache cached data.
    // get how many bytes we need to get
    int cnt = (stopX - startX);
#if 1
    // cache only when convenient. Each random access of words would take about 260 cycles.
    // Also memcpy takes about 290 cycles + about 4 per byte. Therefore we have 290 + (4 * width) * 4
    // the disequation is 260 * (cnt + 1) > 340 + width * 16 
    if ( 260 * cnt > 30 + width * 16)  
    {
        // set cached address
        cachedColumnOffsetDataPatch = patch;
        memcpy(columnOffsetData, (void*) ((uint32_t) patch + 8), 4 * width); 
    }
#else
    // cache only when convenient. Each random access of words would take about 260 cycles.
    // Also the Easy DMA takes about 340 cycles. The time later taken by the DMA is about (width * 4) * 4 (we have 16 B per us)
    // the disequation is 260 * cnt > 340 + width * 16 
    if ( 260 * cnt > 340 + width * 16)  
    {

        NRF_QSPI->READ.CNT = width * 4; // set t read the whole columnoffset
        NRF_QSPI->READ.SRC = ((uint32_t) patch + 8 );
        NRF_QSPI->READ.DST = (uint32_t) columnOffsetData;
        NRF_QSPI->EVENTS_READY = 0;
        NRF_QSPI->TASKS_READSTART = 1;
        // set cached address
        cachedColumnOffsetDataPatch = patch;
        while (NRF_QSPI->EVENTS_READY == 0);
        NRF_QSPI->EVENTS_READY = 0;    
    }

#endif
}
//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
// CPhipps - new wad lump handling, *'s to const*'s
FASTFUN static void R_DrawVisSprite(const vissprite_t *vis)
{
    fixed_t frac;

    R_DrawColumn_f colfunc;
    draw_column_vars_t dcvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    switch (vis->colormap_idx)
    {
        case CURRENT_COLOR_MAP_IDX:
            dcvars.colormap = current_colormap_ptr;
            break;
        case FIXED_COLOR_MAP_IDX:
            dcvars.colormap = fixedcolormap;
            break;
        case SHADOW_COLOR_MAP_IDX:
            dcvars.colormap = NULL;
            break;
        case FULL_COLOR_MAP_IDX:
            dcvars.colormap = fullcolormap;
            break;
        default:
            dcvars.colormap = fullcolormap + 256 * vis->colormap_idx;
    }
    // killough 4/11/98: rearrange and handle translucent sprites
    // mixed with translucent/non-translucenct 2s normals

    if (!dcvars.colormap)   // NULL colormap = shadow draw
        colfunc = R_DrawFuzzColumn;    // killough 3/14/98
    else
    {
        // 2021/02/13 next-hack. To save some bytes, vissprites will have only a byte mobjflags.
        // also, multiplayer is not enabled, so MF_TRANSLATION is never present.
#if 0
      if (vis->mobjflags & (MF_TRANSLATION >> MF_TRANSSHIFT))
      {
        // next-hack: multiplayer not supported.
        printf("Draw Translated column, blocking\r\n");
        while(1);
        //colfunc = R_DrawTranslatedColumn;
        //dcvars.translation = translationtables + ((vis->mobjflags & (MF_TRANSLATION >> MF_TRANSSHIFT)) << 8 );
      }
      else
#endif
        colfunc = R_DrawColumn; // killough 3/14/98, 4/11/98

    }

    // proff 11/06/98: Changed for high-res
    dcvars.iscale = FixedReciprocal(vis->scale);
    dcvars.texturemid = vis->texturemid;
    frac = vis->startfrac;

    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);

    //const patch_t *patch = vis->patch;
    const patch_t *patch = W_CacheLumpNum(vis->lumpNum);
    if (false == isOnExternalFlash(patch))
    {
        for (dcvars.x = vis->x1; dcvars.x <= vis->x2; dcvars.x++, frac += vis->xiscale)
        {
            const column_t *column = (const column_t*) ((const byte*) patch + (0xFFFFFF & patch->columnofs[frac >> FRACBITS]));
            R_DrawMaskedColumn(colfunc, &dcvars, column);
        }
    }
    else
    { // sprite is in Spi flash. different approach here!
      // note that columnOfs are typically less than 64kB, therefore we only read shorts.
      // furthermore:
      // for sprites very up or down scaled (the majority of the cases), reading all the columns is counterproductive.
      // It is better to read directly the single column ofs.
      // furthermore, for mirrored sprites, xiscale might be negative.
      if (vis->x1 <= vis->x2)
      {
//        uint32_t oldColindex = 0xFFFFFFFF;  // invalid index
        //uint8_t columnData[MAX_COLUMN_DATA];
        // get first column
        uint32_t colindex = frac >> FRACBITS;
        //oldColindex = colindex;
        // calculate already next position
        frac += vis->xiscale;
        // set first buffer
        uint32_t bufferNumber = 0;
        cachePatchColumnOffsetData((patch_t*) patch, p_wad_immutable_flash_data->patchLumpSizeOffsets[vis->lumpNum].width, vis->x1 , vis->x2);
        getColumnDataNonBlocking(patch, colindex, columnBuffer[bufferNumber]);
        // Check if we are going to draw a column twice. TODO: improve this!
        fixed_t absXiscale = D_abs(vis->xiscale);
        if (absXiscale >= 0x10000)
        {
            for (dcvars.x = vis->x1; dcvars.x < vis->x2;  dcvars.x++, frac += vis->xiscale)
            {
                // get current column
                colindex = frac >> FRACBITS;

                // wait till DMA has finished loading the column
                while(!NRF_QSPI->EVENTS_READY) {};
                NRF_QSPI->EVENTS_READY = 0;

                getColumnDataNonBlocking(patch, colindex, columnBuffer[1 -bufferNumber]);
                R_DrawMaskedColumn(colfunc, &dcvars, (column_t*) columnBuffer[bufferNumber]);
                // swap buffers
                bufferNumber = 1 - bufferNumber;
            }
            // draw last column! We need to wait last DMA first
            while(!NRF_QSPI->EVENTS_READY) {};
            NRF_QSPI->EVENTS_READY = 0;
            R_DrawMaskedColumn(colfunc, &dcvars, (column_t*) columnBuffer[bufferNumber]);
        }
        else
        {   
            // the sprite is magnified, so at least one column would be drawn twice. Not loading a column might save us 1 us, offsetting the
            // additional checks
            for (uint8_t x = vis->x1; x <= vis->x2;  )
            {
                // set start x
                dcvars.x = x;
                uint32_t newColindex;
                // get current column
                colindex = frac >> FRACBITS;
                // calculate how many columns we should draw with the same clumn data.
                do
                {
                    // calculate the new column
                    newColindex = frac >> FRACBITS;
                    x++;
                    frac += vis->xiscale;
                    newColindex++;     
                } while ( newColindex == colindex && x <= vis->x2);
                // x now contains the first column not equal with the data pointed in colindex
                // if x is x2 or less, then there is still one more column to be loaded. Otherwise, if it is x2+1, it means that this
                // last column will be drawn outside of this loop.
                if (x <= vis->x2)
                {
                    // wait till DMA has finished loading the column
                    while(!NRF_QSPI->EVENTS_READY) {};
                    NRF_QSPI->EVENTS_READY = 0;
                    getColumnDataNonBlocking(patch, colindex, columnBuffer[1 -bufferNumber]);
                    // draw all the columns with the same data
                    for (; dcvars.x < x; dcvars.x++)
                    {
                        R_DrawMaskedColumn(colfunc, &dcvars, (column_t*) columnBuffer[bufferNumber]);
                    }
                    // swap buffers
                    bufferNumber = 1 - bufferNumber;
                }

            }
            // draw last column or group of column, if needed. We need to wait last DMA first
            if (dcvars.x <= vis->x2)
            {
                while(!NRF_QSPI->EVENTS_READY) {};
                NRF_QSPI->EVENTS_READY = 0;
                for (; dcvars.x <= vis->x2; dcvars.x++)
                {
                    R_DrawMaskedColumn(colfunc, &dcvars, (column_t*) columnBuffer[bufferNumber]);
                }
            }

        }
      }
    }
}
#if MULTIPATCH_TEXTURE_SUPPORT
FASTFUN static inline column_t* R_GetColumn(const texture_t *texture, int texcolumn, uint8_t *columnData)
{
    const unsigned int patchcount = texture->patchcount;
    const unsigned int widthmask = texture->widthmask;

    const int xc = texcolumn & widthmask;

    if (patchcount == 1)
    {
        //simple texture.
        const patch_t *patch = texture->patches[0].patch;
        if (!isOnExternalFlash(patch))
        {
            return (column_t*) ((byte*) patch + (0xFFFFFF & patch->columnofs[xc]));
        }
        else
        {
            return getColumnData(patch, xc, columnData);
        }
    }
    else
    {
        unsigned int i = 0;

        do
        {

            const texpatch_t *patch = &texture->patches[i];

            const patch_t *realpatch = patch->patch;

            const int x1 = patch->originx;

            if (xc < x1)
                continue;
            short width;

            if (isOnExternalFlash(realpatch))
            {
                spiFlashSetAddress((uint32_t) &realpatch->width);
                width = spiFlashGetShort();
            }
            else
            {
                width = realpatch->width;
            }
            const int x2 = x1 + width;

            if (xc < x2)
            {
                if (isOnExternalFlash(realpatch))
                {
                    return getColumnData(realpatch, xc - x1, columnData);
                }
                else
                {
                    return (column_t*) ((byte*) realpatch + (realpatch->columnofs[xc - x1] & 0xFFFFFF) );
                }
            }

        } while (++i < patchcount);
    }
    return NULL;
}
#else
FASTFUN static inline column_t* R_GetColumn(const texture_t *texture, int texcolumn, uint8_t *columnData)
{
    const unsigned int widthmask = texture->widthmask;

    const int xc = texcolumn & widthmask;
    //simple texture.
    const patch_t *patch = texture->patches[0].patch;
    if (!isOnExternalFlash(patch))
    {
        return (column_t*) ((byte*) patch + (0xFFFFFF & patch->columnofs[xc]));
    }
    else
    {
        return getColumnData(patch, xc, columnData);
    }
}

#endif

FASTFUN inline static column_t* R_GetColumnNonBlocking(const texture_t *texture, int texcolumn, uint8_t *columnData, bool *wasOnExternalFlash)
{
    const unsigned int widthmask = texture->widthmask;

    const int xc = texcolumn & widthmask;
#if MULTIPATCH_TEXTURE_SUPPORT
    const unsigned int patchcount = texture->patchcount;

    if (patchcount == 1)
    {
        //simple texture.
        const patch_t *patch = texture->patches[0].patch;
        if (!isOnExternalFlash(patch))
        {
            *wasOnExternalFlash = false;
            return (column_t*) ((byte*) patch + (0xFFFFFF & patch->columnofs[xc]));
        }
        else
        {
            *wasOnExternalFlash = true;
            getColumnDataNonBlocking(patch, xc, columnData);
            return (column_t*) columnData;
        }
    }
    else
    {
        unsigned int i = 0;

        do
        {

            const texpatch_t *patch = &texture->patches[i];

            const patch_t *realpatch = patch->patch;

            const int x1 = patch->originx;

            if (xc < x1)
                continue;
            short width;

            if (isOnExternalFlash(realpatch))
            {
                spiFlashSetAddress((uint32_t) &realpatch->width);
                width = spiFlashGetShort();
                const int x2 = x1 + width;
                if (xc < x2)
                {
                    *wasOnExternalFlash = true;
                    getColumnDataNonBlocking(realpatch, xc - x1, columnData);
                    return (column_t*) columnData;
                }
            }
            else
            {
                width = realpatch->width;
                const int x2 = x1 + width;
                if (xc < x2)
                {
                    *wasOnExternalFlash = false;
                    return (column_t*) ((byte*) realpatch + (realpatch->columnofs[xc - x1] & 0xFFFFFF) );
                }
            }
        } while (++i < patchcount);
    }
    *wasOnExternalFlash = false;
    return NULL;
    #else
            //simple texture.
        const patch_t *patch = texture->patches[0].patch;
        if (!isOnExternalFlash(patch))
        {
            *wasOnExternalFlash = false;
            return (column_t*) ((byte*) patch + (0xFFFFFF & patch->columnofs[xc]));
        }
        else
        {
            *wasOnExternalFlash = true;
            getColumnDataNonBlocking(patch, xc, columnData);
            return (column_t*) columnData;
        }

    #endif
}



FASTFUN static inline const texture_t* R_GetOrLoadTexture(int tex_num)
{
    const texture_t *tex = textures[tex_num];
    if (!tex)
    {
        // we are in the middle of a level. No saving to flash.
        tex = R_GetTexture(tex_num, false, NULL);
    }
    return tex;
}
static const uint32_t openingPtrs[] = {0, (uint32_t) negonearray,  (uint32_t) screenheightarray};
FASTFUN static inline short* openingSsptrToAddr(unsigned short ssptr)
{
   if (ssptr <= SCREENHEIGHTARRAY_PTR)
    return (short*) openingPtrs[ssptr];

    /*
    if (ssptr == 0)
        return NULL;
    else if (ssptr == NEGONEARRAY_PTR)
        return (short*) negonearray;
    else if (ssptr == SCREENHEIGHTARRAY_PTR)
        return (short*) screenheightarray;*/
    else
    {
        return (short*) (RAM_PTR_BASE | (ssptr << 1));
//   return &openings[ssptr - 1];
    }
}
FASTFUN static inline unsigned short openingAddrToSsptr(short *ptr)
{

/*  if ((uint32_t) ptr & 0x20001)
  {
    printf("Warning opening outside 128k, blocking \r\n");
    while (1);
  }*/
    return ((unsigned int) ptr) >> 1;
    
  //return 1 + ((((uint32_t) ptr) - ( (uint32_t) &openings[0])) >> 1);
}
//
// R_RenderMaskedSegRange
//
FASTFUN static void R_RenderMaskedSegRange(const drawseg_t *ds, int x1, int x2)
{
    int texnum;
    draw_column_vars_t dcvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?

    curline = ds->curline;  // OPTIMIZE: get rid of LIGHTSEGSHIFT globally

    frontsector = SG_FRONTSECTOR(curline);
    backsector = SG_BACKSECTOR(curline);

    texnum = getSideMidTexture(&_g->lines[curline->linenum], &_g->sides[curline->sidenum]);
    texnum = texturetranslation[texnum];
    // next-hack: added back lighting

    int lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT) + extralight;

    if (curline->v1.y == curline->v2.y)
        lightnum--;
    else if (curline->v1.x == curline->v2.x)
        lightnum++;

    if (lightnum < 0)
        walllights = 0;
    else if (lightnum >= LIGHTLEVELS)
        walllights = LIGHTLEVELS - 1;
    else
        walllights = lightnum;

    // killough 4/13/98: get correct lightlevel for 2s normal textures
    rw_lightlevel = frontsector->lightlevel;

    maskedtexturecol = ds->maskedtexturecol;

    rw_scalestep = ds->scalestep;
    spryscale = ds->scale1 + (x1 - ds->x1) * rw_scalestep;
    // to save some bytes, we might mfloorclip and mceilingclip as chars, some times.
    // In this case, we are pointing to short, so we need to tell also the size of what
    // mfloorclip and mceilingclip are pointing to.
    mfloorclip = (ycoord_t*) openingSsptrToAddr(ds->sprbottomclip_ssptr);
    mceilingclip = (ycoord_t*) openingSsptrToAddr(ds->sprtopclip_ssptr);
    mfloorceiling_ptr_size = sizeof(short);
    // find positioning
    if (_g->lines[curline->linenum].flags & ML_DONTPEGBOTTOM)
    {
        dcvars.texturemid =
                frontsector->floorheight > backsector->floorheight ? frontsector->floorheight : backsector->floorheight;
        dcvars.texturemid = dcvars.texturemid + textureheight[texnum] - viewz;
    }
    else
    {
        dcvars.texturemid =
                frontsector->ceilingheight < backsector->ceilingheight ? frontsector->ceilingheight : backsector->ceilingheight;
        dcvars.texturemid = dcvars.texturemid - viewz;
    }
    dcvars.texturemid += (_g->sides[curline->sidenum].rowoffset << FRACBITS);
    const texture_t *texture = R_GetOrLoadTexture(texnum);
    if (fixedcolormap)
        dcvars.colormap = fixedcolormap;
#if 0
    // draw the columns
    column_t *column = column; // self assign to suppress uninit warning without code generation
    int oldXc = 0xFF000000;
    uint8_t columnData[MAX_COLUMN_DATA];
    for (dcvars.x = x1; dcvars.x <= x2; dcvars.x++, spryscale += rw_scalestep)
    {
        const int xc = maskedtexturecol[dcvars.x];

        if (xc != D_SHRT_MAX) // dropoff overflow
        {
            if (!fixedcolormap)
            {
                int index = spryscale >> LIGHTSCALESHIFT;

                if (index >= MAXLIGHTSCALE)
                    index = MAXLIGHTSCALE - 1;

                dcvars.colormap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];
            }
            sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);
            //
            dcvars.iscale = FixedReciprocal((unsigned) spryscale);

            // draw the texture
            if (oldXc != xc)
            {
                oldXc = xc;
                column = R_GetColumn(texture, xc, columnData);
            }
            R_DrawMaskedColumn(R_DrawColumn, &dcvars, column);

            maskedtexturecol[dcvars.x] = D_SHRT_MAX; // dropoff overflow
        }
    }
 #else
     // draw the columns
    column_t *column = column; // self assign to suppress uninit warning without code generation
    column_t *nextColumn = column; 
    if (x1 <= x2)       // is there any column to draw?
    {
      int x = x1;
      bool wasOnExternalFlash = false;
      uint32_t bufferNumber = 0;
      int xc = maskedtexturecol[x];
      // get the first column
      //
      cachePatchColumnOffsetData( (patch_t*) texture->patches[0].patch, texture->width, x1 , x2);
      //
      column = R_GetColumnNonBlocking(texture, xc, columnBuffer[bufferNumber], &wasOnExternalFlash);
      //
      for (dcvars.x = x1; dcvars.x < x2; dcvars.x++, spryscale += rw_scalestep)
      {
          if (xc != D_SHRT_MAX) // dropoff overflow
          {
              if (!fixedcolormap)
              {
                  int index = spryscale >> LIGHTSCALESHIFT;

                  if (index >= MAXLIGHTSCALE)
                      index = MAXLIGHTSCALE - 1;

                  dcvars.colormap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];
              }
              sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);
              //
              dcvars.iscale = FixedReciprocal((unsigned) spryscale);
              // draw the texture
              if (wasOnExternalFlash)
              {
                while (!NRF_QSPI->EVENTS_READY) {};
                NRF_QSPI->EVENTS_READY = 0;
              }
              nextColumn = R_GetColumnNonBlocking(texture, xc, columnBuffer[1 - bufferNumber], &wasOnExternalFlash);
              R_DrawMaskedColumn(R_DrawColumn, &dcvars, column);
              column = nextColumn;
              maskedtexturecol[dcvars.x] = D_SHRT_MAX; // dropoff overflow
              bufferNumber = 1 - bufferNumber;
          }
          xc = maskedtexturecol[dcvars.x + 1];
      }
      // draw last column
      if (xc != D_SHRT_MAX) // dropoff overflow
      {
          if (!fixedcolormap)
          {
              int index = spryscale >> LIGHTSCALESHIFT;

              if (index >= MAXLIGHTSCALE)
                  index = MAXLIGHTSCALE - 1;

              dcvars.colormap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];
          }
          sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);
          //
          dcvars.iscale = FixedReciprocal((unsigned) spryscale);
          // draw the texture. However, wait till the buffer is ready, if we read if from external flash.
          if (wasOnExternalFlash)
          {
            while (!NRF_QSPI->EVENTS_READY) {};
            NRF_QSPI->EVENTS_READY = 0;
          }
          R_DrawMaskedColumn(R_DrawColumn, &dcvars, column);
          maskedtexturecol[dcvars.x] = D_SHRT_MAX; // dropoff overflow
      }

    }
 #endif
    curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
}

// killough 5/2/98: reformatted

FASTFUN static PUREFUNC int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
{
    const fixed_t lx = line->v1.x;
    const fixed_t ly = line->v1.y;
    const fixed_t ldx = line->v2.x - lx;
    const fixed_t ldy = line->v2.y - ly;

    if (!ldx)
        return x <= lx ? ldy > 0 : ldy < 0;

    if (!ldy)
        return y <= ly ? ldx < 0 : ldx > 0;

    x -= lx;
    y -= ly;

    // Try to quickly decide by looking at sign bits.
    if ((ldy ^ ldx ^ x ^ y) < 0)
        return (ldy ^ x) < 0;          // (left is negative)

    return FixedMul(y, ldx >> FRACBITS) >= FixedMul(ldy >> FRACBITS, x);
}

//
// R_DrawSprite
//

FASTFUN static void R_DrawSprite(const vissprite_t *spr)
{
    ycoord_t *clipbot = floorclip;
    ycoord_t *cliptop = ceilingclip;

    fixed_t scale;
    fixed_t lowscale;
    for (int x = spr->x1; x <= spr->x2; x++)
    {
        clipbot[x] = draw_stopy + 1;
        cliptop[x] = draw_starty - 1;
    }

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale is the clip seg.

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):

    const drawseg_t *drawsegs = _g->drawsegs;

    for (const drawseg_t *ds = ds_p; ds-- > drawsegs;)  // new -- killough
    {
        // determine if the drawseg obscures the sprite
        if (ds->x1 > spr->x2 || ds->x2 < spr->x1 || (!ds->silhouette && !ds->maskedtexturecol))
            continue;      // does not cover sprite

        const int r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        const int r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

        if (ds->scale1 > ds->scale2)
        {
            lowscale = ds->scale2;
            scale = ds->scale1;
        }
        else
        {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }

        if (scale < spr->scale || (lowscale < spr->scale && !R_PointOnSegSide(spr->gx, spr->gy, ds->curline)))
        {
            if (ds->maskedtexturecol)       // masked mid texture?
                R_RenderMaskedSegRange(ds, r1, r2);

            continue;               // seg is behind sprite
        }

        // clip this piece of the sprite
        // killough 3/27/98: optimized and made much shorter

        if ((ds->silhouette & SIL_BOTTOM) && spr->gz < ds->bsilheight) //bottom sil
        {
            for (int x = r1; x <= r2; x++)
            {
                if (clipbot[x] == draw_stopy + 1)
                {
                    clipbot[x] = openingSsptrToAddr(ds->sprbottomclip_ssptr)[x];
                }
            }

        }

        if ((ds->silhouette & SIL_TOP) && spr->gzt > ds->tsilheight)  // top sil
        {
            for (int x = r1; x <= r2; x++)
            {
                if (cliptop[x] == draw_starty - 1)
                {
                    cliptop[x] = openingSsptrToAddr(ds->sprtopclip_ssptr)[x];
                }
            }
        }
    }

    // all clipping has been performed, so draw the sprite
    mfloorclip = clipbot;
    mceilingclip = cliptop;
    mfloorceiling_ptr_size = sizeof(*clipbot);
    R_DrawVisSprite(spr);
}

//
// R_DrawPSprite
//

FASTFUN static void R_DrawPSprite(pspdef_t *psp, int lightlevel)
{
    int x1, x2;
    spritedef_t *sprdef;
    spriteframe_t *sprframe;
    boolean flip;
    vissprite_t *vis;
    vissprite_t avis;
    int width;
    fixed_t topoffset;

    // decide which patch to use
    sprdef = &p_wad_immutable_flash_data->sprites[psp->state->sprite];

    sprframe = &(getSpriteFrames(sprdef)[psp->state->frame & FF_FRAMEMASK]);

    flip = (boolean) SPR_FLIPPED(sprframe, 0);
    patchsizeoffsets_t *patch;
  /*  
    patch_t *cachedPatch = (patch_t*) W_CacheLumpNum(sprframe->lump[0] + _g->firstspritelump);
    patch_t ramPatch;
    if (isOnExternalFlash(cachedPatch))
    {
       spiFlashSetAddress((uint32_t) cachedPatch);
        spiFlashGetData(&ramPatch, 8);  // only first 4 shorts

        patch = &ramPatch;
    }
    else
    {
        patch = cachedPatch;
    }*/
    uint32_t lumpNum = sprframe->lump[0] + _g->firstspritelump;
    patch = &p_wad_immutable_flash_data->patchLumpSizeOffsets[lumpNum];
    // calculate edges of the shape
    fixed_t tx;
    tx = psp->sx - 160 * FRACUNIT;

    tx -= patch->leftoffset << FRACBITS;
    x1 = (centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS;

    tx += patch->width << FRACBITS;
    x2 = ((centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS) - 1;

    width = patch->width;
    topoffset = patch->topoffset << FRACBITS;

    // off the side
    if (x2 < 0 || x1 > SCREENWIDTH)
        return;

    // store information in a vissprite
    vis = &avis;
    vis->mobjflags = 0;
    // killough 12/98: fix psprite positioning problem
    vis->texturemid = (BASEYCENTER << FRACBITS) /* +  FRACUNIT/2 */- (psp->sy - topoffset);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= SCREENWIDTH ? SCREENWIDTH - 1 : x2;
    // proff 11/06/98: Added for high-res
    vis->scale = pspriteyscale;
    // vis->iscale = pspriteyiscale;

    if (flip)
    {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = (width << FRACBITS) - 1;
    }
    else
    {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);

    //vis->patch = cachedPatch;
    vis->lumpNum = lumpNum;
    /*
     if (_g->viewplayer->powers[pw_invisibility] > 4*32 || _g->viewplayer->powers[pw_invisibility] & 8)
     vis->colormap = NULL;                    // shadow draw
     else if (fixedcolormap)
     vis->colormap = fixedcolormap;           // fixed color
     else if (psp->state->frame & FF_FULLBRIGHT)
     vis->colormap = fullcolormap;            // full bright // killough 3/20/98
     else
     vis->colormap = R_LoadColorMap(lightlevel);  // local light
     */
    if (_g->viewplayer->powers[pw_invisibility] > 4 * 32 || (_g->viewplayer->powers[pw_invisibility] & 8))
        vis->colormap_idx = SHADOW_COLOR_MAP_IDX;                 // shadow draw
    else if (fixedcolormap)
        vis->colormap_idx = FIXED_COLOR_MAP_IDX;           // fixed color
    else if (psp->state->frame & FF_FULLBRIGHT)
        vis->colormap_idx = FULL_COLOR_MAP_IDX; // full bright // killough 3/20/98
    else
    {
        current_colormap_ptr = p_wad_immutable_flash_data->colormaps + 256 * scalelight[spritelights][0];
        vis->colormap_idx = CURRENT_COLOR_MAP_IDX;
    }
    R_DrawVisSprite(vis);
}

//
// R_DrawPlayerSprites
//

FASTFUN static void R_DrawPlayerSprites(void)
{
    int i,
            lightlevel = getMobjSubsector(_g->viewplayer->mo)->sector->lightlevel;
    pspdef_t *psp;

    int lightnum = (lightlevel >> LIGHTSEGSHIFT) + extralight;

    if (lightnum < 0)
        spritelights = 0;            //scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = LIGHTLEVELS - 1; //scalelight[LIGHTLEVELS-1];
    else
        spritelights = lightnum; //scalelight[lightnum];
    // clip to screen bounds

    mfloorclip = floorclip;
    mceilingclip = ceilingclip;
    mfloorceiling_ptr_size = sizeof(*floorclip);

 #if 0
    for (int i = 0; i < sizeof(floorclip) / sizeof(floorclip[0]); i++)
    {
        floorclip[i] = draw_stopy + 1;
        ceilingclip[i] = draw_starty - 1;
    }
 #else
    uint32_t *p_fc = (uint32_t*) floorclip;
    uint32_t *p_cc = (uint32_t*) ceilingclip;
    const uint16_t fcv = draw_stopy + 1;
    const uint16_t ccv = draw_starty - 1;
     
    for (i = 0; i < SCREENWIDTH / 2; i++)
    {
      *p_fc++ = fcv | (fcv << 16);
      *p_cc++ = ccv | (ccv << 16);
    }
 #endif

    // mfloorclip = screenheightarray;
    // mceilingclip = negonearray;
    // add all active psprites
    for (i = 0, psp = _g->viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
        if (psp->state)
            R_DrawPSprite(psp, lightlevel);
}

//
// R_SortVisSprites
//
// Rewritten by Lee Killough to avoid using unnecessary
// linked lists, and to use faster sorting algorithm.
//
// killough 9/2/98: merge sort

// 2021-02-13 next-hack changed using indexes.
FASTFUN static void msort(uint8_t *s, uint8_t *t, int n)
{
    if (n >= 16)
    {
        int n1 = n / 2, n2 = n - n1;
        uint8_t *s1 = s, *s2 = s + n1, *d = t;

        msort(s1, t, n1);
        msort(s2, t, n2);

        while (_g->vissprites[*s1].scale > _g->vissprites[*s2].scale ? (*d++ = *s1++, --n1) : (*d++ = *s2++, --n2));

        if (n2)
            memcpy(d, s2, n2 * sizeof(*d));
        else
            memcpy(d, s1, n1 * sizeof(*d));

        memcpy(s, t, n * sizeof(*s));
    }
    else
    {
        int i;
        for (i = 1; i < n; i++)
        {
            uint8_t temp = s[i];
            if (_g->vissprites[s[i - 1]].scale < _g->vissprites[temp].scale)
            {
                int j = i;
                while (_g->vissprites[(s[j] = s[j - 1])].scale < _g->vissprites[temp].scale && --j);
                s[j] = temp;
            }
        }
    }
}
FASTFUN static void R_SortVisSprites(void)
{
    int i = num_vissprite;

    if (i)
    {
        // 2021-02-13 next-hack. It is cheaper to store a byte index, instead of a 4 - byte ptr
        //  while (--i>=0)
        //      _g->vissprite_ptrs[i] = _g->vissprites+i;
        while (--i >= 0)
            _g->vissprite_indexes[i] = i;
        // killough 9/22/98: replace qsort with merge sort, since the keys
        // are roughly in order to begin with, due to BSP rendering.

        // msort(_g->vissprite_ptrs, _g->vissprite_ptrs + num_vissprite, num_vissprite);
        msort(_g->vissprite_indexes, _g->vissprite_indexes + num_vissprite, num_vissprite);
    }
}

//
// R_DrawMasked
//

FASTFUN static void R_DrawMasked(void)
{
    int i;
    drawseg_t *ds;
    drawseg_t *drawsegs = _g->drawsegs;

    R_SortVisSprites();

    // draw all vissprites back to front
    for (i = num_vissprite; --i >= 0;)
    {
        //2021-02-13 next-hack instead of pointers, we use idnexes. Slower, but saves ram
        //    R_DrawSprite(_g->vissprite_ptrs[i]);         // killough
        vissprite_t *vs = &_g->vissprites[_g->vissprite_indexes[i]];
#if DEBUGLIMITS
      if (!check_limits(vs))
        printf("VisSprite error num %d, tot %d, addr 0x%08X\r\n", i, num_vissprite , (uint32_t)vs);
#endif
        R_DrawSprite(vs);
    }

    // render any remaining masked mid textures

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):
    for (ds = ds_p; ds-- > drawsegs;)  // new -- killough
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange(ds, ds->x1, ds->x2);

    R_DrawPlayerSprites();
}

//
// R_DrawSpan
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//

FASTFUN inline static void R_DrawSpanPixel(pixel *dest, const byte *source, const byte *colormap, unsigned int position)
{
#ifdef HIGHRES
    *dest = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
#else
    unsigned int color = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];

    *dest = (color | (color << 8));
#endif
}

FASTFUN static inline void R_DrawSpan(unsigned int y, unsigned int x1, unsigned int x2, const draw_span_vars_t *dsvars)
{
    unsigned int count = (x2 - x1);
    const byte *source = dsvars->source;
    const byte *colormap;
#if CACHE_ALL_COLORMAP_TO_RAM
    colormap = dsvars->colormap;
#else
    // 2021/09/18 next-hack this will improve a lot plane drawing due to better RAM speed
    if (currentCachedColorMap == dsvars->colormap)
    {
        colormap = colorMap;
    }
    else if (x2 - x1 > MIN_PIXEL_TO_DRAW_FOR_CACHING_COLORMAP)
    {
        memcpy(colorMap, dsvars->colormap, 256);
        currentCachedColorMap = (uint8_t*) dsvars->colormap;
        colormap = colorMap;
    }
    else
    {
        colormap = dsvars->colormap;
    }
#endif
    pixel *dest = ((pixel*) drawvars.byte_topleft) + ScreenYToOffset(y) + x1;
    uint32_t * qDest = (uint32_t*) dest;
    const unsigned int step = dsvars->step;
    unsigned int position = dsvars->position;

    unsigned int l = (count >> 4);
    while (l--)
    {
    #if 0
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;

        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;

        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;

        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
        R_DrawSpanPixel((pixel*) dest, source, colormap, position);
        dest += 1;
        position += step;
    #else
      uint32_t quadPix;
      
        quadPix = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 8;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 16;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 24;
        position += step;
        *qDest++ = quadPix;

        quadPix = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 8;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 16;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 24;
        position += step;
        *qDest++ = quadPix;

        quadPix = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 8;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 16;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 24;
        position += step;
        *qDest++ = quadPix;

        quadPix = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 8;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 16;
        position += step;
        quadPix |= (colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]]) << 24;
        position += step;
        *qDest++ = quadPix;

    #endif
    }
    dest = (pixel*) qDest;
    unsigned int r = (count & 15);

    switch (r)
    {
        case 15:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 14:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 13:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 12:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 11:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 10:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 9:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 8:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 7:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 6:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 5:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 4:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 3:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 2:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            dest += 1;
            position += step;
            // fall through, no break
        case 1:
            R_DrawSpanPixel((pixel*) dest, source, colormap, position);
            // fall through, no break
    }
}

FASTFUN static void R_MapPlane(unsigned int y, unsigned int x1, unsigned int x2, draw_span_vars_t *dsvars)
{
    const fixed_t distance = FixedMul(planeheight, yslope[y]);

    dsvars->step = ((FixedMul(distance, basexscale) << 10) & 0xffff0000) | ((FixedMul(distance, baseyscale) >> 6) & 0x0000ffff);

    fixed_t length = FixedMul(distance, distscale[x1]);
    angle_t angle = (viewangle + xtoviewangle[x1]) >> ANGLETOFINESHIFT;

    // killough 2/28/98: Add offsets
    unsigned int xfrac = viewx + FixedMul(finecosine[angle], length);
    unsigned int yfrac = -viewy - FixedMul(finesine[angle], length);

    dsvars->position = ((xfrac << 10) & 0xffff0000) | ((yfrac >> 6) & 0x0000ffff);
    int index;
    if (fixedcolormap)
    {
        dsvars->colormap = fixedcolormap;
    }
    else
    {
        index = distance >> LIGHTZSHIFT;

        if (index >= MAXLIGHTZ)
            index = MAXLIGHTZ - 1;

        dsvars->colormap = p_wad_immutable_flash_data->colormaps + 256 * zlight[planezlight][index];
    }
    R_DrawSpan(y, x1, x2, dsvars);
}
#ifdef OPTIMIZESIZE
#pragma GCC optimize ("Os")
#endif
//
// R_MakeSpans
//

FASTFUN static void R_MakeSpans(int x, unsigned int t1, unsigned int b1, unsigned int t2, unsigned int b2, draw_span_vars_t *dsvars)
{
    for (; t1 < t2 && t1 <= b1; t1++)
    {
        //if (t1 >= viewheight)
        //  printf("t1 > 96 %d\r\n", t1);
        R_MapPlane(t1, spanstart[t1], x, dsvars);
    }

    for (; b1 > b2 && b1 >= t1; b1--)
    {
        //if (b1 >= viewheight)
        //  printf("b1 > 96 %d\r\n", b1);
        R_MapPlane(b1, spanstart[b1], x, dsvars);
    }
    while (t2 < t1 && t2 <= b2)
    {

        //if (t2 >= viewheight)
        //  printf("t2 > 96 %d\r\n", t2);
        spanstart[t2++] = x;
    }
    while (b2 > b1 && b2 >= t2)
    {
        //if (b2 >= viewheight)
        //  printf("b2 > 96 %d\r\n", b2);
        spanstart[b2--] = x;
    }
}

// New function, by Lee Killough

FASTFUN static void R_DoDrawPlane(visplane_t *pl)
{
    register int x;
    draw_column_vars_t dcvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    if (pl->minx <= pl->maxx)
    {
        if (pl->picnum == _g->skyflatnum)
        { // sky flat

            // Normal Doom sky, only one allowed per level
            dcvars.texturemid = skytexturemid;    // Default y-offset

            /* Sky is always drawn full bright, i.e. colormaps[0] is used.
             * Because of this hack, sky is not affected by INVUL inverse mapping.
             * Until Boom fixed this. Compat option added in MBF. */

            if (!(dcvars.colormap = fixedcolormap))
                dcvars.colormap = fullcolormap;          // killough 3/20/98

            // proff 09/21/98: Changed for high-res
            dcvars.iscale = skyiscale;
            // sky drawing not optimized, because it is likely to be in the internal flash.
            const texture_t *tex = R_GetOrLoadTexture(_g->skytexture);
            cachePatchColumnOffsetData((patch_t *) tex->patches[0].patch, tex->width,  pl->minx, pl->maxx);
            uint8_t columnData[MAX_COLUMN_DATA];
            int oldXc = 0xFF000000;
            const column_t *column = column; // self assignment to suppress warning without code generation
            // killough 10/98: Use sky scrolling offset
            for (x = pl->minx; (dcvars.x = x) <= pl->maxx; x++)
            {
                if ((dcvars.yl = pl->top[x]) != -1 && dcvars.yl <= (dcvars.yh = pl->bottom[x])) // dropoff overflow
                {
                    int xc = ((viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT);
                    if (oldXc != xc)
                    {
                        oldXc = xc;
                        column = R_GetColumn(tex, xc, columnData);
                    }
                    dcvars.source = (const byte*) column + 3;
                    R_DrawColumn(&dcvars);
                }
            }
        }
        else
        {     // regular flat

            draw_span_vars_t dsvars;
            uint8_t light;
            dsvars.source = W_CacheLumpNum(_g->firstflat + flattranslation[pl->picnum]);
            if (isOnExternalFlash(dsvars.source))
            {
                printf("Error, found a lump %d at address 0x%08X, picnum %d, name %s \r\n", (unsigned int) (_g->firstflat + flattranslation[pl->picnum]), (unsigned int) W_CacheLumpNum(_g->firstflat + flattranslation[pl->picnum]), flattranslation[pl->picnum], W_GetNameForNum(_g->firstflat + flattranslation[pl->picnum]));
                while (1);
            }
// NOTE: removed!
            //dsvars.colormap = R_LoadColorMap(pl->lightlevel);

            planeheight = D_abs(pl->height - viewz);

            light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;

            if (light >= LIGHTLEVELS)
                light = LIGHTLEVELS - 1;

            if (light < 0)
                light = 0;

            planezlight = light;

            const int stop = pl->maxx + 1;

            pl->top[pl->minx - 1] = pl->top[stop] = 0xff; // dropoff overflow

            for (x = pl->minx; x <= stop; x++)
            {
                int t1 = pl->top[x - 1];
                int b1 = pl->bottom[x - 1];
                int t2 = pl->top[x];
                int b2 = pl->bottom[x];

                R_MakeSpans(x, t1, b1, t2, b2, &dsvars);
            }
        }
    }
}

//*******************************************

//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
// killough 5/2/98: reformatted, cleaned up
// CPhipps - moved here from r_main.c

FASTFUN static fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
    int anglea = ANG90 + (visangle - viewangle);
    int angleb = ANG90 + (visangle - rw_normalangle);

    int den = FixedMul(rw_distance, finesine[anglea >> ANGLETOFINESHIFT]);

// proff 11/06/98: Changed for high-res
    fixed_t num = FixedMul(projectiony, finesine[angleb >> ANGLETOFINESHIFT]);

    return den > num >> 16 ?
            (num = FixedDiv(num, den)) > 64 * FRACUNIT ? 64 * FRACUNIT :
            num < 256 ? 256 : num : 64 * FRACUNIT;
}

//
// R_NewVisSprite
//

static vissprite_t* R_NewVisSprite(void)
{
    if (num_vissprite >= MAXVISSPRITES)
    {
#ifdef RANGECHECK
        I_Error("Vissprite overflow.\r\n");
#endif
        return NULL;
    }

    return _g->vissprites + num_vissprite++;
}

//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//

FASTFUN static void R_ProjectSprite(mobj_t *thing, int lightlevel)
{
    const fixed_t fx = thing->x;
    const fixed_t fy = thing->y;
    const fixed_t fz = thing->z;

    const fixed_t tr_x = fx - viewx;
    const fixed_t tr_y = fy - viewy;

    const fixed_t tz = FixedMul(tr_x, viewcos) - (-FixedMul(tr_y, viewsin));

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    //Too far away. Always draw Cyberdemon and Spiderdemon. They are big sprites!
    if ((tz > MAXZ) && (thing->type != MT_CYBORG) && (thing->type != MT_SPIDER))
        return;

    fixed_t tx = -(FixedMul(tr_y, viewcos) + (-FixedMul(tr_x, viewsin)));

    // too far off the side?
    if (D_abs(tx) > (tz << 2))
        return;

    // decide which patch to use for sprite relative to player
    const spritedef_t *sprdef = &p_wad_immutable_flash_data->sprites[thing->sprite];
    const spriteframe_t *sprframe = (const spriteframe_t*) &(getSpriteFrames(sprdef)[thing->frame & FF_FRAMEMASK]);

    unsigned int rot = 0;

    if (sprframe->rotate)
    {
        // choose a different rotation based on player view
        angle_t ang = R_PointToAngle(fx, fy);
        rot = (ang - thing->angle + (unsigned) (ANG45 / 2) * 9) >> 29;
    }

    const boolean flip = (boolean) SPR_FLIPPED(sprframe, rot);
    uint32_t lumpNum = sprframe->lump[rot] + _g->firstspritelump;
    


    patchsizeoffsets_t *patch;
    patch = &p_wad_immutable_flash_data->patchLumpSizeOffsets[lumpNum];
/*
    patch_t *p_patch = (patch_t*) W_CacheLumpNum(lumpNum);
    patch_t tpatch;
    if (isOnExternalFlash(p_patch))
    {
        spiFlashSetAddress((uint32_t) p_patch);
        spiFlashGetData(&tpatch, 8);
        patch = &tpatch;
    }
    else
    {
        patch = p_patch;
    }*/

    /* calculate edges of the shape
     * cph 2003/08/1 - fraggle points out that this offset must be flipped
     * if the sprite is flipped; e.g. FreeDoom imp is messed up by this. */
    if (flip)
        tx -= (patch->width - patch->leftoffset) << FRACBITS;
    else
        tx -= patch->leftoffset << FRACBITS;

    const fixed_t xscale = FixedDiv(projection, tz);

    fixed_t xl = (centerxfrac + FixedMul(tx, xscale));

    // off the side?
    if (xl > (SCREENWIDTH << FRACBITS))
        return;

    fixed_t xr = (centerxfrac + FixedMul(tx + (patch->width << FRACBITS), xscale)) - FRACUNIT;

    // off the side?
    if (xr < 0)
        return;

    //Too small.
    if (xr <= (xl + (FRACUNIT >> 2)))
        return;

    const int x1 = (xl >> FRACBITS);
    const int x2 = (xr >> FRACBITS);

    // store information in a vissprite
    vissprite_t *vis = R_NewVisSprite();

    //No more vissprites.
    if (!vis)
        return;
// 2021-02-13 next-hack: useless having to store 32 bit, when you need only 2...
    vis->mobjflags = thing->flags >> MF_TRANSSHIFT;

    // proff 11/06/98: Changed for high-res
    vis->scale = FixedDiv(projectiony, tz);
    //vis->iscale = FixedReciprocal(vis->scale);
    //vis->patch = p_patch;
    vis->lumpNum = lumpNum;
    vis->gx = fx;
    vis->gy = fy;
    vis->gz = fz;
    vis->gzt = fz + (patch->topoffset << FRACBITS);          // killough 3/27/98
    vis->texturemid = vis->gzt - viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= SCREENWIDTH ? SCREENWIDTH - 1 : x2;

    const fixed_t iscale = FixedDiv(FRACUNIT, xscale);

    if (flip)
    {
        vis->startfrac = (patch->width << FRACBITS) - 1;
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);

    // get light level
    /*    if (thing->flags & MF_SHADOW)

     vis->colormap = NULL;             // shadow draw
     else if (fixedcolormap)
     vis->colormap = fixedcolormap;      // fixed map
     else if (thing->frame & FF_FULLBRIGHT)
     vis->colormap = fullcolormap;     // full bright  // killough 3/20/98
     else
     {      // diminished light
     vis->colormap = R_ColourMap(lightlevel);
     }*/
    // get light level
    if (thing->flags & MF_SHADOW)
        vis->colormap_idx = SHADOW_COLOR_MAP_IDX;             // shadow draw
    else if (fixedcolormap)
        vis->colormap_idx = FIXED_COLOR_MAP_IDX;      // fixed map
    else if (thing->frame & FF_FULLBRIGHT)
        vis->colormap_idx = FULL_COLOR_MAP_IDX; // full bright  // killough 3/20/98
    else
    {      // diminished light

        const lighttable_t *cm = R_ColourMap(lightlevel);
        if (cm == fixedcolormap)
            vis->colormap_idx = FIXED_COLOR_MAP_IDX;
        else
        {
            //uint32_t n = (((uint32_t) cm) - ((uint32_t) fullcolormap)) / 256;
            int index = xscale >> LIGHTSCALESHIFT;

            if (index >= MAXLIGHTSCALE)
            {
                index = MAXLIGHTSCALE - 1;
            }
            //vis->colormap_idx = n;
            vis->colormap_idx = scalelight[spritelights][index];
        }
    }
}

//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
static void R_AddSprites(subsector_t *subsec, int lightlevel)
{
    sector_t *sec = subsec->sector;
    mobj_t *thing;

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.

    if (sec->validcount == _g->validcount)
        return;

    // Well, now it will be done.
    sec->validcount = _g->validcount;

    // light

    int lightnum = (lightlevel >> LIGHTSEGSHIFT) + extralight;

    if (lightnum < 0)
        spritelights = 0;  //scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = LIGHTLEVELS - 1; //scalelight[LIGHTLEVELS-1];
    else
        spritelights = lightnum; // scalelight[lightnum];

    // Handle all things in sector.

    for (thing = getSectorThingList(sec); thing; thing = getSNext(thing))
        R_ProjectSprite(thing, lightlevel);
}

//
// R_FindPlane
//
// killough 2/28/98: Add offsets

// New function, by Lee Killough

static visplane_t* new_visplane(unsigned hash)
{
    static int allocatedVp = 0;
    visplane_t *check = _g->freetail;

    if (!check)
    {
        allocatedVp++;
        check = Z_Calloc(1, sizeof(visplane_t), PU_LEVEL, NULL);
        //printf("NewVispl. %d\r\n", allocatedVp);
    }
    else
    {
        if (!(_g->freetail = _g->freetail->next))
            _g->freehead = &_g->freetail;
    }

    check->next = _g->visplanes[hash];
    _g->visplanes[hash] = check;

    return check;
}

static visplane_t* R_FindPlane(fixed_t height, int picnum, int lightlevel)
{
    visplane_t *check;
    unsigned hash;                      // killough

    if (picnum == _g->skyflatnum)
        height = lightlevel = 0;    // killough 7/19/98: most skies map together

    // New visplane algorithm uses hash table -- killough
    hash = visplane_hash(picnum, lightlevel, height);

    for (check = _g->visplanes[hash]; check; check = check->next)  // killough
        if (height == check->height && picnum == check->picnum && lightlevel == check->lightlevel)
            return check;

    check = new_visplane(hash);         // killough

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH; // Was SCREENWIDTH -- killough 11/98
    check->maxx = -1;

    memset(check->top, UINT_MAX, sizeof(check->top));

    check->modified = 0;

    return check;
}

/*
 * R_DupPlane
 *
 * cph 2003/04/18 - create duplicate of existing visplane and set initial range
 */
static visplane_t* R_DupPlane(const visplane_t *pl, int start, int stop)
{
    unsigned hash = visplane_hash(pl->picnum, pl->lightlevel, pl->height);
    visplane_t *new_pl = new_visplane(hash);

    new_pl->height = pl->height;
    new_pl->picnum = pl->picnum;
    new_pl->lightlevel = pl->lightlevel;
    new_pl->minx = start;
    new_pl->maxx = stop;

    memset(new_pl->top, UINT_MAX, sizeof(new_pl->top));

    new_pl->modified = false;

    return new_pl;
}

//
// R_CheckPlane
//
static visplane_t* R_CheckPlane(visplane_t *pl, int start, int stop)
{
    int intrl, intrh, unionl, unionh, x;

    if (start < pl->minx)
        intrl = pl->minx, unionl = start;
    else
        unionl = pl->minx, intrl = start;

    if (stop > pl->maxx)
        intrh = pl->maxx, unionh = stop;
    else
        unionh = pl->maxx, intrh = stop;

    for (x = intrl; x <= intrh && pl->top[x] == 0xff; x++) // dropoff overflow
        ;

    if (x > intrh)
    { /* Can use existing plane; extend range */
        pl->minx = unionl;
        pl->maxx = unionh;
        return pl;
    }
    else
        /* Cannot use existing plane; create a new one */
        return R_DupPlane(pl, start, stop);
}
#if MULTIPATCH_TEXTURE_SUPPORT
static inline void R_DrawColumnInCache(const column_t *patch, byte *cache, int originy, int cacheheight)
{
    while (patch->topdelta != 0xff)
    {
        const byte *source = (const byte*) patch + 3;
        int count = patch->length;
        int position = originy + patch->topdelta;

        if (position < 0)
        {
            count += position;
            position = 0;
        }

        if (position + count > cacheheight)
            count = cacheheight - position;

        if (count > 0)
            memcpy(cache + position, source, count);

        patch = (const column_t*) ((const byte*) patch + patch->length + 4);
    }
}
#endif

/*
 * Draw a column of pixels of the specified texture.
 * If the texture is simple (1 patch, full height) then just draw
 * straight from const patch_t*.
 */
#if MULTIPATCH_TEXTURE_SUPPORT
FASTFUN static inline const byte* R_ComposeColumn(const texture_t *tex, int texcolumn, unsigned int iscale)
{
    // TODO: clean up this mess
/*    if (isOnExternalFlash(tex))
    {
        printf("Error, found a texture at address 0x%08X\r\n", (unsigned int) tex);
        while (1);
    }*/
#if ENABLE_MIPMAP
    int colmask = 0xfffe;
    if (tex->width > 8)
    {
        if (iscale > (4 << FRACBITS))
            colmask = 0xfff0;
        else if (iscale > (3 << FRACBITS))
            colmask = 0xfff8;
        else if (iscale > (2 << FRACBITS))
            colmask = 0xfffc;
    }

    const int xc = (texcolumn & colmask) & tex->widthmask;
#else
    // 2021/06/05 next-hack: since we do not implement any cache, there is no point of mip-mapping
    // in fact mip-mapping was probably added by doomhack to increase hit/miss ratio.
    const int xc = (texcolumn & tex->widthmask);
#endif
    unsigned int i = 0;
    unsigned int patchcount = tex->patchcount;

    do
    {
        // note: all texetures and patch addresses are cached in flash.
        const texpatch_t *patch = &tex->patches[i];
        const patch_t *realpatch = patch->patch;
        const int x1 = patch->originx;

        if (xc < x1)
            continue;
        short width;
        if (isOnExternalFlash(realpatch))
        {
            spiFlashSetAddress((uint32_t) &realpatch->width);
            width = spiFlashGetShort();
        }
        else
        {
            width = realpatch->width;
        }
        const int x2 = x1 + width;

        if (xc < x2)
        {
           #if 0
            const column_t *patchcol;
            if (isOnExternalFlash(realpatch))
            {
                // 2021-08-13 next-hack: we can directly load the column data to the dest column buffer
                uint32_t ofs = realpatch->columnofs[xc - x1];
                patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & ofs));
                unsigned int length = ((ofs >> 24) - 8);
                int position = patch->originy + patchcol->topdelta;

               if (position < 0)
                {
                    length += position;
                    position = 0;
                }

                if (position + length > tex->height)
                    length = tex->height - position;


                NRF_QSPI->READ.CNT = length;
                NRF_QSPI->READ.SRC = (uint32_t) (((uint8_t*)patchcol) + 3);
                NRF_QSPI->READ.DST = (uint32_t) ((uint8_t*) columnCacheBuffer + position);;
                NRF_QSPI->EVENTS_READY = 0;
                NRF_QSPI->TASKS_READSTART = 1;
                while (NRF_QSPI->EVENTS_READY == 0);
                NRF_QSPI->EVENTS_READY = 0;    
            }
            else
            {
               // 2021-08-13 next-hack: if the patch is in internal flash, we copy it to the dest column buffer by words and not bytes. Huge boost!
                patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & realpatch->columnofs[xc - x1]));
                
                unsigned int length = patchcol->length; //((realpatch->columnofs[xc - x1] >> 24) - 8);
                //printf("L %d I %d\r\n", patchcol->length ,((realpatch->columnofs[xc - x1] >> 24) - 8));
                int position = patch->originy + patchcol->topdelta;


                if (position < 0)
                {
                    length += position;
                    position = 0;
                }

                if (position + length > tex->height)
                    length = tex->height - position;
                uint32_t *d = (uint32_t* )((uint8_t*) columnCacheBuffer + position);
                uint32_t *s = (uint32_t* )(((uint8_t*)patchcol) + 3);
                for (int j = 0; j < length >> 2; j++)
                  *d++ = *s++;
            }

           #elif 1
            const column_t *patchcol;
            if (isOnExternalFlash(realpatch))
            {
                // in most of the cases, the patch will be a single post, multiple of 4 bytes and 4-bytes aligned.
                // therefore we can directly draw to the buffer. 
                // 2021-08-13 next-hack: we can directly load the column data to the dest column buffer
                uint32_t ofs = realpatch->columnofs[xc - x1];
                patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & ofs));
                unsigned int columnLength = ((ofs >> 24) - 8);
                unsigned int postLength = patchcol->length;
                bool done = false;
                if (postLength == columnLength)
                {
                  unsigned int length = postLength;
                  int position = patch->originy + patchcol->topdelta;

                 if (position < 0)
                  {
                      length += position;
                      position = 0;
                  }

                  if (position + length > tex->height)
                      length = tex->height - position;

                  if ((position & 3) == 0 && (length & 3) == 0)
                  {
                    done = true;
                    NRF_QSPI->READ.CNT = length;
                    NRF_QSPI->READ.SRC = (uint32_t) (((uint8_t*)patchcol) + 3);
                    NRF_QSPI->READ.DST = (uint32_t) ((uint8_t*) columnCacheBuffer + position);;
                    NRF_QSPI->EVENTS_READY = 0;
                    NRF_QSPI->TASKS_READSTART = 1;
                    while (NRF_QSPI->EVENTS_READY == 0);
                    NRF_QSPI->EVENTS_READY = 0;   
                  }
                }
                if (!done)
                {    
                  // we were not able to optimize. What a pity.
                  uint8_t columnData[MAX_COLUMN_DATA];
                  const column_t *patchcol;
                  patchcol = getColumnData(realpatch, xc - x1, columnData);
                  R_DrawColumnInCache(patchcol, columnCacheBuffer, patch->originy, tex->height);
                }
            }
            else
            {
               // 2021-08-13 next-hack: if the patch is in internal flash, we copy it to the dest column buffer by words and not bytes. Huge boost!
                patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & realpatch->columnofs[xc - x1]));
                
                unsigned int columnLength = ((realpatch->columnofs[xc - x1] >> 24) - 8);
                int position = patch->originy + patchcol->topdelta;

                unsigned int postLength = patchcol->length;
                bool done = false;
                if (postLength == columnLength)
                {
                  unsigned int length = postLength;
                  if (position < 0)
                  {
                      length += position;
                      position = 0;
                  }

                  if (position + length > tex->height)
                      length = tex->height - position;

                  if ((position & 3) == 0 && (length & 3) == 0)
                  {
                    done = true;
                    uint32_t *d = (uint32_t* )((uint8_t*) columnCacheBuffer + position);
                    uint32_t *s = (uint32_t* )(((uint8_t*)patchcol) + 3);
                    for (int j = 0; j < length >> 2; j++)
                      *d++ = *s++;
                  }
                }
                if (!done)
                {
                  patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & realpatch->columnofs[xc - x1]));
                  R_DrawColumnInCache(patchcol, columnCacheBuffer, patch->originy, tex->height);
                }


            }
            #else
            uint8_t columnData[MAX_COLUMN_DATA];
            const column_t *patchcol;
            if (isOnExternalFlash(realpatch))
            {
                patchcol = getColumnData(realpatch, xc - x1, columnData);
            }
            else
            {
                patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & realpatch->columnofs[xc - x1]));
            }
            // TODO: not sure if this does not overflow.
            R_DrawColumnInCache(patchcol, columnCacheBuffer, patch->originy, tex->height);
            #endif
        }

    } while (++i < patchcount);
    return columnCacheBuffer;
}
#endif
#if 0
uint8_t composeColumnBuffer[2][MAX_COLUMN_DATA];
FASTFUN static inline const byte* R_ComposeColumnNonBlocking(const texture_t *tex, int texcolumn)
{
    // TODO: clean up this mess
    const int xc = (texcolumn & tex->widthmask);
    unsigned int i = 0;
    unsigned int patchcount = tex->patchcount;
    bool wasOnExternalFlash = false;
    uint32_t bufferNumber = 0;
    bool validColumn = false;
    int  patch_originy, tex_height;
    column_t *patchcol, *next_patchcol;
    texpatch_t *patch;

    // get the first column
    do
    {
        // note: all texetures and patch addresses are cached in flash.
        patch = &tex->patches[i];
        const patch_t *realpatch = patch->patch;
        const int x1 = patch->originx;

        if (xc < x1)
            continue;
        short width;
        if (isOnExternalFlash(realpatch))
        {
            spiFlashSetAddress((uint32_t) &realpatch->width);
            width = spiFlashGetShort();
        }
        else
        {
            width = realpatch->width;
        }
        const int x2 = x1 + width;

        if (xc < x2)
        {
            uint8_t columnData[MAX_COLUMN_DATA];
            const column_t *patchcol;
            if (isOnExternalFlash(realpatch))
            {
                getColumnDataNonBlocking(realpatch, xc - x1, composeColumnBuffer[bufferNumber]);
                patchcol  = composeColumnBuffer[bufferNumber];
                wasOnExternalFlash = true;
            }
            else
            {
                patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & realpatch->columnofs[xc - x1]));
                wasOnExternalFlash = false;
            }
            i++;
            validColumn = true;
            break;
        }
    } while (++i < patchcount);
    // now we got the first good patch column
    if (validColumn)
    {
      patch_originy = patch->originy;
      tex_height = tex->height;
      // now the next ones
      do
      {
          // note: all texetures and patch addresses are cached in flash.
          patch = &tex->patches[i];
          const patch_t *realpatch = patch->patch;
          const int x1 = patch->originx;

          if (xc < x1)
          {

              continue;
          }
          short width;
          if (isOnExternalFlash(realpatch))
          {
              // before accessing the external flash, wait
              if (wasOnExternalFlash)
              {
                while (!NRF_QSPI->EVENTS_READY);
                NRF_QSPI->EVENTS_READY = 0;
              }
              spiFlashSetAddress((uint32_t) &realpatch->width);
              width = spiFlashGetShort();
          }
          else
          {
              width = realpatch->width;
          }
          const int x2 = x1 + width;

          if (xc < x2)
          {
              if (isOnExternalFlash(realpatch))
              {
                  getColumnDataNonBlocking(realpatch, xc - x1, composeColumnBuffer[1 - bufferNumber]);
                  next_patchcol  = composeColumnBuffer[1 - bufferNumber];
                  wasOnExternalFlash = true;
              }
              else
              {
                  next_patchcol = (const column_t*) ((const byte*) realpatch + (0xFFFFFF & realpatch->columnofs[xc - x1]));
                  wasOnExternalFlash = false;
              }
              R_DrawColumnInCache(patchcol, columnCacheBuffer, patch_originy, tex_height);
              validColumn = true;
              patch_originy = patch->originy;
              tex_height = tex->height;
              patchcol = next_patchcol;
              bufferNumber =  1 - bufferNumber;
          }
/*          else
          {
              if (validColumn)
                R_DrawColumnInCache(patchcol, columnCacheBuffer, patch_originy, tex_height);
              validColumn = false;
          }*/
      } while (++i < patchcount);

      if (wasOnExternalFlash)
      {
        while (!NRF_QSPI->EVENTS_READY);
        NRF_QSPI->EVENTS_READY = 0;
      }
      // draw last patch? only if valid
      if (validColumn)
        R_DrawColumnInCache(patchcol, columnCacheBuffer, patch_originy, tex_height);
    }
    return columnCacheBuffer;
}
#endif
#if MULTIPATCH_TEXTURE_SUPPORT
FASTFUN inline static void R_DrawSegTextureColumn(const texture_t *tex, int texcolumn, draw_column_vars_t *dcvars, uint8_t *columnData)
{
    if (tex->patchcount == 1)
    {
#if 0
        const column_t *column = R_GetColumn(tex, texcolumn, columnData);
#else
        // very minimal performance increase. Well, better than nothing.
        const column_t *column;
        if ((dcvars->yh - dcvars->yl) > 8) 
        //if (dcvars->iscale > 0x10000)  
        {
            column = R_GetColumn(tex, texcolumn, columnData);
        }
        else
        {
            const unsigned int widthmask = tex->widthmask;
            const int xc = texcolumn & widthmask;
            const patch_t *patch = tex->patches[0].patch;
            column = (column_t*) ((byte*) patch + (0xFFFFFF & patch->columnofs[xc]));
        }
#endif
        dcvars->source = (const byte*) column + 3;
    }
    else if (tex->overlapped == 0)
    {
        const column_t *column = R_GetColumn(tex, texcolumn, columnData);
        dcvars->source = (const byte*) column + 3;
    }
    else
    {
        dcvars->source = R_ComposeColumn(tex, texcolumn, dcvars->iscale);
    }
    R_DrawColumn(dcvars);
}
#endif
//
FASTFUN inline static void* R_GetSegTextureColumnNonBlocking(const texture_t *tex, int texcolumn,  uint8_t *columnData, bool *wasOnExternalFlash,   fixed_t iscale)
{
#if MULTIPATCH_TEXTURE_SUPPORT

    if (tex->overlapped == 0)
    {
        const column_t *column = R_GetColumnNonBlocking(tex, texcolumn, columnData, wasOnExternalFlash);
        return (byte*) column + 3;
    }
    else
    {
        return (void*) R_ComposeColumn(tex, texcolumn, iscale);
    }
#else
    const column_t *column = R_GetColumnNonBlocking(tex, texcolumn, columnData, wasOnExternalFlash);
    return (byte*) column + 3;
#endif
}
//
typedef union
{
    struct 
    {
        uint8_t topTextureLoadByDma;
        uint8_t bottomTextureLoadByDma;
    };
    uint16_t pendingDmaOperation;
} dmaOperation_t;

FASTFUN inline static void* R_GetSinglePatchTextureColumnNonBlocking(const texture_t *texture, int texcolumn,  uint8_t *columnData,   fixed_t iscale, dmaOperation_t *dmaOperation, uint32_t isTop)
{
    const unsigned int widthmask = texture->widthmask;

    const int xc = texcolumn & widthmask;
    //simple texture.
    const patch_t *patch = texture->patches[0].patch;
    if (!isOnExternalFlash(patch))
    {
        return ((byte*) patch + (0xFFFFFF & patch->columnofs[xc]) + 3);
    }
    else
    {   
        // was there any pending DMA operation ? 
        if (dmaOperation->pendingDmaOperation)
        {
            while(!NRF_QSPI->EVENTS_READY);
            NRF_QSPI->EVENTS_READY = 0;
        }
#define GET_SIZE_FROM_TEXTURE_DATA 1
#if GET_SIZE_FROM_TEXTURE_DATA

        NRF_QSPI->READ.CNT = texture->height;
        NRF_QSPI->READ.SRC = (uint32_t) patch 
                                        +  8                            // first 8 bytes are patch data
                                        + 4 * texture->width            // column offset data
                                        + xc * (texture->height  + 5)   // each column has topdelta, length, pad, <texture->height> data, pad, invalid topdelta.  
                                        + 3;                            // point to data
        NRF_QSPI->READ.DST = (uint32_t) columnData;
        NRF_QSPI->EVENTS_READY = 0;
        NRF_QSPI->TASKS_READSTART = 1;
#else
        uint32_t co = getColumnOffsetData((patch_t*) patch, xc);
        uint32_t cnt = co >> 24;
        co = co & 0xFFFFFF;
        NRF_QSPI->READ.CNT = cnt - 4;
        NRF_QSPI->READ.SRC = (uint32_t) patch + co + 3;
        NRF_QSPI->READ.DST = (uint32_t) columnData;
        NRF_QSPI->EVENTS_READY = 0;
        NRF_QSPI->TASKS_READSTART = 1;
#endif
        if (isTop)
        {
            dmaOperation->pendingDmaOperation = 0x0001;
        }
        else
        {
            dmaOperation->pendingDmaOperation = 0x0100;
        }
        return columnData;
    }
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//

#define HEIGHTBITS 12
#define HEIGHTUNIT (1<<HEIGHTBITS)
static inline void R_RenderSegLoop(int rw_x)
{
    draw_column_vars_t dcvars;
    fixed_t texturecolumn = 0;   // shut up compiler warning

    //R_SetDefaultDrawColumnVars(&dcvars);
// NOTE: removed!
//    dcvars.colormap = R_LoadColorMap(rw_lightlevel);
#if 0
    uint8_t columnData[MAX_COLUMN_DATA];
    int oldTextureColumn = 0xFF000000;  // invalid value
    for (; rw_x < rw_stopx; rw_x++)
    {
        // mark floor / ceiling areas

        int yh = bottomfrac >> HEIGHTBITS;
        int yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

        int cc_rwx = ceilingclip[rw_x];
        int fc_rwx = floorclip[rw_x];

        // no space above wall?
        int bottom, top = cc_rwx + 1;

        if (yl < top)
            yl = top;

        if (yl < draw_starty)
            yl = draw_starty;

        if (yh > draw_stopy)
            yh = draw_stopy;

        if (markceiling)
        {
            bottom = yl - 1;

            if (bottom >= fc_rwx)
                bottom = fc_rwx - 1;

            if (top <= bottom)
            {
                ceilingplane->top[rw_x] = top;
                ceilingplane->bottom[rw_x] = bottom;
                ceilingplane->modified = 1;
            }
            // SoM: this should be set here
            cc_rwx = bottom;
        }

        bottom = fc_rwx - 1;
        if (yh > bottom)
            yh = bottom;

        if (markfloor)
        {

            top = yh < cc_rwx ? cc_rwx : yh;

            if (++top <= bottom)
            {
                floorplane->top[rw_x] = top;
                floorplane->bottom[rw_x] = bottom;
                floorplane->modified = 1;
            }
            // SoM: This should be set here to prevent overdraw
            fc_rwx = top;
        }

        // texturecolumn and lighting are independent of wall tiers
        if (segtextured)
        {
            // calculate texture offset
            angle_t angle = (rw_centerangle + xtoviewangle[rw_x]) >> ANGLETOFINESHIFT;

            texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);

            texturecolumn >>= FRACBITS;

            // calculate lighting
            int index = rw_scale >> LIGHTSCALESHIFT;

            if (index >= MAXLIGHTSCALE)
                index = MAXLIGHTSCALE - 1;

            dcvars.colormap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];

            dcvars.x = rw_x;

            dcvars.iscale = FixedReciprocal((unsigned) rw_scale);
        }

        // draw the wall tiers
        if (midtexture)
        {

            dcvars.yl = yl;     // single sided line
            dcvars.yh = yh;
            dcvars.texturemid = rw_midtexturemid;
            //
            const texture_t *tex = R_GetOrLoadTexture(midtexture);
            if (texturecolumn != oldTextureColumn || tex->overlapped)
            {
                oldTextureColumn = texturecolumn;
                R_DrawSegTextureColumn(tex, texturecolumn, &dcvars, columnData);
            }
            R_DrawColumn(&dcvars);
            cc_rwx = draw_stopy + 1;
            fc_rwx = draw_starty - 1;
        }
        else
        {

            // two sided line
            if (toptexture)
            {
                // top wall
                int mid = pixhigh >> HEIGHTBITS;
                pixhigh += pixhighstep;

                if (mid >= fc_rwx)
                    mid = fc_rwx - 1;

                if (mid >= yl)
                {
                    dcvars.yl = yl;
                    dcvars.yh = mid;
                    dcvars.texturemid = rw_toptexturemid;
                    const texture_t *tex = R_GetOrLoadTexture(toptexture);
                    R_DrawSegTextureColumn(tex, texturecolumn, &dcvars, columnData);
                    R_DrawColumn(&dcvars);

                    cc_rwx = mid;
                }
                else
                    cc_rwx = yl - 1;
            }
            else  // no top wall
            {

                if (markceiling)
                    cc_rwx = yl - 1;
            }

            if (bottomtexture)          // bottom wall
            {
                int mid = (pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS;
                pixlow += pixlowstep;

                // no space above wall?
                if (mid <= cc_rwx)
                    mid = cc_rwx + 1;

                if (mid <= yh)
                {
                    dcvars.yl = mid;
                    dcvars.yh = yh;
                    dcvars.texturemid = rw_bottomtexturemid;
                    const texture_t *tex = R_GetOrLoadTexture(bottomtexture);
                    R_DrawSegTextureColumn(tex, texturecolumn, &dcvars, columnData);
                    R_DrawColumn(&dcvars);

                    fc_rwx = mid;
                }
                else
                    fc_rwx = yh + 1;
            }
            else        // no bottom wall
            {
                if (markfloor)
                    fc_rwx = yh + 1;
            }

            // cph - if we completely blocked further sight through this column,
            // add this info to the solid columns array for r_bsp.c
            if ((markceiling || markfloor) && (fc_rwx <= cc_rwx + 1))
            {

                solidcol[rw_x] = 1;
                didsolidcol = 1;
            }

            // save texturecol for backdrawing of masked mid texture
            if (maskedtexture)
                maskedtexturecol[rw_x] = texturecolumn;
        }

        rw_scale += rw_scalestep;
        topfrac += topstep;
        bottomfrac += bottomstep;

        floorclip[rw_x] = fc_rwx;
        ceilingclip[rw_x] = cc_rwx;
    }
#else 
#if MULTIPATCH_TEXTURE_SUPPORT
    if (midtexture)
    {
      if (rw_x >= rw_stopx)
        return;
      //uint8_t columnData[MAX_COLUMN_DATA];
      void *columnDataPtr, *nextColumnDataPtr;
      bool wasOnExternalFlash = false;
      int bufferNumber = 0;
      fixed_t iscale = FixedReciprocal((unsigned) rw_scale);
      const texture_t *tex = R_GetOrLoadTexture(midtexture);
      cachePatchColumnOffsetData((patch_t*) tex->patches[0].patch, tex->width, rw_x , rw_stopx);
      // texturecolumn and lighting are independent of wall tiers
      if (segtextured)
      {
          // calculate texture offset
          angle_t angle = (rw_centerangle + xtoviewangle[rw_x]) >> ANGLETOFINESHIFT;

          texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);

          texturecolumn >>= FRACBITS;
      }

      columnDataPtr =  R_GetSegTextureColumnNonBlocking(tex, texturecolumn, columnBuffer[1 - bufferNumber], &wasOnExternalFlash, iscale);
  
      for (; rw_x < rw_stopx; rw_x++)
      {
          // mark floor / ceiling areas

          int yh = bottomfrac >> HEIGHTBITS;
          int yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

          int cc_rwx = ceilingclip[rw_x];
          int fc_rwx = floorclip[rw_x];

          // no space above wall?
          int bottom, top = cc_rwx + 1;

          if (yl < top)
              yl = top;

          if (yl < draw_starty)
              yl = draw_starty;

          if (yh > draw_stopy)
              yh = draw_stopy;

          if (markceiling)
          {
              bottom = yl - 1;

              if (bottom >= fc_rwx)
                  bottom = fc_rwx - 1;

              if (top <= bottom)
              {
                  ceilingplane->top[rw_x] = top;
                  ceilingplane->bottom[rw_x] = bottom;
                  ceilingplane->modified = 1;
              }
              // SoM: this should be set here
              cc_rwx = bottom;
          }

          bottom = fc_rwx - 1;
          if (yh > bottom)
              yh = bottom;

          if (markfloor)
          {

              top = yh < cc_rwx ? cc_rwx : yh;

              if (++top <= bottom)
              {
                  floorplane->top[rw_x] = top;
                  floorplane->bottom[rw_x] = bottom;
                  floorplane->modified = 1;
              }
              // SoM: This should be set here to prevent overdraw
              fc_rwx = top;
          }


          // draw the wall tiers
 
          dcvars.yl = yl;     // single sided line
          dcvars.yh = yh;
          dcvars.texturemid = rw_midtexturemid;
          //
          const texture_t *tex = R_GetOrLoadTexture(midtexture);
          dcvars.source = columnDataPtr;


          // calculate dcvars for NEXT column
          // texturecolumn and lighting are independent of wall tiers
          if (segtextured)
          {
              // calculate next texture offset
              angle_t angle = (rw_centerangle + xtoviewangle[rw_x + 1]) >> ANGLETOFINESHIFT;

              texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);

              texturecolumn >>= FRACBITS;

              // calculate lighting
              int index = rw_scale >> LIGHTSCALESHIFT;

              if (index >= MAXLIGHTSCALE)
                  index = MAXLIGHTSCALE - 1;

              dcvars.colormap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];

              dcvars.x = rw_x;

              dcvars.iscale = iscale;
              iscale =  FixedReciprocal((unsigned)rw_scale + rw_scalestep) ;
          }

          if (wasOnExternalFlash)
          {
            while(!NRF_QSPI->EVENTS_READY);
            NRF_QSPI->EVENTS_READY = 0;
          }

          // load next column. Only if this is not the last one!
          if (rw_x < rw_stopx - 1)
          {
            nextColumnDataPtr =  R_GetSegTextureColumnNonBlocking(tex, texturecolumn,  columnBuffer[1 - bufferNumber], &wasOnExternalFlash, iscale);
          }
          bufferNumber = 1 - bufferNumber;
          R_DrawColumn(&dcvars);
          columnDataPtr = nextColumnDataPtr;
          cc_rwx = draw_stopy + 1;
          fc_rwx = draw_starty - 1;

          rw_scale += rw_scalestep;
          topfrac += topstep;
          bottomfrac += bottomstep;

          floorclip[rw_x] = fc_rwx;
          ceilingclip[rw_x] = cc_rwx;

      }
 #else
    if (midtexture)
    {
      if (rw_x >= rw_stopx)
        return;
      //uint8_t columnData[MAX_COLUMN_DATA];
      void  *nextColumnDataPtr;
      dmaOperation_t dmaOperation = {.pendingDmaOperation  = 0};
      int bufferNumber = 0;
      fixed_t iscale = FixedReciprocal((unsigned) rw_scale);
      const texture_t *tex = R_GetOrLoadTexture(midtexture);
      //cachePatchColumnOffsetData((patch_t*) tex->patches[0].patch, tex->width, rw_x , rw_stopx);
      // texturecolumn and lighting are independent of wall tiers
      //if (segtextured) midtexture already means it is segtextured
      {
          // calculate texture offset
          angle_t angle = (rw_centerangle + xtoviewangle[rw_x]) >> ANGLETOFINESHIFT;

          texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);

          texturecolumn >>= FRACBITS;
      }

      dcvars.source = R_GetSinglePatchTextureColumnNonBlocking (tex, texturecolumn, columnBuffer[bufferNumber], iscale, &dmaOperation, 1);
      // R_GetSegTextureColumnNonBlocking(tex, texturecolumn, columnBuffer[1 - bufferNumber], &wasOnExternalFlash, iscale);
  
      for (; rw_x < rw_stopx; rw_x++)
      {
          // mark floor / ceiling areas

          int yh = bottomfrac >> HEIGHTBITS;
          int yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

          int cc_rwx = ceilingclip[rw_x];
          int fc_rwx = floorclip[rw_x];

          // no space above wall?
          int bottom, top = cc_rwx + 1;

          if (yl < top)
              yl = top;

          if (yl < draw_starty)
              yl = draw_starty;

          if (yh > draw_stopy)
              yh = draw_stopy;

          if (markceiling)
          {
              bottom = yl - 1;

              if (bottom >= fc_rwx)
                  bottom = fc_rwx - 1;

              if (top <= bottom)
              {
                  ceilingplane->top[rw_x] = top;
                  ceilingplane->bottom[rw_x] = bottom;
                  ceilingplane->modified = 1;
              }
              // SoM: this should be set here
              cc_rwx = bottom;
          }

          bottom = fc_rwx - 1;
          if (yh > bottom)
              yh = bottom;

          if (markfloor)
          {

              top = yh < cc_rwx ? cc_rwx : yh;

              if (++top <= bottom)
              {
                  floorplane->top[rw_x] = top;
                  floorplane->bottom[rw_x] = bottom;
                  floorplane->modified = 1;
              }
              // SoM: This should be set here to prevent overdraw
              fc_rwx = top;
          }


          // draw the wall tiers
 
          dcvars.yl = yl;     // single sided line
          dcvars.yh = yh;
          dcvars.texturemid = rw_midtexturemid;
          //
          //const texture_t *tex = R_GetOrLoadTexture(midtexture);
          //dcvars.source = columnDataPtr;


          // calculate dcvars for NEXT column
          // texturecolumn and lighting are independent of wall tiers
          // if (segtextured) // midtexture implied segtextured.
          {
              // calculate next texture offset
              angle_t angle = (rw_centerangle + xtoviewangle[rw_x + 1]) >> ANGLETOFINESHIFT;

              texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);

              texturecolumn >>= FRACBITS;

              // calculate lighting
              int index = rw_scale >> LIGHTSCALESHIFT;

              if (index >= MAXLIGHTSCALE)
                  index = MAXLIGHTSCALE - 1;

              dcvars.colormap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];

              dcvars.x = rw_x;

              dcvars.iscale = iscale;
              iscale =  FixedReciprocal((unsigned)rw_scale + rw_scalestep) ;
          }

          // load next column. Only if this is not the last one!
          if (rw_x < rw_stopx - 1)
          {
            bufferNumber = 1 - bufferNumber;
            nextColumnDataPtr = R_GetSinglePatchTextureColumnNonBlocking (tex, texturecolumn, columnBuffer[bufferNumber], iscale, &dmaOperation, 1);
          }
          R_DrawColumn(&dcvars);
          dcvars.source = nextColumnDataPtr;
          cc_rwx = draw_stopy + 1;
          fc_rwx = draw_starty - 1;

          rw_scale += rw_scalestep;
          topfrac += topstep;
          bottomfrac += bottomstep;

          floorclip[rw_x] = fc_rwx;
          ceilingclip[rw_x] = cc_rwx;

      }
 #endif
    }
    else
    {
#if 1
        //
        dmaOperation_t dmaOperation = {.pendingDmaOperation  = 0};
        bool validTop = false;
        bool validBot = false;
        bool wasOnExternalFlash = false;
        uint32_t nBuff = 0;
        draw_column_vars_t topDcvars;
        draw_column_vars_t botDcvars;
        void *nextTopColumnPtr, *nextBotColumnPtr;
        //
        texture_t *toptex, *bottex;
        if (toptexture)
        {
            toptex = (texture_t*) R_GetOrLoadTexture(toptexture);            
            //cachePatchColumnOffsetData((patch_t*) toptex->patches[0].patch, toptex->width, rw_x , rw_stopx);
        }
        if (bottomtexture)
        {
            bottex = (texture_t*) R_GetOrLoadTexture(bottomtexture);
//            cachePatchColumnOffsetData((patch_t*) bottex->patches[0].patch, bottex->width, rw_x , rw_stopx);
        }
        for (; rw_x < rw_stopx; rw_x++)
        {
            // mark floor / ceiling areas

            int yh = bottomfrac >> HEIGHTBITS;
            int yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

            int cc_rwx = ceilingclip[rw_x];
            int fc_rwx = floorclip[rw_x];

            // no space above wall?
            int bottom, top = cc_rwx + 1;

            if (yl < top)
                yl = top;

            if (yl < draw_starty)
                yl = draw_starty;

            if (yh > draw_stopy)
                yh = draw_stopy;

            if (markceiling)
            {
                bottom = yl - 1;

                if (bottom >= fc_rwx)
                    bottom = fc_rwx - 1;

                if (top <= bottom)
                {
                    ceilingplane->top[rw_x] = top;
                    ceilingplane->bottom[rw_x] = bottom;
                    ceilingplane->modified = 1;
                }
                // SoM: this should be set here
                cc_rwx = bottom;
            }

            bottom = fc_rwx - 1;
            if (yh > bottom)
                yh = bottom;

            if (markfloor)
            {

                top = yh < cc_rwx ? cc_rwx : yh;

                if (++top <= bottom)
                {
                    floorplane->top[rw_x] = top;
                    floorplane->bottom[rw_x] = bottom;
                    floorplane->modified = 1;
                }
                // SoM: This should be set here to prevent overdraw
                fc_rwx = top;
            }

            // texturecolumn and lighting are independent of wall tiers
            lighttable_t *nextColorMap;
            fixed_t iscale;
            if (segtextured)
            {
               uint32_t index;
                // calculate texture offset
                angle_t angle = (rw_centerangle + xtoviewangle[rw_x]) >> ANGLETOFINESHIFT;

                texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);

                texturecolumn >>= FRACBITS;

                // calculate lighting
                index = rw_scale >> LIGHTSCALESHIFT;

                if (index >= MAXLIGHTSCALE)
                    index = MAXLIGHTSCALE - 1;
                nextColorMap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];
                iscale = FixedReciprocal((unsigned) rw_scale);
            }

            // draw the wall tiers
            {

                // two sided line
                if (toptexture)
                {

                    // top wall
                    int mid = pixhigh >> HEIGHTBITS;
                    pixhigh += pixhighstep;

                    if (mid >= fc_rwx)
                        mid = fc_rwx - 1;

                    if (mid >= yl)
                    {   
                        nextTopColumnPtr =  R_GetSinglePatchTextureColumnNonBlocking(toptex, texturecolumn,  columnBuffer[nBuff], iscale, &dmaOperation, 1);
                        cc_rwx = mid;
                        if (validTop)
                        {   
                           R_DrawColumn(&topDcvars);
                        }
                        topDcvars.source = nextTopColumnPtr;
                        topDcvars.colormap = nextColorMap;
                        topDcvars.iscale = iscale;         
                        topDcvars.yl = yl;
                        topDcvars.yh = mid;
                        topDcvars.texturemid = rw_toptexturemid;
                        topDcvars.x = rw_x;
                        validTop = true;
                    }
                    else
                    {
                        cc_rwx = yl - 1;
                        if (validTop)
                        {   
                            // no column must be loaded but before we had to load via DMA and we did not wait (e.g. no bottom texture is present)
                            // we must make sure the column has been loaded
                            if (dmaOperation.topTextureLoadByDma)
                            {
                                while(!NRF_QSPI->EVENTS_READY);
                                NRF_QSPI->EVENTS_READY = 0;
                            }
                            dmaOperation.pendingDmaOperation = 0; // after this, for sure it has been loaded
                            R_DrawColumn(&topDcvars);
                        }
                        validTop = false;
                    }
                }
                else  // no top wall
                {

                    if (markceiling)
                        cc_rwx = yl - 1;
                }

                if (bottomtexture)          // bottom wall
                {

                    int mid = (pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS;
                    pixlow += pixlowstep;

                    // no space above wall?
                    if (mid <= cc_rwx)
                        mid = cc_rwx + 1;

                    if (mid <= yh)
                    {
                        nextBotColumnPtr =  R_GetSinglePatchTextureColumnNonBlocking(bottex, texturecolumn,  (uint8_t*) columnBuffer[nBuff] + 128, iscale, &dmaOperation, 0);
                        fc_rwx = mid;
                        if (validBot)
                        {   
                           R_DrawColumn(&botDcvars);
                        }
                        validBot = true;
                        botDcvars.source = nextBotColumnPtr;
                        botDcvars.colormap = nextColorMap;
                        botDcvars.yl = mid;
                        botDcvars.yh = yh;
                        botDcvars.iscale = iscale;
                        botDcvars.texturemid = rw_bottomtexturemid;
                        botDcvars.x = rw_x;
                    }
                    else
                    {
                        fc_rwx = yh + 1;
                        if (validBot)
                        {   
                            // no column must be loaded but before we had to load via DMA and we did not wait (e.g. no top texture is present)
                            // we must make sure the column has been loaded
                            if (dmaOperation.bottomTextureLoadByDma)
                            {
                                while(!NRF_QSPI->EVENTS_READY);
                                NRF_QSPI->EVENTS_READY = 0;
                            }
                            dmaOperation.pendingDmaOperation = 0; // after this, for sure it has been loaded

                            R_DrawColumn(&botDcvars);
                        }
                        validBot = false;
                    }

                }
                else        // no bottom wall
                {
                    if (markfloor)
                        fc_rwx = yh + 1;
                }

                // cph - if we completely blocked further sight through this column,
                // add this info to the solid columns array for r_bsp.c
                if ((markceiling || markfloor) && (fc_rwx <= cc_rwx + 1))
                {

                    solidcol[rw_x] = 1;
                    didsolidcol = 1;
                }
                //
                nBuff = 1 - nBuff;

                // save texturecol for backdrawing of masked mid texture
                if (maskedtexture)
                    maskedtexturecol[rw_x] = texturecolumn;
            }

            rw_scale += rw_scalestep;
            topfrac += topstep;
            bottomfrac += bottomstep;

            floorclip[rw_x] = fc_rwx;
            ceilingclip[rw_x] = cc_rwx;
        }
        if (validTop)
        {   
            // no column must be loaded but before we had to load via DMA and we did not wait (e.g. no bottom texture is present)
            // we must make sure the column has been loaded
            if (dmaOperation.topTextureLoadByDma)
            {
                while(!NRF_QSPI->EVENTS_READY);
                NRF_QSPI->EVENTS_READY = 0;
            }
            dmaOperation.pendingDmaOperation = 0; // after this, for sure it has been loaded
            R_DrawColumn(&topDcvars);
        }
        if (validBot)
        {   
            // no column must be loaded but before we had to load via DMA and we did not wait (e.g. no top texture is present)
            // we must make sure the column has been loaded
            if (dmaOperation.bottomTextureLoadByDma)
            {
                while(!NRF_QSPI->EVENTS_READY);
                NRF_QSPI->EVENTS_READY = 0;
            }
            dmaOperation.pendingDmaOperation = 0; // after this, for sure it has been loaded

            R_DrawColumn(&botDcvars);
        }
        #else
        uint8_t columnData[MAX_COLUMN_DATA];
       /* if (bottomtexture)
        {
            const texture_t *tex = R_GetOrLoadTexture(bottomtexture);
            cachePatchColumnOffsetData((patch_t *) tex->patches[0].patch, tex->width, rw_x , rw_stopx);
        }*/

        for (; rw_x < rw_stopx; rw_x++)
        {
            // mark floor / ceiling areas

            int yh = bottomfrac >> HEIGHTBITS;
            int yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

            int cc_rwx = ceilingclip[rw_x];
            int fc_rwx = floorclip[rw_x];

            // no space above wall?
            int bottom, top = cc_rwx + 1;

            if (yl < top)
                yl = top;

            if (yl < draw_starty)
                yl = draw_starty;

            if (yh > draw_stopy)
                yh = draw_stopy;

            if (markceiling)
            {
                bottom = yl - 1;

                if (bottom >= fc_rwx)
                    bottom = fc_rwx - 1;

                if (top <= bottom)
                {
                    ceilingplane->top[rw_x] = top;
                    ceilingplane->bottom[rw_x] = bottom;
                    ceilingplane->modified = 1;
                }
                // SoM: this should be set here
                cc_rwx = bottom;
            }

            bottom = fc_rwx - 1;
            if (yh > bottom)
                yh = bottom;

            if (markfloor)
            {

                top = yh < cc_rwx ? cc_rwx : yh;

                if (++top <= bottom)
                {
                    floorplane->top[rw_x] = top;
                    floorplane->bottom[rw_x] = bottom;
                    floorplane->modified = 1;
                }
                // SoM: This should be set here to prevent overdraw
                fc_rwx = top;
            }

            // texturecolumn and lighting are independent of wall tiers
            if (segtextured)
            {
                // calculate texture offset
                angle_t angle = (rw_centerangle + xtoviewangle[rw_x]) >> ANGLETOFINESHIFT;

                texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);

                texturecolumn >>= FRACBITS;

                // calculate lighting
                int index = rw_scale >> LIGHTSCALESHIFT;

                if (index >= MAXLIGHTSCALE)
                    index = MAXLIGHTSCALE - 1;

                dcvars.colormap = p_wad_immutable_flash_data->colormaps + 256 * scalelight[walllights][index];

                dcvars.x = rw_x;

                dcvars.iscale = FixedReciprocal((unsigned) rw_scale);
            }

            // draw the wall tiers
            {

                // two sided line
                if (toptexture)
                {
                    // top wall
                    int mid = pixhigh >> HEIGHTBITS;
                    pixhigh += pixhighstep;

                    if (mid >= fc_rwx)
                        mid = fc_rwx - 1;

                    if (mid >= yl)
                    {
                        dcvars.yl = yl;
                        dcvars.yh = mid;
                        dcvars.texturemid = rw_toptexturemid;
                        const texture_t *tex = R_GetOrLoadTexture(toptexture);
                        R_DrawSegTextureColumn(tex, texturecolumn, &dcvars, columnData);
 
                        cc_rwx = mid;
                    }
                    else
                        cc_rwx = yl - 1;
                }
                else  // no top wall
                {

                    if (markceiling)
                        cc_rwx = yl - 1;
                }

                if (bottomtexture)          // bottom wall
                {
                    int mid = (pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS;
                    pixlow += pixlowstep;

                    // no space above wall?
                    if (mid <= cc_rwx)
                        mid = cc_rwx + 1;

                    if (mid <= yh)
                    {
                        dcvars.yl = mid;
                        dcvars.yh = yh;
                        dcvars.texturemid = rw_bottomtexturemid;
                        const texture_t *tex = R_GetOrLoadTexture(bottomtexture);
                        R_DrawSegTextureColumn(tex, texturecolumn, &dcvars, columnData);

                        fc_rwx = mid;
                    }
                    else
                        fc_rwx = yh + 1;
                }
                else        // no bottom wall
                {
                    if (markfloor)
                        fc_rwx = yh + 1;
                }

                // cph - if we completely blocked further sight through this column,
                // add this info to the solid columns array for r_bsp.c
                if ((markceiling || markfloor) && (fc_rwx <= cc_rwx + 1))
                {

                    solidcol[rw_x] = 1;
                    didsolidcol = 1;
                }

                // save texturecol for backdrawing of masked mid texture
                if (maskedtexture)
                    maskedtexturecol[rw_x] = texturecolumn;
            }

            rw_scale += rw_scalestep;
            topfrac += topstep;
            bottomfrac += bottomstep;

            floorclip[rw_x] = fc_rwx;
            ceilingclip[rw_x] = cc_rwx;
        }
        #endif
    }
    #endif
}
// 2021-13-08 next-hack: created this function to copy openings by word and not by halfword
static inline void fastOpeningsCopy(short *dest, short * source, int num)
{
    int32_t *s = (int32_t*) source;
    int32_t *d = (int32_t*) dest;
    for (int i = 0; i < num / 2; i++)
    {
    *d++ = *s++;
    }
    // copy any odd opening
    if (num & 1)
        *((short*)d) = *((short*)s);  
}

static boolean R_CheckOpenings(const int start)
{
    int pos = _g->lastopening - openings;
    int need = (rw_stopx - start) * 4 + pos;

//#ifdef RANGECHECK
    if (need > MAXOPENINGS)
        printf("Openings overflow. Need = %d", need);
//#endif

    return need <= MAXOPENINGS;
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
FASTFUN static void R_StoreWallRange(const int start, const int stop)
{
    fixed_t hyp;
    angle_t offsetangle;

    // don't overflow and crash
    if (ds_p == &_g->drawsegs[MAXDRAWSEGS])
    {
#ifdef RANGECHECK
        I_Error("Drawsegs overflow.");
#endif
        return;
    }

    linedata_t *linedata = &_g->linedata[curline->linenum];

    // mark the segment as visible for auto map
    linedata->r_flags |= RF_MAPPED; // 2020-03-14 next-hack was ML_MAPPED, changed to RF_MAPPED

    sidedef = &_g->sides[curline->sidenum];
    linedef = &_g->lines[curline->linenum];

    // calculate rw_distance for scale calculation
    rw_normalangle = curline->angle + ANG90;

    offsetangle = rw_normalangle - rw_angle1;

    if (D_abs(offsetangle) > ANG90)
        offsetangle = ANG90;

    hyp = (viewx == curline->v1.x && viewy == curline->v1.y) ? 0 : R_PointToDist(curline->v1.x, curline->v1.y);

    rw_distance = FixedMul(hyp, finecosine[offsetangle >> ANGLETOFINESHIFT]);

    int rw_x = ds_p->x1 = start;
    ds_p->x2 = stop;
    ds_p->curline = curline;
    rw_stopx = stop + 1;

    //Openings overflow. Nevermind.
    if (!R_CheckOpenings(start))
        return;

    // calculate scale at both ends and step
    ds_p->scale1 = rw_scale = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[start]);

    if (stop > start)
    {
        ds_p->scale2 = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[stop]);
        ds_p->scalestep = rw_scalestep = (ds_p->scale2 - rw_scale) / (stop - start);
    }
    else
        ds_p->scale2 = ds_p->scale1;

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed

    worldtop = frontsector->ceilingheight - viewz;
    worldbottom = frontsector->floorheight - viewz;

    midtexture = toptexture = bottomtexture = maskedtexture = 0;
    ds_p->maskedtexturecol = NULL;

    if (!backsector)
    {
        // single sided line
        midtexture = texturetranslation[getSideMidTexture(&_g->lines[curline->linenum], sidedef)];

        // a single sided line is terminal, so it must mark ends
        markfloor = markceiling = true;

        if (linedef->flags & ML_DONTPEGBOTTOM)
        {         // bottom of texture at bottom
            fixed_t vtop = frontsector->floorheight + textureheight[getSideMidTexture(&_g->lines[curline->linenum], sidedef)];
            rw_midtexturemid = vtop - viewz;
        }
        else
            // top of texture at top
            rw_midtexturemid = worldtop;

        rw_midtexturemid += FixedMod((sidedef->rowoffset << FRACBITS), textureheight[midtexture]);

        ds_p->silhouette = SIL_BOTH;
        ds_p->sprtopclip_ssptr = SCREENHEIGHTARRAY_PTR; //(short *) screenheightarray;
        ds_p->sprbottomclip_ssptr = NEGONEARRAY_PTR;    //(short *) negonearray;
        ds_p->bsilheight = INT_MAX;
        ds_p->tsilheight = INT_MIN;
    }
    else      // two sided line
    {
        ds_p->sprtopclip_ssptr = ds_p->sprbottomclip_ssptr = 0;
        ds_p->silhouette = 0;

        if (linedata->r_flags & RF_CLOSED)
        { /* cph - closed 2S line e.g. door */
            // cph - killough's (outdated) comment follows - this deals with both
            // "automap fixes", his and mine
            // killough 1/17/98: this test is required if the fix
            // for the automap bug (r_bsp.c) is used, or else some
            // sprites will be displayed behind closed doors. That
            // fix prevents lines behind closed doors with dropoffs
            // from being displayed on the automap.

            ds_p->silhouette = SIL_BOTH;
            ds_p->sprbottomclip_ssptr = NEGONEARRAY_PTR; //(short *) negonearray;
            ds_p->bsilheight = INT_MAX;
            ds_p->sprtopclip_ssptr = SCREENHEIGHTARRAY_PTR; //(short *) screenheightarray;
            ds_p->tsilheight = INT_MIN;

        }
        else
        { /* not solid - old code */

            if (frontsector->floorheight > backsector->floorheight)
            {
                ds_p->silhouette = SIL_BOTTOM;
                ds_p->bsilheight = frontsector->floorheight;
            }
            else if (backsector->floorheight > viewz)
            {
                ds_p->silhouette = SIL_BOTTOM;
                ds_p->bsilheight = INT_MAX;
            }

            if (frontsector->ceilingheight < backsector->ceilingheight)
            {
                ds_p->silhouette |= SIL_TOP;
                ds_p->tsilheight = frontsector->ceilingheight;
            }
            else if (backsector->ceilingheight < viewz)
            {
                ds_p->silhouette |= SIL_TOP;
                ds_p->tsilheight = INT_MIN;
            }
        }

        worldhigh = backsector->ceilingheight - viewz;
        worldlow = backsector->floorheight - viewz;

        // hack to allow height changes in outdoor areas
        if (frontsector->ceilingpic == _g->skyflatnum && backsector->ceilingpic == _g->skyflatnum)
            worldtop = worldhigh;

        markfloor = worldlow != worldbottom || backsector->floorpic != frontsector->floorpic || backsector->lightlevel != frontsector->lightlevel;

        markceiling = worldhigh != worldtop || backsector->ceilingpic != frontsector->ceilingpic || backsector->lightlevel != frontsector->lightlevel;

        if (backsector->ceilingheight <= frontsector->floorheight || backsector->floorheight >= frontsector->ceilingheight)
            markceiling = markfloor = true;   // closed door

        if (worldhigh < worldtop)   // top texture
        {
            toptexture = texturetranslation[getSideTopTexture(linedef, sidedef)];
            rw_toptexturemid =
                    linedef->flags & ML_DONTPEGTOP ? worldtop : backsector->ceilingheight + textureheight[getSideTopTexture(linedef, sidedef)] - viewz;
            rw_toptexturemid += FixedMod((sidedef->rowoffset << FRACBITS), textureheight[toptexture]);
        }

        if (worldlow > worldbottom) // bottom texture
        {
            bottomtexture = texturetranslation[getSideBottomTexture(linedef, sidedef)];
            rw_bottomtexturemid =
                    linedef->flags & ML_DONTPEGBOTTOM ? worldtop : worldlow;

            rw_bottomtexturemid += FixedMod((sidedef->rowoffset << FRACBITS), textureheight[bottomtexture]);
        }

        // allocate space for masked texture tables
        if (getSideMidTexture(&_g->lines[curline->linenum], sidedef)) // masked midtexture
        {
            maskedtexture = true;
            ds_p->maskedtexturecol = maskedtexturecol = _g->lastopening - rw_x;
            _g->lastopening += rw_stopx - rw_x;
        }
    }

    // calculate rw_offset (only needed for textured lines)
    segtextured = ((midtexture | toptexture | bottomtexture | maskedtexture) > 0);

    if (segtextured)
    {
        rw_offset = FixedMul(hyp, -finesine[offsetangle >> ANGLETOFINESHIFT]);

//        rw_offset += (sidedef->textureoffset << FRACBITS) + curline->offset;
// 2021-02-13 next-hack: now textureoffsets are stored in RAM in a separate array. sides are stored in flash
        rw_offset += (_g->textureoffsets[curline->sidenum] << FRACBITS) + curline->offset;

        rw_centerangle = ANG90 + viewangle - rw_normalangle;

        rw_lightlevel = frontsector->lightlevel;
        // calculate light table
        //  use different light tables
        //  for horizontal / vertical / diagonal
        // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
        if (!fixedcolormap)
        {
            int lightnum = (rw_lightlevel >> LIGHTSEGSHIFT) + extralight;

            if (curline->v1.y == curline->v2.y)
                lightnum--;
            else if (curline->v1.x == curline->v2.x)
                lightnum++;

            if (lightnum < 0)
                walllights = 0;        //scalelight[0];
            else if (lightnum >= LIGHTLEVELS)
                walllights = LIGHTLEVELS - 1;       //scalelight[LIGHTLEVELS-1];
            else
                walllights = lightnum; //scalelight[lightnum];
        }
    }

    // if a floor / ceiling plane is on the wrong side of the view
    // plane, it is definitely invisible and doesn't need to be marked.
    if (frontsector->floorheight >= viewz)       // above view plane
        markfloor = false;
    if (frontsector->ceilingheight <= viewz && frontsector->ceilingpic != _g->skyflatnum) // below view plane
        markceiling = false;

    // calculate incremental stepping values for texture edges
    worldtop >>= 4;
    worldbottom >>= 4;

    topstep = -FixedMul(rw_scalestep, worldtop);
    topfrac = (centeryfrac >> 4) - FixedMul(worldtop, rw_scale);
    //
    bottomstep = -FixedMul(rw_scalestep, worldbottom);
    bottomfrac = (centeryfrac >> 4) - FixedMul(worldbottom, rw_scale);

    if (backsector)
    {
        worldhigh >>= 4;
        worldlow >>= 4;

        if (worldhigh < worldtop)
        {
            pixhigh = (centeryfrac >> 4) - FixedMul(worldhigh, rw_scale);
            pixhighstep = -FixedMul(rw_scalestep, worldhigh);
        }
        if (worldlow > worldbottom)
        {
            pixlow = (centeryfrac >> 4) - FixedMul(worldlow, rw_scale);
            pixlowstep = -FixedMul(rw_scalestep, worldlow);
        }
    }

    // render it
    if (markceiling)
    {
        if (ceilingplane)   // killough 4/11/98: add NULL ptr checks
            ceilingplane = R_CheckPlane(ceilingplane, rw_x, rw_stopx - 1);
        else
            markceiling = 0;
    }

    if (markfloor)
    {
        if (floorplane)     // killough 4/11/98: add NULL ptr checks
            /* cph 2003/04/18  - ceilingplane and floorplane might be the same
             * visplane (e.g. if both skies); R_CheckPlane doesn't know about
             * modifications to the plane that might happen in parallel with the check
             * being made, so we have to override it and split them anyway if that is
             * a possibility, otherwise the floor marking would overwrite the ceiling
             * marking, resulting in HOM. */
            if (markceiling && ceilingplane == floorplane)
                floorplane = R_DupPlane(floorplane, rw_x, rw_stopx - 1);
            else
                floorplane = R_CheckPlane(floorplane, rw_x, rw_stopx - 1);
        else
            markfloor = 0;
    }

    didsolidcol = 0;
    R_RenderSegLoop(rw_x);

    /* cph - if a column was made solid by this wall, we _must_ save full clipping info */
    if (backsector && didsolidcol)
    {
        if (!(ds_p->silhouette & SIL_BOTTOM))
        {
            ds_p->silhouette |= SIL_BOTTOM;
            ds_p->bsilheight = backsector->floorheight;
        }
        if (!(ds_p->silhouette & SIL_TOP))
        {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = backsector->ceilingheight;
        }
    }

    // save sprite clipping info
    if (((ds_p->silhouette & SIL_TOP) || maskedtexture) && !ds_p->sprtopclip_ssptr)
    {
 #if 1
      fastOpeningsCopy(_g->lastopening, &ceilingclip[start], (rw_stopx - start));
 #else
        for (int f = 0; f < (rw_stopx - start); f++)
        {
            _g->lastopening[f] = ceilingclip[f + start];
        }
 #endif 
        ds_p->sprtopclip_ssptr = openingAddrToSsptr(_g->lastopening - start);
        _g->lastopening += rw_stopx - start;
    }

    if (((ds_p->silhouette & SIL_BOTTOM) || maskedtexture) && !ds_p->sprbottomclip_ssptr)
    {
#if 1
      fastOpeningsCopy(_g->lastopening, &floorclip[start], (rw_stopx - start));
#else
        for (int f = 0; f < (rw_stopx - start); f++)
        {
            _g->lastopening[f] = floorclip[f + start];
        }
#endif
        ds_p->sprbottomclip_ssptr = openingAddrToSsptr(_g->lastopening - start);
        _g->lastopening += rw_stopx - start;
    }
    if (maskedtexture && !(ds_p->silhouette & SIL_TOP))
    {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
    }

    if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM))
    {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
    }

    ds_p++;
}

// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// cph - converted to R_RecalcLineFlags. This recalculates all the flags for
// a line, including closure and texture tiling.
static void R_RecalcLineFlags(void)
{
    linedata_t *linedata = &_g->linedata[linedef->lineno];

    const side_t *side = &_g->sides[curline->sidenum];

    linedata->r_validcount = (_g->gametic & VALIDCOUNT_MASK);

    /* First decide if the line is closed, normal, or invisible */
    if (!(linedef->flags & ML_TWOSIDED) || backsector->ceilingheight <= frontsector->floorheight || backsector->floorheight >= frontsector->ceilingheight || (
    // if door is closed because back is shut:
    backsector->ceilingheight <= backsector->floorheight

    // preserve a kind of transparent door/lift special effect:
    && (backsector->ceilingheight >= frontsector->ceilingheight || getSideTopTexture(linedef, side))

    && (backsector->floorheight <= frontsector->floorheight || getSideBottomTexture(linedef, side))

    // properly render skies (consider door "open" if both ceilings are sky):
    && (backsector->ceilingpic != _g->skyflatnum || frontsector->ceilingpic != _g->skyflatnum)))
        linedata->r_flags = (RF_CLOSED | (linedata->r_flags & RF_MAPPED)); // 2020-03-14 next-hack was ML_MAPPED, changed to RF_MAPPED
    else
    {
        // Reject empty lines used for triggers
        //  and special events.
        // Identical floor and ceiling on both sides,
        // identical light levels on both sides,
        // and no middle texture.
        // CPhipps - recode for speed, not certain if this is portable though
        if (backsector->ceilingheight != frontsector->ceilingheight || backsector->floorheight != frontsector->floorheight || getSideMidTexture(linedef, side) || backsector->ceilingpic != frontsector->ceilingpic || backsector->floorpic != frontsector->floorpic || backsector->lightlevel != frontsector->lightlevel)
        {
            linedata->r_flags = (linedata->r_flags & RF_MAPPED);
            return; // 2020-03-14 next-hack was ML_MAPPED, changed to RF_MAPPED
        }
        else
            linedata->r_flags = (RF_IGNORE | (linedata->r_flags & RF_MAPPED)); // 2020-03-14 next-hack was ML_MAPPED, changed to RF_MAPPED
    }
}

// CPhipps -
// R_ClipWallSegment
//
// Replaces the old R_Clip*WallSegment functions. It draws bits of walls in those
// columns which aren't solid, and updates the solidcol[] array appropriately

static void R_ClipWallSegment(int first, int last, boolean solid)
{
    byte *p;
    while (first < last)
    {
        if (solidcol[first])
        {
            if (!(p = ByteFind(solidcol + first, 0, last - first)))
                return; // All solid

            first = p - solidcol;
        }
        else
        {
            int to;
            if (!(p = ByteFind(solidcol + first, 1, last - first)))
                to = last;
            else
                to = p - solidcol;

            R_StoreWallRange(first, to - 1);

            if (solid)
            {
                memset(solidcol + first, 1, to - first);
            }

            first = to;
        }
    }
}

//
// R_ClearClipSegs
//

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//

static void R_AddLine(const seg_t *line)
{
    int x1;
    int x2;
    angle_t angle1;
    angle_t angle2;
    angle_t span;
    angle_t tspan;

    curline = line;

    angle1 = R_PointToAngle(line->v1.x, line->v1.y);
    angle2 = R_PointToAngle(line->v2.x, line->v2.y);

    // Clip to view edges.
    span = angle1 - angle2;

    // Back side, i.e. backface culling
    if (span >= ANG180)
        return;

    // Global angle needed by segcalc.
    rw_angle1 = angle1;
    angle1 -= viewangle;
    angle2 -= viewangle;

    tspan = angle1 + clipangle;
    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;

        angle1 = clipangle;
    }

    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;
        angle2 = 0 - clipangle;
    }

    // The seg is in the view range,
    // but not necessarily visible.

    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;

    // killough 1/31/98: Here is where "slime trails" can SOMETIMES occur:
    x1 = viewangletox[angle1];
    x2 = viewangletox[angle2];

    // Does not cross a pixel?
    if (x1 >= x2)       // killough 1/31/98 -- change == to >= for robustness
        return;

    backsector = SG_BACKSECTOR(line);

    /* cph - roll up linedef properties in flags */
    linedef = &_g->lines[curline->linenum];
    linedata_t *linedata = &_g->linedata[linedef->lineno];

    if (linedata->r_validcount != (_g->gametic & VALIDCOUNT_MASK))
        R_RecalcLineFlags();

    if (linedata->r_flags & RF_IGNORE)
    {
        return;
    }
    else
        R_ClipWallSegment(x1, x2, linedata->r_flags & RF_CLOSED);
}

//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
// killough 1/31/98 -- made static, polished

static void R_Subsector(int num)
{
    int count;
    const seg_t *line;
    subsector_t *sub;

    sub = &_g->subsectors[num];
    frontsector = sub->sector;
    count = sub->numlines;
    line = &_g->segs[sub->firstline];
    if (frontsector->floorheight < viewz)
    {
        floorplane = R_FindPlane(frontsector->floorheight, frontsector->floorpic, frontsector->lightlevel // killough 3/16/98
        );
    }
    else
    {
        floorplane = NULL;
    }

    if (frontsector->ceilingheight > viewz || (frontsector->ceilingpic == _g->skyflatnum))
    {
        ceilingplane = R_FindPlane(frontsector->ceilingheight, // killough 3/8/98
        frontsector->ceilingpic, frontsector->lightlevel);
    }
    else
    {
        ceilingplane = NULL;
    }

    R_AddSprites(sub, frontsector->lightlevel);
    while (count--)
    {
        R_AddLine(line);
        line++;
        curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
    }
}

//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//

static const byte checkcoord[12][4] = // killough -- static const
{
    { 3, 0, 2, 1 },
    { 3, 0, 2, 0 },
    { 3, 1, 2, 0 },
    { 0 },
    { 2, 0, 2, 1 },
    { 0, 0, 0, 0 },
    { 3, 1, 3, 0 },
    { 0 },
    { 2, 0, 3, 1 },
    { 2, 1, 3, 1 },
    { 2, 1, 3, 0 } 
};

// killough 1/28/98: static // CPhipps - const parameter, reformatted
static boolean R_CheckBBox(const short *bspcoord)
{
    angle_t angle1, angle2;

    {
        int boxpos;
        const byte *check;

        // Find the corners of the box
        // that define the edges from current viewpoint.
        boxpos = (viewx <= ((fixed_t) bspcoord[BOXLEFT] << FRACBITS) ? 0 :
                  viewx < ((fixed_t) bspcoord[BOXRIGHT] << FRACBITS) ? 1 : 2) + (
                viewy >= ((fixed_t) bspcoord[BOXTOP] << FRACBITS) ? 0 :
                viewy > ((fixed_t) bspcoord[BOXBOTTOM] << FRACBITS) ? 4 : 8);

        if (boxpos == 5)
            return true;

        check = checkcoord[boxpos];
        angle1 = R_PointToAngle(((fixed_t) bspcoord[check[0]] << FRACBITS), ((fixed_t) bspcoord[check[1]] << FRACBITS)) - viewangle;
        angle2 = R_PointToAngle(((fixed_t) bspcoord[check[2]] << FRACBITS), ((fixed_t) bspcoord[check[3]] << FRACBITS)) - viewangle;
    }

    // cph - replaced old code, which was unclear and badly commented
    // Much more efficient code now
    if ((signed) angle1 < (signed) angle2)
    { /* it's "behind" us */
        /* Either angle1 or angle2 is behind us, so it doesn't matter if we
         * change it to the corect sign
         */
        if ((angle1 >= ANG180) && (angle1 < ANG270))
            angle1 = INT_MAX; /* which is ANG180-1 */
        else
            angle2 = INT_MIN;
    }

    if ((signed) angle2 >= (signed) clipangle)
        return false; // Both off left edge
    if ((signed) angle1 <= -(signed) clipangle)
        return false; // Both off right edge
    if ((signed) angle1 >= (signed) clipangle)
        angle1 = clipangle; // Clip at left edge
    if ((signed) angle2 <= -(signed) clipangle)
        angle2 = 0 - clipangle; // Clip at right edge

    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    {
        int sx1 = viewangletox[angle1];
        int sx2 = viewangletox[angle2];
        //    const cliprange_t *start;

        // Does not cross a pixel.
        if (sx1 == sx2)
            return false;

        if (!ByteFind(solidcol + sx1, 0, sx2 - sx1))
            return false;
        // All columns it covers are already solidly covered
    }

    return true;
}

//Render a BSP subsector if bspnum is a leaf node.
//Return false if bspnum is frame node.

static boolean R_RenderBspSubsector(int bspnum)
{
    // Found a subsector?
    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1)
            R_Subsector(0);
        else
            R_Subsector(bspnum & (~NF_SUBSECTOR));

        return true;
    }

    return false;
}

// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.

//Non recursive version.
//constant stack space used and easier to
//performance profile.

static void R_RenderBSPNode(int bspnum)
{
    // 2021/05/22 next-hack: converted to short [] to avoid having a huge stack.
    // after all, all the quantities are less than 32k.
    // short bspstack[MAX_BSP_DEPTH];
    int sp = 0;

    const mapnode_t *bsp;
    int side = 0;

    while (true)
    {
        //Front sides.
        while (!R_RenderBspSubsector(bspnum))
        {
            if (sp == MAX_BSP_DEPTH)
                break;

            bsp = &nodes[bspnum];

            side = R_PointOnSide(viewx, viewy, bsp);

            bspstack[sp++] = bspnum;
            bspstack[sp++] = side;

            bspnum = bsp->children[side];
        }

        if (sp == 0)
        {
            //back at root node and not visible. All done!
            return;
        }

        //Back sides.
        side = bspstack[--sp];
        bspnum = bspstack[--sp];
        bsp = &nodes[bspnum];

        // Possibly divide back space.
        //Walk back up the tree until we find
        //a node that has a visible backspace.
        while (!R_CheckBBox(bsp->bbox[side ^ 1]))
        {
            if (sp == 0)
            {
                //back at root node and not visible. All done!
                return;
            }

            //Back side next.
            side = bspstack[--sp];
            bspnum = bspstack[--sp];

            bsp = &nodes[bspnum];
        }

        bspnum = bsp->children[side ^ 1];
    }
}

static void R_ClearDrawSegs(void)
{
    ds_p = _g->drawsegs;
}

static void R_ClearClipSegs(void)
{
    memset(solidcol, 0, SCREENWIDTH);
}

//
// R_ClearSprites
// Called at frame start.
//

static void R_ClearSprites(void)
{
    num_vissprite = 0;            // killough
}

//
// RDrawPlanes
// At the end of each frame.
//

static void R_DrawPlanes(void)
{
    for (int i = 0; i < MAXVISPLANES; i++)
    {
        visplane_t *pl = _g->visplanes[i];

        while (pl)
        {
            if (pl->modified)
                R_DoDrawPlane(pl);

            pl = pl->next;
        }
    }
}

//
// R_ClearPlanes
// At begining of frame.
//

static void R_ClearPlanes(void)
{
    int i;

    // opening / clipping determination
 #if 1
    uint32_t *p_fc = (uint32_t*) floorclip;
    uint32_t *p_cc = (uint32_t*) ceilingclip;
    const uint16_t fcv = draw_stopy + 1;
    const uint16_t ccv = draw_starty - 1;
     
    for (i = 0; i < SCREENWIDTH / 2; i++)
    {
      *p_fc++ = fcv | (fcv << 16);
      *p_cc++ = ccv | (ccv << 16);
    }
#else
    for (i = 0; i < SCREENWIDTH; i++)
    {
        floorclip[i] = draw_stopy + 1, ceilingclip[i] = draw_starty - 1;
    }
#endif
    for (i = 0; i < MAXVISPLANES; i++)    // new code -- killough
        for (*_g->freehead = _g->visplanes[i], _g->visplanes[i] = NULL; *_g->freehead;)
            _g->freehead = &(*_g->freehead)->next;

    _g->lastopening = openings;

    // scale will be unit scale at SCREENWIDTH/2 distance
    //basexscale = FixedDiv (viewsin,projection);
    //baseyscale = FixedDiv (viewcos,projection);

    basexscale = FixedMul(viewsin, iprojection);
    baseyscale = FixedMul(viewcos, iprojection);
}

//
// R_RenderView
//
void R_RenderPlayerView(player_t *player)
{

   uint32_t tn;
    
    tn = I_GetTimeMicrosecs();

    R_SetupFrame(player);


    // Clear buffers.
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();

    tn = I_GetTimeMicrosecs() - tn;
    #define PROFILE_DRAWING 0
#if PROFILE_DRAWING
    printf("CL: %d us\r\n", tn);
#endif
    tn = I_GetTimeMicrosecs();

    // The head node is the last node output.
    R_RenderBSPNode(numnodes - 1);
    tn = I_GetTimeMicrosecs() - tn;
#if PROFILE_DRAWING
    printf("RB: %d us\r\n", tn);
#endif
    tn = I_GetTimeMicrosecs();

    R_DrawPlanes();

    tn = I_GetTimeMicrosecs() - tn;

#if PROFILE_DRAWING
    printf("DP: %d us\r\n", tn);
#endif
    tn = I_GetTimeMicrosecs();


    R_DrawMasked();

    tn = I_GetTimeMicrosecs() - tn;
#if PROFILE_DRAWING
    printf("DM: %d us\r\n", tn);
#endif
}

void V_DrawPatchNoScale(int x, int y, const patch_t *patch)
{

    if (false == isOnExternalFlash(patch))
    {
        y -= patch->topoffset;
        x -= patch->leftoffset;
        byte *desttop = (byte*) _g->screens[0].data;
#ifndef HIGHRES
      desttop += (ScreenYToOffset(y) << 1) + x;
  #else
        desttop += (ScreenYToOffset(y)) + x;
#endif
        unsigned int width = patch->width;

        for (unsigned int col = 0; col < width; col++, desttop++)
        {
            const column_t *column = (const column_t*) ((const byte*) patch + (0xFFFFFF & patch->columnofs[col]));
            // step through the posts in a column
            while (column->topdelta != 0xff)
            {
                const byte *source = (const byte*) column + 3;
                byte *dest = desttop + (ScreenYToOffset(column->topdelta));

                // next-hack 2021-03-15: clip to boundary
                int starty = column->topdelta + y;
                int stopy = column->topdelta + y + column->length - 1;
                int count = column->length;

                //
                if (starty < draw_starty)
                {
                    int delta = draw_starty - starty;
                    source += delta;
                    count -= delta;
                }
                if (stopy > draw_stopy)
                {
                    int delta = stopy - draw_stopy;
                    count -= delta;
                }
                //
                #if 0
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
                #else
                if (count > 0)
                {
                    while (count--)
                    {
                        unsigned int color = *source++;
                        *dest = color;
                        dest += SCREENWIDTH_PHYSICAL;
                    }
                }
                #endif
                column = (const column_t*) ((const byte*) column + column->length + 4);
            }
        }
    }
    else
    {
#if 1
        // data in spi flash. we need a different strategy
        //
        // get patch address;
        spiFlashSetAddress((uint32_t) patch);
        // now we need to load patch width, offsets and columnoffset
        short width, height;
        width = spiFlashGetShort();
        height = spiFlashGetShort(); // height, not used, but getting 2 bytes is cheaper than set the address again.
        (void) height;    // remove unused compiler warning
        // remove offset
        x -= spiFlashGetShort();    // patch->leftoffset;
        y -= spiFlashGetShort(); // patch->topoffset;

        // actually we should create a much smaller size. But we reserved a good deed of stack for this

        int columnofs[MAX_PATCH_COLOFFS];
        // read column offs
        // read column offs
        if (width <= MAX_PATCH_COLOFFS)
        {
            spiFlashGetData(columnofs, width * sizeof(columnofs[0]));
        }
        else
        {
            spiFlashGetData(columnofs, MAX_PATCH_COLOFFS * sizeof(columnofs[0]));
        }
        byte *desttop = (byte*) _g->screens[0].data;

        desttop += (ScreenYToOffset(y)) + x;

        int coloffset = 0;
        for (unsigned int colindex = 0; colindex < width; colindex++, desttop++)
        {
            // point to column
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
            spiFlashSetAddress((uint32_t) patch + (0xFFFFFF & columnofs[colindex - coloffset]));

            //
            column_t columnInfo;
            // read column info
            spiFlashGetData(&columnInfo, sizeof(columnInfo));
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

                byte *dest = desttop + (ScreenYToOffset(columnInfo.topdelta));

                // next-hack 2021-03-15: clip to boundary
                int starty = columnInfo.topdelta + y;
                int stopy = columnInfo.topdelta + y + columnInfo.length - 1;
                int count = columnInfo.length;
                //
                if (starty < draw_starty)
                {
                    int delta = draw_starty - starty;
                    source += delta;
                    count -= delta;
                }
                if (stopy > draw_stopy)
                {
                    int delta = stopy - draw_stopy;
                    count -= delta;
                }
                //
                if (count > 0)
                {
                    while (count--)
                    {
                        unsigned int color = *source++;
                        *dest = color;
                        dest += SCREENWIDTH_PHYSICAL;
                    }
                }
                // read again the column information
                // ignore last post byte.
                spiFlashGetByte();
                // read next info data (topdelta and length)
                spiFlashGetData(&columnInfo, sizeof(columnInfo));
            }
        }
#else
        // data in spi flash. we need a different strategy
        //
        // get patch address;
        spiFlashSetAddress((uint32_t) patch);
        patchsizeoffsets_t pso;
        spiFlashGetData((uint32_t) &pso, sizeof(pso));
        // now we need to load patch width, offsets and columnoffset
        short width, height;
        width = pso.width;
        height = pso.height; 
        // remove offset
        x -= pso.leftoffset;
        y -= pso.topoffset;

        // actually we should create a much smaller size. But we reserved a good deed of stack for this

        // read column offs
        // read column offs
        cachePatchColumnOffsetData(patch, width, 0, width);
        byte *desttop = (byte*) _g->screens[0].data;

        desttop += (ScreenYToOffset(y)) + x;
        
        for (unsigned int colindex = 0; colindex < width; colindex++, desttop++)
        {
            uint8_t columnData[MAX_COLUMN_DATA];
            const column_t *column = getColumnData(patch, colindex, columnData);
            // step through the posts in a column
            while (column->topdelta != 0xff)
            {
                const byte *source = (const byte*) column + 3;
                byte *dest = desttop + (ScreenYToOffset(column->topdelta));

                // next-hack 2021-03-15: clip to boundary
                int starty = column->topdelta + y;
                int stopy = column->topdelta + y + column->length - 1;
                int count = column->length;

                //
                if (starty < draw_starty)
                {
                    int delta = draw_starty - starty;
                    source += delta;
                    count -= delta;
                }
                if (stopy > draw_stopy)
                {
                    int delta = stopy - draw_stopy;
                    count -= delta;
                }
                //
                #if 1
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
                #else
                if (count > 0)
                {
                    while (count--)
                    {
                        unsigned int color = *source++;
                        *dest = color;
                        dest += SCREENWIDTH_PHYSICAL;
                    }
                }
                #endif
                column = (const column_t*) ((const byte*) column + column->length + 4);
            }
        }


 #endif
  
    }
}

//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
// killough 4/19/98: made static, cleaned up

static int P_DivlineSide(fixed_t x, fixed_t y, const divline_t *node)
{
    fixed_t left, right;
    return !node->dx ? x == node->x ? 2 :
                       x <= node->x ? node->dy > 0 : node->dy < 0
           :
           !node->dy ? (y) == node->y ? 2 :
                       y <= node->y ? node->dx < 0 : node->dx > 0
           :
           (right = ((y - node->y) >> FRACBITS) * (node->dx >> FRACBITS)) < (left = ((x - node->x) >> FRACBITS) * (node->dy >> FRACBITS)) ? 0 :
           right == left ? 2 : 1;
}

//
// P_CrossSubsector
// Returns true
//  if strace crosses the given subsector successfully.
//
// killough 4/19/98: made static and cleaned up

static boolean P_CrossSubsector(int num)
{
    const seg_t *seg = _g->segs + _g->subsectors[num].firstline;
    int count;
    fixed_t opentop = 0, openbottom = 0;
    const sector_t *front = NULL, *back = NULL;

    for (count = _g->subsectors[num].numlines; --count >= 0; seg++)
    { // check lines
        int linenum = seg->linenum;

        const line_t *line = &_g->lines[linenum];
        divline_t divl;

        // allready checked other side?
        if (_g->linedata[linenum].validcount == (_g->validcount))
            continue;

        _g->linedata[linenum].validcount = _g->validcount;

        if (line->bbox[BOXLEFT] > los.bbox[BOXRIGHT] || line->bbox[BOXRIGHT] < los.bbox[BOXLEFT] || line->bbox[BOXBOTTOM] > los.bbox[BOXTOP] || line->bbox[BOXTOP] < los.bbox[BOXBOTTOM])
            continue;

        // cph - do what we can before forced to check intersection
        if (line->flags & ML_TWOSIDED)
        {

            // no wall to block sight with?
            if ((front = SG_FRONTSECTOR(seg))->floorheight == (back = SG_BACKSECTOR(seg))->floorheight && front->ceilingheight == back->ceilingheight)
                continue;

            // possible occluder
            // because of ceiling height differences
            opentop =
                    front->ceilingheight < back->ceilingheight ? front->ceilingheight : back->ceilingheight;

            // because of floor height differences
            openbottom =
                    front->floorheight > back->floorheight ? front->floorheight : back->floorheight;

            // cph - reject if does not intrude in the z-space of the possible LOS
            if ((opentop >= los.maxz) && (openbottom <= los.minz))
                continue;
        }

        // Forget this line if it doesn't cross the line of sight
        const vertex_t *v1, *v2;

        v1 = &line->v1;
        v2 = &line->v2;

        if (P_DivlineSide(v1->x, v1->y, &los.strace) == P_DivlineSide(v2->x, v2->y, &los.strace))
            continue;

        divl.dx = v2->x - (divl.x = v1->x);
        divl.dy = v2->y - (divl.y = v1->y);

        // line isn't crossed?
        if (P_DivlineSide(los.strace.x, los.strace.y, &divl) == P_DivlineSide(los.t2x, los.t2y, &divl))
            continue;

        // cph - if bottom >= top or top < minz or bottom > maxz then it must be
        // solid wrt this LOS
        if (!(line->flags & ML_TWOSIDED) || (openbottom >= opentop) || (opentop < los.minz) || (openbottom > los.maxz))
            return false;

        // crosses a two sided line
        /* cph 2006/07/15 - oops, we missed this in 2.4.0 & .1;
         *  use P_InterceptVector2 for those compat levels only. */
        fixed_t frac = P_InterceptVector2(&los.strace, &divl);

        if (front->floorheight != back->floorheight)
        {
            fixed_t slope = FixedDiv(openbottom - los.sightzstart, frac);
            if (slope > los.bottomslope)
                los.bottomslope = slope;
        }

        if (front->ceilingheight != back->ceilingheight)
        {
            fixed_t slope = FixedDiv(opentop - los.sightzstart, frac);
            if (slope < los.topslope)
                los.topslope = slope;
        }

        if (los.topslope <= los.bottomslope)
            return false;               // stop

    }
    // passed the subsector ok
    return true;
}

boolean P_CrossBSPNode(int bspnum)
{
    while (!(bspnum & NF_SUBSECTOR))
    {
        const mapnode_t *bsp = nodes + bspnum;

        divline_t dl;
        dl.x = ((fixed_t) bsp->x << FRACBITS);
        dl.y = ((fixed_t) bsp->y << FRACBITS);
        dl.dx = ((fixed_t) bsp->dx << FRACBITS);
        dl.dy = ((fixed_t) bsp->dy << FRACBITS);

        int side, side2;
        side = P_DivlineSide(los.strace.x, los.strace.y, &dl) & 1;
        side2 = P_DivlineSide(los.t2x, los.t2y, &dl);

        if (side == side2)
            bspnum = bsp->children[side]; // doesn't touch the other side
        else         // the partition plane is crossed here
        if (!P_CrossBSPNode(bsp->children[side]))
            return 0;  // cross the starting side
        else
            bspnum = bsp->children[side ^ 1];  // cross the ending side
    }
    return P_CrossSubsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

//
// P_MobjThinker
//

void P_NightmareRespawn(mobj_t *mobj);
void P_XYMovement(mobj_t *mo);
void P_ZMovement(mobj_t *mo);

//
// P_SetMobjState
// Returns true if the mobj is still present.
//

boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
    const state_t *st;

    do
    {
        if (state == S_NULL)
        {
            //mobj->state = (state_t *) S_NULL;
            mobj->state_idx = state;
            P_RemoveMobj(mobj);
            return false;
        }
        //
        //if (mobj->flags & MF_STATIC)
        //{
        //    printf("attempt to set mobjs state on static\r\n");
        //    return false;
        //}
        st = &states[state];
        //mobj->state = st;
        mobj->state_idx = state;
        mobj->tics = st->tics;
        mobj->sprite = st->sprite;
        mobj->frame = st->frame;

        // Modified handling.
        // Call action functions when the state is set
        if (st->action)
        {
            if (!(_g->player.cheats & CF_ENEMY_ROCKETS))
            {
                st->action(mobj);
            }
            else
            {
                if (getMobjInfo(mobj)->missilestate && (state >= getMobjInfo(mobj)->missilestate) && (state < getMobjInfo(mobj)->painstate))
                    A_CyberAttack(mobj);
                else
                    st->action(mobj);
            }
        }

        state = st->nextstate;

    } while (!mobj->tics);

    return true;
}

void P_MobjThinker(mobj_t *mobj)
{
    // killough 11/98:
    // removed old code which looked at target references
    // (we use pointer reference counting now)
    if (mobj->flags & MF_STATIC)
        printf("Thinker on static object!\r\n");
    // momentum movement
    if (mobj->momx || mobj->momy || (mobj->flags & MF_SKULLFLY )) // it was momx | momy, but it should have been ||
    {
        P_XYMovement(mobj);
        if (mobj->thinker.function != P_MobjThinker) // cph - Must've been removed
            return;       // killough - mobj was removed
    }

    if (mobj->z != mobj->floorz || mobj->momz)
    {
        P_ZMovement(mobj);
        if (mobj->thinker.function != P_MobjThinker) // cph - Must've been removed
            return;       // killough - mobj was removed
    }

    // cycle through states,
    // calling action functions at transitions

    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic

        if (!mobj->tics)
            if (!P_SetMobjState(mobj, getMobjState(mobj)->nextstate))
                return;     // freed itself
    }
    else
    {

        // check for nightmare respawn

        if (!(mobj->flags & MF_COUNTKILL ))
            return;

        if (!_g->respawnmonsters)
            return;

        mobj->movecount++;

        if (mobj->movecount < 12 * 35)
            return;

        if (_g->leveltime & 31)
            return;

        if (P_Random(__FILE__, __LINE__, __FUNCTION__) > 4)
            return;

        P_NightmareRespawn(mobj);
    }

}

//
// P_RunThinkers
//
// killough 4/25/98:
//
// Fix deallocator to stop using "next" pointer after node has been freed
// (a Doom bug).
//
// Process each thinker. For thinkers which are marked deleted, we must
// load the "next" pointer prior to freeing the node. In Doom, the "next"
// pointer was loaded AFTER the thinker was freed, which could have caused
// crashes.
//
// But if we are not deleting the thinker, we should reload the "next"
// pointer after calling the function, in case additional thinkers are
// added at the end of the list.
//
// killough 11/98:
//
// Rewritten to delete nodes implicitly, by making currentthinker
// external and using P_RemoveThinkerDelayed() implicitly.
//

void P_RunThinkers(void)
{
    thinker_t *th = getThinkerNext(&thinkercap);
    thinker_t* th_end = &thinkercap;

    while(th != th_end)
    {
        thinker_t* th_next = getThinkerNext(th);
        if(th->function)
        th->function(th);

        th = th_next;
    }
}

