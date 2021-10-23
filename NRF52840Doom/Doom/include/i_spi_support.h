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
 *  DESCRIPTION:
 *  next-hack: Some wrapper functions, to use SPI routines.
 *  Memory mapped code has been removed.
 */
#ifndef I_SPI_SUPPORT_H_
#define I_SPI_SUPPORT_H_
#include <stdint.h>
#include "string.h"
//
extern uint8_t *currentSpiAddress;
static inline uint8_t spiFlashGetByte()
{
    return *currentSpiAddress++;
}
static inline short spiFlashGetShort()
{
   uint16_t s = *currentSpiAddress++;
    s |= (*currentSpiAddress++ << 8);
    return (short) s; 
    /*
    short s = *((short*) currentSpiAddress);
    currentSpiAddress+=2;
    return s;*/
}
static inline uint8_t spiFlashGetByteFromAddress(const void *addr)
{
  currentSpiAddress = (void*) addr;
  return spiFlashGetByte();
}
static inline void* spiFlashGetData(void *dest, unsigned int length)
{
  uint32_t *b  = (void*) currentSpiAddress;
  memcpy(dest, b, length);
  currentSpiAddress += length;
  return dest;
}
static inline uint32_t spiFlashSetAddress(uint32_t address)
{
    currentSpiAddress = (uint8_t*) address;
}
#endif /* SOURCE_I_SPI_SUPPORT_H_ */
