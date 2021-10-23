/**
 *  QSPI flash driver for Doom Port to nRF52840.
 *
 *  Copyright (C) 2021 by Nicola Wrachien (next-hack in the comments)
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
 *  QSPI driver. Performs initialization, by enabling QSPI mode,
 *  also setting maximum drive strength, and flash size detection
 *
 */
#ifndef QSPI_H_
#define QSPI_H_
#include <stdint.h>
#include "nrf.h"
void qspiFlashProgram(uint32_t address, uint8_t* data, uint32_t size);
void qspiFlashErasePage64k(uint32_t address);
void qspiFlashErasePage4k(uint32_t address);
void qspiInit();
uint32_t qspiFlashGetSize(void);

#endif