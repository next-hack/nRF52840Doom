/*
 *  This file contains display-specific functions.
 *
 *  Display:
 *  These functions support a 240x240 SPI display, controller ST7789.
 *
 *  Original code: Adafruit tft library?
 *
 *  Modified by Nicola Wrachien (next-hack in the comments) 2018-2021
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program  is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef DISPLAYH
#define DISPLAYH

#ifdef __cplusplus
extern "C"
{
#endif
#include "nrf.h"
#include "main.h"
#include "delay.h"

// Useful macros
#if MINEWDONGLE
    static inline void DISPLAY_LATCH()
    {
        // CS HIGH
        GPIO_PORT(PORT_NUM_QSPI_NCS)->OUTSET = (1 << PIN_QSPI_NCS);
        // CLOCK LOW
        GPIO_PORT(PORT_NUM_QSPI_CLK)->OUTCLR = (1 << PIN_QSPI_CLK);
        delay(1);
        // CLOCK HIGH
        GPIO_PORT(PORT_NUM_QSPI_CLK)->OUTSET = (1 << PIN_QSPI_CLK);
        delay(1);
        // RESTORE CLOCK LOW
        GPIO_PORT(PORT_NUM_QSPI_CLK)->OUTCLR = (1 << PIN_QSPI_CLK);
        delay(1);
    }
// for those stupid boards that does not route out NCS...
#if DISPLAY_USES_RESET_INSTEAD_OF_NCS
    static inline void DISPLAY_DC_HIGH()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_RESET)->OUTSET = (1 << PIN_DISPLAY_RESET);
        GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTSET = (1 << PIN_DISPLAY_DC);
        DISPLAY_LATCH();
    }
    static inline void DISPLAY_DC_LOW()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_RESET)->OUTSET = (1 << PIN_DISPLAY_RESET);
        GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTCLR = (1 << PIN_DISPLAY_DC);
        DISPLAY_LATCH();
    }
    static inline void DISPLAY_RES_HIGH()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_RESET)->OUTSET = (1 << PIN_DISPLAY_RESET);
        GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTSET = (1 << PIN_DISPLAY_DC);
        DISPLAY_LATCH();
    }
    static inline void DISPLAY_RES_LOW()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_RESET)->OUTCLR = (1 << PIN_DISPLAY_RESET);
        GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTSET = (1 << PIN_DISPLAY_DC);
        DISPLAY_LATCH();
    }
// chip select always low, if defined
#ifdef PIN_DISPLAY_NCS
#define DISPLAY_NCS_LOW() GPIO_PORT(PORT_NUM_DISPLAY_NCS)->OUTCLR = (1 << PIN_DISPLAY_NCS)
#else
#define DISPLAY_NCS_LOW()
#endif
#define DISPLAY_NCS_HIGH() DISPLAY_NCS_LOW()
#else
#warning code not tested!
    static inline void DISPLAY_DC_HIGH()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTSET = (1 << PIN_DISPLAY_DC);
        DISPLAY_LATCH();
    }
    static inline void DISPLAY_DC_LOW()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTCLR = (1 << PIN_DISPLAY_DC);
        DISPLAY_LATCH();
    }
    static inline void DISPLAY_NCS_HIGH()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_NCS)->OUTSET = (1 << PIN_DISPLAY_NCS);
        DISPLAY_LATCH();
    }
    static inline void DISPLAY_NCS_LOW()
    {
        GPIO_PORT(PORT_NUM_DISPLAY_NCS)->OUTCLR = (1 << PIN_DISPLAY_NCS);
        DISPLAY_LATCH();
    }
#endif
#else
#define DISPLAY_DC_HIGH() GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTSET = (1 << PIN_DISPLAY_DC)
#define DISPLAY_DC_LOW() GPIO_PORT(PORT_NUM_DISPLAY_DC)->OUTCLR = (1 << PIN_DISPLAY_DC)
#define DISPLAY_RES_HIGH() GPIO_PORT(PORT_NUM_DISPLAY_RESET)->OUTSET = (1 << PIN_DISPLAY_RESET)
#define DISPLAY_RES_LOW() GPIO_PORT(PORT_NUM_DISPLAY_RESET)->OUTCLR = (1 << PIN_DISPLAY_RESET)
// for those stupid boards that does not route out NCS...
#if DISPLAY_USES_RESET_INSTEAD_OF_NCS
// chip select always low, if defined
#ifdef PIN_DISPLAY_NCS
#define DISPLAY_NCS_LOW() GPIO_PORT(PORT_NUM_DISPLAY_NCS)->OUTCLR = (1 << PIN_DISPLAY_NCS)
#else
#define DISPLAY_NCS_LOW()
#endif
#define DISPLAY_NCS_HIGH() DISPLAY_NCS_LOW()
#else
#define DISPLAY_NCS_HIGH() GPIO_PORT(PORT_NUM_DISPLAY_NCS)->OUTSET = (1 << PIN_DISPLAY_NCS)
#define DISPLAY_NCS_LOW() GPIO_PORT(PORT_NUM_DISPLAY_NCS)->OUTCLR = (1 << PIN_DISPLAY_NCS)
#endif
#endif

// screen size
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define MAXROWS (SCREEN_HEIGHT / 8)
#define MAXCOLS (SCREEN_WIDTH / 8)
    void UpdateDisplay(void);
    void DisplayInit(void);
    void BeginUpdateDisplay(void);
    void EndUpdateDisplay(void);
    void SelectDisplay(void);
    void DisplayWriteData(uint8_t value);
    void initDisplaySpi();

#ifdef __cplusplus
}
#endif

#endif