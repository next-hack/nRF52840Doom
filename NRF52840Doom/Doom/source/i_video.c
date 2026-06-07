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
 *  DOOM graphics stuff.
 *  next-hack: and also key handling, don't ask me why they were put in
 *  i_video.c. Key handling and graphics I/O modified according HW...
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>
#include "main.h"

#include "doomstat.h"
#include "doomdef.h"
#include "doomtype.h"
#include "v_video.h"
#include "r_draw.h"
#include "d_main.h"
#include "d_event.h"
#include "i_video.h"
#include "i_sound.h"
#include "m_menu.h"
#include "z_zone.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "st_stuff.h"
#include "lprintf.h"
#include "doomdef.h"

#include "global_data.h"
#include "display.h"
#include "graphics.h"
#include "keyboard.h"

#define NO_PALETTE_CHANGE 255

uint16_t palette[256];
bool toggleBacklight = false;

key_t keysDown()
{
  key_t buttons = 0;
  getKeys(&buttons);
  return buttons;
}
//
// I_StartTic
//
void I_StartTic(void)
{
    static uint16_t oldGameKeyState = 0;
    // get which keys are currently pressed
    key_t hwKeyState = keysDown();
    // translate between the hardware key state and the game key state
    uint16_t gameKeyState = 0;

    if (hwKeyState & KEY_ALT)
    {
        if (hwKeyState & KEY_RIGHT)
            gameKeyState |= 1 << KEYD_SR;
        if (hwKeyState & KEY_LEFT)
            gameKeyState |= 1 << KEYD_SL;
        if (hwKeyState & KEY_USE)
#if KEYBOARD == I2C_GAMEPAD
            // USE negates always run setting, so with a dedicated MENU key the USE button
            // can be freed from the ALT combo allowing it to be dual-purpose USE and SPEED button
            gameKeyState |= 1 << KEYD_USE;
#else
            gameKeyState |= 1 << KEYD_MENU;
#endif
        if (hwKeyState & KEY_CHGW)
            gameKeyState |= 1 << KEYD_CHGWDOWN;
#if OLD_KEYMAP // it was not wise setting ALT with fire to trigger automap...
        if (hwKeyState & KEY_FIRE)
            gameKeyState |= 1 << KEYD_MAP1;
#else
        if (hwKeyState & KEY_FIRE)
            gameKeyState |= 1 << KEYD_FIRE;
#endif
        if (! (hwKeyState & (KEY_RIGHT | KEY_LEFT | KEY_USE | KEY_CHGW | KEY_FIRE)))
            gameKeyState |= 1 << KEYD_ALT;
    }
    else
    {
        if (hwKeyState & KEY_RIGHT)
            gameKeyState |= 1 << KEYD_RIGHT;
        if (hwKeyState & KEY_LEFT)
            gameKeyState |= 1 << KEYD_LEFT;
        if (hwKeyState & KEY_FIRE)
            gameKeyState |= 1 << KEYD_FIRE;
#if KEYBOARD == I2C_GAMEPAD
        // remap USE+CHGW from MAP1 in other keymaps to CHGWDOWN
        if ((hwKeyState & (KEY_USE | KEY_CHGW)) == (KEY_USE | KEY_CHGW))
            gameKeyState |= 1 << KEYD_CHGWDOWN;
        else
        {
#endif
            if (hwKeyState & KEY_USE)
                gameKeyState |= 1 << KEYD_USE;
            if (hwKeyState & KEY_CHGW)
                gameKeyState |= 1 << KEYD_CHGW;
#if KEYBOARD == I2C_GAMEPAD
        }
#endif
    }

    if (hwKeyState & KEY_UP)
        gameKeyState |= 1 << KEYD_UP;
    if (hwKeyState & KEY_DOWN)
        gameKeyState |= 1 << KEYD_DOWN;
#if KEYBOARD == I2C_GAMEPAD
    // Gamepad has 10 total buttons, with extra byte for key data can have MAP1 and MENU as dedicated buttons
    if (hwKeyState & KEY_MAP)
        gameKeyState |= 1 << KEYD_MAP1;
    if (hwKeyState & KEY_MENU)
        gameKeyState |= 1 << KEYD_MENU;
#endif
#if !OLD_KEYMAP && KEYBOARD != I2C_GAMEPAD
    // automap is now enabled when use and change weapon are pressed at the same time (or chgwdown for i2c gamepad)
    if ((hwKeyState & (KEY_USE | KEY_CHGW)) == (KEY_USE | KEY_CHGW))
        gameKeyState |= 1 << KEYD_MAP1;
#endif
    // Get which keys have changed since last time
    uint16_t keys_changed = oldGameKeyState ^ gameKeyState;

    event_t ev;
    ev.type = ev_keydown;
    for (int i = 0; i < NUMKEYS; i++)
    {
        if ((1 & (keys_changed >> i)) && (1 & (gameKeyState >> i)))
        {
            ev.data1 = i;
            D_PostEvent(&ev);
        }
    }
    ev.type = ev_keyup;
    for (int i = 0; i < NUMKEYS; i++)
    {
        if ((1 & (keys_changed >> i)) && !(1 & (gameKeyState >> i)))
        {
            ev.data1 = i;
            D_PostEvent(&ev);
        }
    }
    oldGameKeyState = gameKeyState;

#if !MINEWDONGLE
    // handle onboard clue buttons for gamma adjust and toggling backlight
    static bool aBtnWasDown = false;
    static bool bBtnWasDown = false;
    bool aDown = !(GPIO_PORT(PORT_NUM_BTN_A)->IN & (1 << PIN_NUM_BTN_A));
    bool bDown = !(GPIO_PORT(PORT_NUM_BTN_B)->IN & (1 << PIN_NUM_BTN_B));

    if (aDown && bDown && (!aBtnWasDown || !bBtnWasDown))
      toggleBacklight = true;
    else if (aDown && !aBtnWasDown)
        M_ChangeGamma(0);
    else if (bDown && !bBtnWasDown)
        M_ChangeGamma(1);

    aBtnWasDown = aDown;
    bBtnWasDown = bDown;
#endif
}

boolean I_StartDisplay(void)
{
    _g->screens[0].data = displayData.displayFrameBuffer[displayData.workingBuffer];
    // Same with base row offset.
    drawvars.byte_topleft = _g->screens[0].data;
    return true;
}

/////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
// Palette stuff.
//

static void I_UploadNewPalette(int pal)
{
    _g->current_palette = palette;
    //
    if (_g->gamma)
    {
   
        for (int i = 0; i < 256; i++)
        {
            uint16_t r = gammatable[_g->gamma][p_wad_immutable_flash_data->palette_lump[pal * 256 * 3 + 3 * i]] >> 3;
            uint16_t g = gammatable[_g->gamma][p_wad_immutable_flash_data->palette_lump[pal * 256 * 3 + 3 * i + 1]] >> 2;
            uint16_t b = gammatable[_g->gamma][p_wad_immutable_flash_data->palette_lump[pal * 256 * 3 + 3 * i + 2]] >> 3;
            uint16_t rgb = (r << (6 + 5)) | (g << 5) | (b << (0));
            rgb = (rgb >> 8) | (rgb << 8);
            _g->current_palette[i] = rgb;
        }
    }
    else
    {   // save some cycles for those playing with 0 gamma.
           for (int i = 0; i < 256; i++)
        {
            uint16_t r = p_wad_immutable_flash_data->palette_lump[pal * 256 * 3 + 3 * i] >> 3;
            uint16_t g = p_wad_immutable_flash_data->palette_lump[pal * 256 * 3 + 3 * i + 1] >> 2;
            uint16_t b = p_wad_immutable_flash_data->palette_lump[pal * 256 * 3 + 3 * i + 2] >> 3;
            uint16_t rgb = (r << (6 + 5)) | (g << 5) | (b << (0));
            rgb = (rgb >> 8) | (rgb << 8);
            _g->current_palette[i] = rgb;
        }
    }
    displayData.pPalette = _g->current_palette;
}

//////////////////////////////////////////////////////////////////////////////
// Graphics API
//

void I_FinishUpdateBlock(void)
{
    I_UploadNewPalette(_g->newpal);

    startDisplayRefresh(displayData.workingBuffer);
    displayData.workingBuffer = 1 - displayData.workingBuffer;
}

//
// I_SetPalette
//
void I_SetPalette(int pal)
{
    _g->newpal = pal;
}

void handleDisplayBacklightToggle() {
    static bool backlightOn = true;

    if (toggleBacklight)
    {
        toggleBacklight = false;
        backlightOn = !backlightOn;
        SetBacklight(backlightOn);
    }
}