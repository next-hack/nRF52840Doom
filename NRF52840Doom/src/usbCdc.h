/**
 *  USB CDC implementation for Doom Port to nRF52840.
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
 *  Simple and lightweight USB CDC implementation. There are some 
 *  restriction e.g. lack of support of automatic ZLP generation 
 *  for payload large exactly 64 bytes. However this CDC is used
 *  for yModem only, where only one char is sent at most.
 *
 */
#ifndef USBCDC_H
#define USBCDC_H
#include <stdint.h>
#include "usb.h"
void usbCdcClassISR();
int usbCdcGetChar();
void usbCdcEnableOut();
int usbCdcInit();
int usbCdcPutChar(char c);
int usbCdcGetChar();
void usbCdcFlush();
#endif