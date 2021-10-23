/*********************************************************************
* 
*  Keyboard and audio for nRF52840 Doom by Nicola Wrachien (next-hack)
*
*  Simple.
*
*  Copyright 2021, Nicola Wrachien
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
*********************************************************************/
#ifndef MAIN_H
#define MAIN_H
#include <stdbool.h>
#include "nrf.h"
// configuration stuff.
#define RF_ADDRESS {'D', 'O', 'O', 'M'};
#define RF_ADDRESS_PREFIX 0xAA
#define NO_RF_TIMEOUT_MICROS 10000  // if we do not receive a RF audio packet within 10 ms, we mute everything
// uart debug pins
#define PIN_TXD        (12)
#define PIN_RXD        (20)
#define PWM_AUDIO_OUT_PIN (24)
//
#endif