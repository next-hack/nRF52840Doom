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
 * Description: Keyboard handling functions for different types
 * (parallel, i2c and radio). For radio keyboard, this handles
 * also audio data transmission.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>
#include "main.h"
//Keys
#if KEYBOARD == PARALLEL_KEYBOARD
    #define KEY_ALT 0x08
    #define KEY_FIRE 0x10
    #define KEY_USE 0x40
    #define KEY_CHGW 0x20
    #define KEY_UP 0x01
    #define KEY_DOWN 0x02
    #define KEY_LEFT 0x04
    #define KEY_RIGHT 0x80
#elif KEYBOARD == I2C_KEYBOARD
    #define KEY_ALT 0x10 
    #define KEY_FIRE 0x20
    #define KEY_USE 0x40
    #define KEY_CHGW 0x80 
    #define KEY_UP 0x02
    #define KEY_DOWN 0x04 
    #define KEY_LEFT 0x08 
    #define KEY_RIGHT 0x01 
#elif KEYBOARD == I2C_GAMEPAD
    // lower key_t byte
    #define KEY_MAP (1 << GAMEPADQT_BUTTON_SELECT) // 0x0001
    #define KEY_ALT (1 << GAMEPADQT_BUTTON_B) // 0x0002
    #define KEY_USE (1 << GAMEPADQT_BUTTON_Y) // 0x0004
    #define KEY_LEFT 0x0008
    #define KEY_DOWN 0x0010
    #define KEY_FIRE (1 << GAMEPADQT_BUTTON_A) // 0x0020
    #define KEY_CHGW (1 << GAMEPADQT_BUTTON_X) // 0x0040
    #define KEY_UP 0x0080
    // upper key_t byte
    #define KEY_MENU (1 << (GAMEPADQT_BUTTON_START+8)) // 0x0100
    #define KEY_RIGHT 0x0200
#elif KEYBOARD == RADIO_KEYBOARD
    #define KEY_ALT  (1 << 1)    
    #define KEY_FIRE (1 << 2) 
    #define KEY_USE (1 << 0)
    #define KEY_CHGW (1<< 7)  
    #define KEY_UP (1 << 6)
    #define KEY_DOWN (1 << 4)  
    #define KEY_LEFT (1 << 3)  
    #define KEY_RIGHT (1 << 5)  
#endif

void initKeyboard();
void getKeys(key_t *keys);
#if KEYBOARD == RADIO_KEYBOARD 
    void disableWirelessAudio();
    void restoreWirelessAudio();
    void initWirelessAudio();
#else
    // No wireless keyboard? Empty macros
    #define disableWirelessAudio()
    #define restoreWirelessAudio()
    #define initWirelessAudio()
#endif
#endif