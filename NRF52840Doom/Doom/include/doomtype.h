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
 *      Simple basic typedefs, isolated here to make it easier
 *       separating modules.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdbool.h>
#ifndef __BYTEBOOL__
#define __BYTEBOOL__
/* Fixed to use builtin bool type with C++. */
#if 0
	#ifdef __cplusplus
		typedef bool boolean;
	#else
		typedef enum {false, true} boolean;
	#endif
#else
// next-hack
//typedef enum {false, true} boolean_t;
typedef unsigned char boolean;
#endif
typedef unsigned char byte;
#endif
// 2021-02-13 next-hack. Each pointer is 4 bytes long, and we use a lot of them.
// But since we will have less than 256kB of RAM, and since pointers to struct, int
// other zmalloc'ed things are 4-bytes aligned, we can just use one unsigned short
// In this way, we save 2 bytes per each pointer.

//e6y
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* cph - Wrapper for the long long type, as Win32 used a different name.
 * Except I don't know what to test as it's compiler specific
 * Proff - I fixed it */

#ifdef __WINS__
	#define int_64_t  __int64
	#define uint_64_t unsigned __int64
#else
#define int_64_t  long long
#define uint_64_t unsigned long long
#endif

//typedef _int64 int_64_t;
//typedef unsigned _int64 uint_64_t;
// define compiled-specific long-long contstant notation here
#define LONGLONG(num) (uint_64_t)num
#undef PATH_MAX
#define PATH_MAX 1024

#ifdef __GNUC__
#define CONSTFUNC __attribute__((const))
#define PUREFUNC __attribute__((pure))
#define NORETURN __attribute__ ((noreturn))
#else
#define CONSTFUNC
#define PUREFUNC
#define NORETURN
#endif

/* CPhipps - use limits.h instead of depreciated values.h */
#include <limits.h>

/* cph - from v_video.h, needed by gl_struct.h */
enum patch_translation_e
{
    VPT_NONE = 0, // Normal
    VPT_FLIP = 1, // Flip image horizontally
    VPT_TRANS = 2, // Translate image via a translation table
    VPT_STRETCH = 4, // Stretch to compensate for high-res
};

#endif
