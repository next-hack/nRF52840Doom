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
 *  Description: main.h header file. Use this to configure the hardware
 *  as you wish
 */
#ifndef MAIN_H
#define MAIN_H
#include <stdbool.h>
#include "nrf.h"
#include "graphics.h"

// 1: BLE DONGLE, 0 = ADAFRUIT CLUE
#define MINEWDONGLE 1
#define DEBUG_OUT_PRINTF 0
//various configurations
#define CACHE_ALL_COLORMAP_TO_RAM 0 // this wastes 8.25 more kB, and typically 900 us are saved. Remember to reduce zone memory!
#define USE_SPI_FOR_QSPI_INIT 1    
#define GAMMA_CORRECTION 1      // This will create an almost linear gamma.
#define GAMMA_SETTINGS 0xf0, 0x7, 0xf, 0x0, 0x0, 0x30, 0x28, 0x54, 0x56, 0x3e, 0x1e, 0x1f, 0x37, 0x38   // used if GAMMA_CORRECTION is 1
#define RF_ADDRESS {'D', 'O', 'O', 'M'};
#define RF_ADDRESS_PREFIX 0xAA

#define DEBUG_SETUP 0
#if DEBUG_SETUP
#define AUTOSTART_GAME 0
    #define START_MAP 7
    #define SHOW_FPS true
#else
    #define START_MAP 1
    #define SHOW_FPS false
#endif
// utilities for accessing ports easily, regardless they are in P0 and P1
#define PORT(t) NRF_P ## t 
#define GPIO_PORT(port) PORT(port) 
#define P0 0
#define P1 1
//
#define DISPLAY_USES_RESET_INSTEAD_OF_NCS 1 // for displays without CS...
// note: for debug, do not use shorts, otherwise the display data is likely to be trashed. However the frame rate will be slower.
#define DISPLAY_SPIM_USES_SHORTS 1
//
//
#define I2C_KEYBOARD 1
#define PARALLEL_KEYBOARD 2
#define RADIO_KEYBOARD 3
//
#define PIN_TXD        (5)
#define PIN_RXD        (4)
//
#if MINEWDONGLE 
    #define ADAFRUIT_CLUE_DISPLAY 0
    #define INTERNAL_CLUE_FLASH 0
    #define EXTERNAL_CLUE_FLASH 0
    #define KEYBOARD RADIO_KEYBOARD 

#else
    #define ADAFRUIT_CLUE_DISPLAY 1
    #define EXTERNAL_CLUE_FLASH 0
    #define INTERNAL_CLUE_FLASH (1 - EXTERNAL_CLUE_FLASH )
    #define KEYBOARD RADIO_KEYBOARD 
#endif
#if KEYBOARD == I2C_KEYBOARD
    #if MINEWDONGLE
        #error Only radio keyboard can be used with the dongle!
    #endif
    #define EXPANDER_I2C_ADDRESS 0x27            // for gamepad

    #define PORT_NUM_I2C_SCL P0
    #define PORT_NUM_I2C_SDA P0

    #define PIN_NUM_I2C_SCL 25
    #define PIN_NUM_I2C_SDA 24

    #define MCP23008_IODIR 0
    #define MCP23008_IPOL 1
    #define MCP23008_GPINTEN 2
    #define MCP23008_DEFVAL 3
    #define MCP23008_INTCON 4
    #define MCP23008_IOCON 5
    #define MCP23008_GPPU 6
    #define MCP23008_INTF 7
    #define MCP23008_INTCAP 8
    #define MCP23008_GPIO 9
    #define MCP23008_OLAT 10
#endif
#if INTERNAL_CLUE_FLASH 
    // port config for QSPI
    //
    #define PORT_NUM_QSPI_D0 0
    #define PORT_NUM_QSPI_D1 0
    #define PORT_NUM_QSPI_D2 0
    #define PORT_NUM_QSPI_D3 0
    #define PORT_NUM_QSPI_NCS 0
    #define PORT_NUM_QSPI_CLK 0
    //
    #define PIN_QSPI_D0 17
    #define PIN_QSPI_D1 22
    #define PIN_QSPI_D2 23
    #define PIN_QSPI_D3 21
    #define PIN_QSPI_CLK 19
    #define PIN_QSPI_NCS 20
#elif EXTERNAL_CLUE_FLASH
    //
    #define PORT_NUM_QSPI_D0 0
    #define PORT_NUM_QSPI_D1 0
    #define PORT_NUM_QSPI_D2 0
    #define PORT_NUM_QSPI_D3 0
    #define PORT_NUM_QSPI_NCS 0
    #define PORT_NUM_QSPI_CLK 1
    //
    #define PIN_QSPI_D0 31
    #define PIN_QSPI_D1 6
    #define PIN_QSPI_D2 26
    #define PIN_QSPI_D3 3
    #define PIN_QSPI_CLK 7
    #define PIN_QSPI_NCS 8
#elif MINEWDONGLE
    #define PORT_NUM_QSPI_D0 1
    #define PORT_NUM_QSPI_D1 0
    #define PORT_NUM_QSPI_D2 0
    #define PORT_NUM_QSPI_D3 1
    #define PORT_NUM_QSPI_NCS 0
    #define PORT_NUM_QSPI_CLK 0
    //
    #define PIN_QSPI_D0 6  // righmost button
    #define PIN_QSPI_D1 1  // XTAL left pin
    #define PIN_QSPI_D2 0  // XTAL right pin
    #define PIN_QSPI_D3 9 // green RGB LED

    #define PIN_QSPI_CLK 19 // reset btn
    #define PIN_QSPI_NCS 6  // Green LED
#else
    #error you must define QSPI pins!
#endif
#if KEYBOARD == PARALLEL_KEYBOARD
    #if EXTERNAL_CLUE_FLASH
        #error parallel keyboard and external flash not supported on clue!
    #endif
    #define PIN_KEY_UP  7 
    #define PIN_KEY_DOWN  31 
    #define PIN_KEY_LEFT 8  
    #define PIN_KEY_RIGHT 3
    #define PIN_KEY_USE 29
    #define PIN_KEY_CHGW 6 
    #define PIN_KEY_ALT 2
    #define PIN_KEY_FIRE 26
    //
    #define PORT_NUM_KEY_UP P1
    #define PORT_NUM_KEY_DOWN P0
    #define PORT_NUM_KEY_LEFT P0
    #define PORT_NUM_KEY_RIGHT P0
    #define PORT_NUM_KEY_USE P0
    #define PORT_NUM_KEY_CHGW P0
    #define PORT_NUM_KEY_ALT P1
    #define PORT_NUM_KEY_FIRE P0  
#endif
#if KEYBOARD == PARALLEL_KEYBOARD
    extern  const uint8_t pins[];
    extern  NRF_GPIO_Type* const ports[];
    extern  const uint8_t keys[]; 
#endif
//
// required to enable QSPI in some devices
//
#define PIN_FLASHSPI_MOSI PIN_QSPI_D0
#define PIN_FLASHSPI_MISO PIN_QSPI_D1
#define PIN_FLASHSPI_HOLD PIN_QSPI_D2
#define PIN_FLASHSPI_WP PIN_QSPI_D3
#define PIN_FLASHSPI_NCS PIN_QSPI_NCS
#define PIN_FLASHSPI_CLK PIN_QSPI_CLK
//
// 
//
#define PORT_NUM_FLASHSPI_MOSI PORT_NUM_QSPI_D0
#define PORT_NUM_FLASHSPI_MISO PORT_NUM_QSPI_D1
#define PORT_NUM_FLASHSPI_HOLD PORT_NUM_QSPI_D2
#define PORT_NUM_FLASHSPI_WP PORT_NUM_QSPI_D3
#define PORT_NUM_FLASHSPI_NCS PORT_NUM_QSPI_NCS
#define PORT_NUM_FLASHSPI_CLK PORT_NUM_QSPI_CLK
//
#if ADAFRUIT_CLUE_DISPLAY
#define DISPLAY_INVERT_XY 1

#define PIN_DISPLAY_MOSI 15
#define PIN_DISPLAY_SCK 14
#define PIN_DISPLAY_NCS 12
#define PIN_DISPLAY_DC 13
#define PIN_DISPLAY_RESET 3  // P1
#define PIN_DISPLAY_BACKLIGHT 5  // P1

#define PORT_NUM_DISPLAY_MOSI P0
#define PORT_NUM_DISPLAY_SCK P0
#define PORT_NUM_DISPLAY_NCS P0
#define PORT_NUM_DISPLAY_DC P0
#define PORT_NUM_DISPLAY_RESET P1  // P1
#define PORT_NUM_DISPLAY_BACKLIGHT P1  // P1
#elif MINEWDONGLE
//
#define PIN_DISPLAY_MOSI 12   // Red LED, RGB
#define PIN_DISPLAY_SCK 8  //Blue LED, RGB
#define PIN_DISPLAY_DC PIN_QSPI_D1   // to 7474
#define PIN_DISPLAY_RESET PIN_QSPI_D0  // to 7474

#define PORT_NUM_DISPLAY_MOSI P0
#define PORT_NUM_DISPLAY_SCK P0
#define PORT_NUM_DISPLAY_DC PORT_NUM_QSPI_D1
#define PORT_NUM_DISPLAY_RESET PORT_NUM_QSPI_D0  // P1


#endif
#if DISPLAY_SPIM_USES_SHORTS
/* 
 *   If we use SHORTS for SPIM3, the maximum frame rate will be 0.4 fps higher. 
 *   This has the drawback that if somehow the easy DMA is not fed regularly,
 *   with the correct pointer, the display will go out of sync. 
 *   This occurs if we disable ISRs or if we stop for debugging.
 *   The easiest way would be to suspend SPIM3, but this has an hardware bug,
 *   and this operation is not reliable.
 *   If we really need to disable all the ISR, then we must wait for the display
 *   to be drawn. Otherwise, we can still allow for display to go on (SPIM3
 *   has ISR pri of 0), and disable other ISR, which are set to 1 or higher
 */ 
static inline void nh_enable_irq()
{
    __enable_irq();
    // NRF_SPIM3->TASKS_RESUME = 1;
}
static inline void nh_disable_irq()
{
    //    NRF_SPIM3->TASKS_SUSPEND = 1;
    // guess what? can't suspend SPIM3 because it is bugged.
    while(displayData.dmaBusy);
    __disable_irq();
}
#endif


#endif