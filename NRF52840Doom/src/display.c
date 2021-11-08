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
#include "display.h"
#include "ST77XX_registers.h"
#include "delay.h"
#include "nrf.h"
#include "main.h"
#if DISPLAY_INVERT_XY
#define MADCTL_CONFIG (ST77XX_MADCTL_MV | ST77XX_MADCTL_MY)
#else
#define MADCTL_CONFIG (0x0)
#endif
#define nop() __asm__ volatile("nop");
#define CMD_ARGS_HAVE_DELAY ST_CMD_DELAY
void initDisplaySpi()
{
    // NO MISO
    NRF_SPIM3->PSEL.MISO = SPI_PSEL_MISO_CONNECT_Disconnected << SPI_PSEL_MISO_CONNECT_Pos;
    // Clock and MOSI USED
    NRF_SPIM3->PSEL.SCK = (PORT_NUM_DISPLAY_SCK << SPIM_PSEL_SCK_PORT_Pos) | (SPI_PSEL_SCK_CONNECT_Connected << SPI_PSEL_MISO_CONNECT_Pos) |
                          (PIN_DISPLAY_SCK << SPIM_PSEL_SCK_PIN_Pos);
    NRF_SPIM3->PSEL.MOSI = (PORT_NUM_DISPLAY_MOSI << SPIM_PSEL_MOSI_PORT_Pos) | (SPI_PSEL_MOSI_CONNECT_Connected << SPI_PSEL_MISO_CONNECT_Pos) |
                           (PIN_DISPLAY_MOSI << SPIM_PSEL_MOSI_PIN_Pos);
    //
    NRF_SPIM3->FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M32 << SPIM_FREQUENCY_FREQUENCY_Pos;
// ORC: don't care. Config: good the reset value.
#if DISPLAY_USES_RESET_INSTEAD_OF_NCS
    // hack for those stupid boards not using NCS. Damn it.
    NRF_SPIM3->CONFIG = SPIM_CONFIG_CPHA_Msk | SPIM_CONFIG_CPOL_Msk;
#endif
   NRF_SPIM3->IFTIMING.CSNDUR = 0;
    // Enable and configure easy dma for SPIM03
    NRF_SPIM3->TXD.LIST = SPIM_TXD_LIST_LIST_ArrayList << SPIM_TXD_LIST_LIST_Pos;
    NRF_SPIM3->RXD.LIST = SPIM_RXD_LIST_LIST_Disabled << SPIM_RXD_LIST_LIST_Pos;
    // Enable SPI
    NRF_SPIM3->ENABLE = SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos;
}
void displaySpiWrite(volatile uint8_t byte)
{
    NRF_SPIM3->TXD.PTR = (uint32_t)&byte;
    NRF_SPIM3->RXD.MAXCNT = 0;
    NRF_SPIM3->TXD.MAXCNT = 1;
    NRF_SPIM3->TASKS_START = 1;
    while (NRF_SPIM3->EVENTS_END == 0)
    {
        // wait
    }
    NRF_SPIM3->EVENTS_END = 0;
}
void DisplayWriteCommand(uint8_t value)
{
    DISPLAY_DC_LOW();
    DISPLAY_NCS_LOW();
    nop();
    delay(1);
    displaySpiWrite(value);
    DISPLAY_NCS_HIGH();
}
void DisplayWriteData(uint8_t value)
{
    DISPLAY_DC_HIGH();
    DISPLAY_NCS_LOW();
    delay(1);
    displaySpiWrite(value);
    DISPLAY_NCS_HIGH();
}

void InitDisplayGPIO(void)
{
    // RESET
    const uint32_t outcfg = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                            (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                            (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_DISPLAY_MOSI)->PIN_CNF[PIN_DISPLAY_MOSI] = outcfg;
    GPIO_PORT(PORT_NUM_DISPLAY_SCK)->PIN_CNF[PIN_DISPLAY_SCK] = outcfg;
    GPIO_PORT(PORT_NUM_DISPLAY_DC)->PIN_CNF[PIN_DISPLAY_DC] = outcfg;

#if MINEWDONGLE
    // if we use the dongle, we need also QSPI CLOCK, NCS, D1 and D0
    GPIO_PORT(PORT_NUM_QSPI_CLK)->PIN_CNF[PIN_QSPI_CLK] = outcfg;
    GPIO_PORT(PORT_NUM_QSPI_NCS)->PIN_CNF[PIN_QSPI_NCS] = outcfg;
#endif

// optional pins
#ifdef PIN_DISPLAY_NCS
    GPIO_PORT(PORT_NUM_DISPLAY_NCS)->PIN_CNF[PIN_DISPLAY_NCS] = outcfg;
#endif
#ifdef PIN_DISPLAY_RESET
    GPIO_PORT(PORT_NUM_DISPLAY_RESET)->PIN_CNF[PIN_DISPLAY_RESET] = outcfg;
#else
#ifndef PIN_DISPLAY_NCS
#error The display must at least have NCS or RESET PIN connected!
#endif
#endif
#ifdef PIN_DISPLAY_BACKLIGHT
    GPIO_PORT(PORT_NUM_DISPLAY_BACKLIGHT)->PIN_CNF[PIN_DISPLAY_BACKLIGHT] = outcfg;
#endif
    // Some crappy display modules routed RES and not CS... so if reset is defined, we need to issue a reset for some time
    //
    DISPLAY_NCS_HIGH();
    DISPLAY_DC_HIGH();
#ifdef PIN_DISPLAY_RESET
    DISPLAY_RES_HIGH();
    delay(50);
    DISPLAY_RES_LOW();
    delay(50);
    DISPLAY_RES_HIGH();
    delay(50);
#endif
#ifdef PIN_DISPLAY_BACKLIGHT
    GPIO_PORT(PORT_NUM_DISPLAY_BACKLIGHT)->OUTSET = (1 << PIN_DISPLAY_BACKLIGHT);
#endif
}
static void executeDisplayCommands(const uint8_t * cmds)
{
    // Executes a list of commands for the display.
    /* cmds array has the following structure:
     *  number of commands
     *  Command Code, number of data bytes, data bytes (if any), delay in ms (if number of bytes is in OR with 0x80)
     *  ...
     */

    uint8_t ms, numArgs, numCommands;
    //
    // Send initialization commands
    numCommands = *cmds++; // Number of commands to follow
    while (numCommands--)  // For each command...
    {
        DisplayWriteCommand(*cmds++);       // Read, issue command
        numArgs = *cmds++;                  // Number of args to follow
        ms = numArgs & CMD_ARGS_HAVE_DELAY; // If hibit set, delay follows args
        numArgs &= ~CMD_ARGS_HAVE_DELAY;    // Mask out delay bit
        while (numArgs--)                   // For each argument...
        {
            DisplayWriteData(*cmds++); // Read, issue argument
        }

        if (ms)
        {
            ms = *cmds++; // Read post-command delay time (ms)
            delay((ms == 255 ? 500 : ms));
        }
    }
}
void DisplayInit(void)
{
    // Display initialization.
    // Note: this function DOES NOT initialize the SPI.
    // It just initializes the I/O ports for D/C and nCS and sends the initialization commands to the display
    /* The following array has the following structure:
     *  number of commands
     *  Command Code, number of data bytes, data bytes (if any), delay in ms (if number of bytes is in OR with 0x80)
     *  ...
     */
    const uint8_t generic_st7789[] = {    // Init commands for 7789 screens
        10 + (GAMMA_CORRECTION ? 10 : 0), //  10 commands in list:
        ST77XX_SWRESET,
        ST_CMD_DELAY, //  1: Software reset, no args, w/delay
        150,          //     ~150 ms delay
        ST77XX_SLPOUT,
        ST_CMD_DELAY, //  2: Out of sleep mode, no args, w/delay
        10,           //      10 ms delay
        ST77XX_COLMOD,
        1 + ST_CMD_DELAY, //  3: Set color mode, 1 arg + delay:
        0x55,             //     16-bit color
        10,               //     10 ms delay
        ST77XX_MADCTL,
        1,             //  4: Mem access ctrl (directions), 1 arg:
        MADCTL_CONFIG, //     Row/col addr, bottom-top refresh
        ST77XX_CASET,
        4, //  5: Column addr set, 4 args, no delay:
        0x00,
        0, //     XSTART = 0
        0,
        240, //     XEND = 240
        ST77XX_RASET,
        4, //  6: Row addr set, 4 args, no delay:
        0x00,
        0, //     YSTART = 0
        320 >> 8,
        320 & 0xFF, //     YEND = 320*/
#if GAMMA_CORRECTION
        ST7789_GCTRL,
        1 + ST_CMD_DELAY,
        0x35,
        10, // reset value
        ST7789_VCOMS,
        1 + ST_CMD_DELAY,
        0x20,
        10, 
        ST7789_VDVVRHEN,
        2 + ST_CMD_DELAY,
        0x01,
        0xFF,
        10,
        ST7789_VRHS,
        1 + ST_CMD_DELAY,
        0x0b,
        10, // voltage VRHS
        ST7789_VDVSET,
        1 + ST_CMD_DELAY,
        0x20,
        10, // def value
        ST7789_FRCTR2,
        1 + ST_CMD_DELAY,
        0x0f,
        10,
        ST7789_PWCTRL1,
        2 + ST_CMD_DELAY,
        0xa4,
        0xa1,
        10,
        ST7789_PVGAMCTRL,
        ST_CMD_DELAY + 14,
        GAMMA_SETTINGS,
        10, // 10 ms delay
        ST7789_NVGAMCTRL,
        ST_CMD_DELAY + 14,
        GAMMA_SETTINGS,
        10, //    10 ms delay
#endif
        ST77XX_INVON,
        ST_CMD_DELAY, //  7: hack
        10,
        ST77XX_NORON,
        ST_CMD_DELAY, //  8: Normal display on, no args, w/delay
        10,           //     10 ms delay
        ST77XX_DISPON,
        ST_CMD_DELAY, //  9: Main screen turn on, no args, delay
        10,
        ST77XX_GAMSET,
        1 + ST_CMD_DELAY,
        1,  // gamma
        10, // 10 ms delay
    };

    InitDisplayGPIO();
    executeDisplayCommands(generic_st7789);
}
void BeginUpdateDisplay()
{
    static const uint8_t cmd[] =
    { 
        3,
#if DISPLAY_INVERT_XY
        ST77XX_CASET,
        4,
        0,
        320 - SCREEN_WIDTH,
        (320 - 1) >> 8,
        (319 & 0xFF),
#else
        ST77XX_CASET,
        4,
        0,
        0,
        0,
        SCREEN_WIDTH - 1,
#endif
        ST77XX_RASET,
        4,
        0,
        0,
        0,
        SCREEN_HEIGHT - 1,
        ST77XX_RAMWR,
        0 
    };
    executeDisplayCommands(cmd);
    delay(10);
    DISPLAY_DC_HIGH();
    delay(10);
    DISPLAY_NCS_LOW();
    delay(10);
    nop();
    // second highest
    NVIC_SetPriority(SPIM3_IRQn, 1);
    // for continuouswriting
    NVIC_EnableIRQ(SPIM3_IRQn);
}