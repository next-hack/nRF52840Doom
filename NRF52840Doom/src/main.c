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
 *  Description: main entry point.
 *
 *  Sorry guys, but I like Allman formatting and I HATE HAL bloatware.
 *  Bare-metal programming all the life!
 */
#include "main.h"
#include "d_main.h"
#include "delay.h"
#include "display.h"
#include "doom_iwad.h"
#include "global_data.h"
#include "graphics.h"
#include "i_system.h"
#include "i_video.h"
#include "nrf.h"
#include "printf.h"
#include "qspi.h"
#include "usb.h"
#include "usbCdc.h"
#include "ymodem.h"
#include "z_zone.h"
#include "pwm_audio.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#pragma GCC optimize("Ofast")
//
void uartConfig()
{
    // Configure the UART with no flow control, one parity bit and 115200 baud rate
    NRF_UART0->CONFIG = (UART_CONFIG_HWFC_Disabled << UART_CONFIG_HWFC_Pos) | (UART_CONFIG_PARITY_Excluded << UART_CONFIG_PARITY_Pos);

    NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud115200 << UART_BAUDRATE_BAUDRATE_Pos;

    // Select TX and RX pins
    NRF_UART0->PSELTXD = PIN_TXD;
    NRF_UART0->PSELRXD = PIN_RXD;

    // Enable the UART (starts using the TX/RX pins)
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled << UART_ENABLE_ENABLE_Pos;
    // start tx
    NRF_UART0->TASKS_STARTTX = 1;
    // transmit a return y
    NRF_UART0->TXD = '\r';
}
#define DEBUG_MINEGW 0
void _putchar(char character)
{
#if MINEWDONGLE && DEBUG_MINEGW
    usbCdcPutChar(character);
#else
    while (NRF_UART0->EVENTS_TXDRDY == 0)
    {
    }
    NRF_UART0->EVENTS_TXDRDY = 0;
    NRF_UART0->TXD = character;
#endif
}


void timerInit(void)
{
    // use Timer 3 to get microsecond timer
    NRF_TIMER3->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
    NRF_TIMER3->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER3->PRESCALER = 4 << TIMER_PRESCALER_PRESCALER_Pos; // 1 MHz
    NRF_TIMER3->TASKS_START = 1;
    // use timer 2 to always make Timer 3 capture time
    NRF_TIMER2->SHORTS = TIMER_SHORTS_COMPARE1_CLEAR_Enabled << TIMER_SHORTS_COMPARE1_CLEAR_Pos;
    NRF_TIMER2->CC[0] = 1;
    NRF_TIMER2->CC[1] = 1;
    NRF_TIMER2->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                // mode: timer
    NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_08Bit << TIMER_BITMODE_BITMODE_Pos; // 8 bits are enough.
    NRF_TIMER2->PRESCALER = 0 << TIMER_PRESCALER_PRESCALER_Pos;                     // frequency = 16 MHZ/2^PRESCALER, 16 MHz
    // Start timer, free running
    NRF_TIMER2->TASKS_START = 1;
    // Channel 0: trigger TIMER3 capture on TIMER 2 ch0 compare.
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_TIMER3->TASKS_CAPTURE[0];
    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_TIMER2->EVENTS_COMPARE[0];
    // Enable PPI Channels
    NRF_PPI->CHENSET = (PPI_CHENSET_CH0_Enabled << PPI_CHENSET_CH0_Pos);
}
#define ENABLE_TESTS 0
// random tests.
#if ENABLE_TESTS
extern uint8_t staticZone[];
static inline void startSysTick()
{
    SysTick->VAL = 16777215;
    SysTick->LOAD = 16777215;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
}
static inline uint32_t getSysTickTime() { return 16777215 - SysTick->VAL; }
void tests(void)
{

    // let's get the time required to read from QSPI in various methods
    uint8_t * buffer = staticZone; // use static zone for test
    const uint16_t sizes[] = {1, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    const uint16_t steps[] = {1, 2, 3, 4, 5, 6, 7, 8, 1025};
    uint32_t address;
    uint32_t time;
#if ENABLE_TESTS == 2
    // these are useful with a scope.
    volatile uint8_t b;
    NRF_P0->PIN_CNF[29] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                          (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                          (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    NRF_P0->OUTCLR = 1 << 29;
    // make a first random read
    //        b = *(volatile uint8_t*) 0x12000100;
    delay(2000);
    // used to test port delay
    NRF_P0->OUTSET = 1 << 29;
    NRF_P0->OUTCLR = 1 << 29;
    NRF_P0->OUTSET = 1 << 29;
    NRF_P0->OUTCLR = 1 << 29;
    NRF_P0->OUTSET = 1 << 29;
    NRF_P0->OUTCLR = 1 << 29;
    NRF_P0->OUTSET = 1 << 29;
    NRF_P0->OUTCLR = 1 << 29;
    NRF_P0->OUTSET = 1 << 29;
    NRF_P0->OUTCLR = 1 << 29;
    NRF_P0->OUTSET = 1 << 29;

    b = *(volatile uint8_t *)0x12000000;
    NRF_P0->OUTCLR = 1 << 29;
    delay(1);
    for (int i = 0; i < 2048;)
    {
        NRF_P0->OUTSET = 1 << 29;
        b = *(volatile uint8_t *)(0x12000000 + i);
        i += 1;
        NRF_P0->OUTCLR = 1 << 29;
        b = *(volatile uint8_t *)(0x12000000 + i);
        i += 1;
    }

#endif
    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        address = 0x12000000;
        // method 1: memcpy
        startSysTick();
        memcpy(buffer, (void *)address, sizes[i]);
        time = getSysTickTime();
        // show time
        printf("memcpy %d: \"%d\" clkc\r\n", sizes[i], time);
        // method 2: byte copy  (no address change)
        uint8_t * s = (uint8_t *)address;
        uint8_t * d = buffer;
        startSysTick();
        for (int j = 0; j < sizes[i]; j++)
        {
            *d++ = *s++;
        }
        time = getSysTickTime();
        // show time
        printf("bcpy %d: \"%d\" clkc\r\n", sizes[i], time);
        s += 4096;
        // method 3: byte copy at a different starting address
        // restore dest buffer
        d = buffer;
        startSysTick();
        for (int j = 0; j < sizes[i]; j++)
        {
            *d++ = *s++;
        }
        time = getSysTickTime();
        // show time
        printf("bcpy2 %d: \"%d\" clkc\r\n", sizes[i], time);
        // method 4: word copy
        uint32_t * dw = (uint32_t *)buffer;
        uint32_t * sw = (uint32_t *)address + 8192; // let's point to something else, to prevent caching or prefetch, if any.
        startSysTick();
        for (int j = 0; j < sizes[i] / sizeof(uint32_t); j++)
        {
            *dw++ = *sw++;
        }
        time = getSysTickTime();
        // show time
        printf("wcpy %d: \"%d\" clkc\r\n", sizes[i], time);
        // method 5: DMA read
        startSysTick();
        NRF_QSPI->READ.CNT = sizes[i];
        NRF_QSPI->READ.SRC = address;
        NRF_QSPI->READ.DST = (uint32_t)buffer;
        NRF_QSPI->EVENTS_READY = 0;
        NRF_QSPI->TASKS_READSTART = 1;
        while (NRF_QSPI->EVENTS_READY == 0)
            ;
        NRF_QSPI->EVENTS_READY = 0;
        time = getSysTickTime();
        // show time
        printf("DMA %d: \"%d\" clkc\r\n", sizes[i], time);
        // method 6 and above: read of non contiguous bytes, at various steps
        // change address, to prevent any caching or prefetch buffer, if any.
        address += 4096;
        for (int s = 0; s < sizeof(steps) / sizeof(steps[0]); s++)
        {
            uint32_t step = steps[s];
            d = buffer;
            startSysTick();
            for (int j = 0; j < sizes[i]; j++)
            {
                *d++ = *((uint8_t *)address);
                address = address + step;
            }
            time = getSysTickTime();
            // show time
            printf("bcpy %d, s: %d, \"%d\" clkc\r\n", sizes[i], step, time);
            address += 4096;
        }
        // new adress
        d = buffer;
        s += 4096;
        startSysTick();
        for (int j = 0; j < sizes[i] / 2; j++)
        {
            // suppose a read to a scaled up texture
            *d++ = *s;
            *d++ = *s++;
        }
        time = getSysTickTime();
        // show time
        printf("Scaled up read %d: \"%d\" clkc\r\n", sizes[i], time);
        address += 131072;
    }
    while (1)
        ;
}
#endif

void main(void)
{
    // check if voltage is not set to 3.3V
     if (NRF_UICR->REGOUT0 != UICR_REGOUT0_VOUT_3V3)
    {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
        {
        }
        NRF_UICR->REGOUT0 = UICR_REGOUT0_VOUT_3V3;

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
        {
        }
        NVIC_SystemReset();
    }
    // Start HFCLK from crystal oscillator.
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
        ;
    NRF_NVMC->ICACHECNF = NVMC_ICACHECNF_CACHEEN_Enabled << NVMC_ICACHECNF_CACHEEN_Pos;
    //
    timerInit();
    //
    uartConfig();
    initGraphics();
    initDisplaySpi();
    DisplayInit();
    initKeyboard();
    BeginUpdateDisplay();
    displayPrintln(1, "Doom port to nRF52840");
    displayPrintln(1, "by Nicola Wrachien (next-hack)");
    displayPrintln(1, "");
    // measure refresh time!
    uint32_t oldTime = NRF_TIMER3->CC[0];
    startDisplayRefresh(0);
    oldTime = NRF_TIMER3->CC[0] - oldTime;
    displayPrintln(1, "Frame refresh time %d us!", oldTime);
    displayPrintln(1, "");
#if MINEWDONGLE && DEBUG_MINEGW
    enableUsb();
#endif
#if KEYBOARD == RADIO_KEYBOARD
    displayPrintln(1, "Turn on the radio gamepad!");
#endif
    displayPrintln(1, "Press change weapon+use+alt");
    displayPrintln(1, "within 2s to start WAD upload!");

    // we need to init audio because it is required for the wireless keyboard.
    initPwmAudio();
    //
    qspiInit();
    displayPrintln(1, "Found %d bytes flash!", qspiFlashGetSize());

    delay(2000);
//
#if ENABLE_TESTS
    tests();
#endif
//#define TEST_GAMMA 1
#if TEST_GAMMA
    uint16_t palette[256];
    displayData.pPalette = palette;
    displayData.workingBuffer = 0;
    for (int i = 0; i < 64; i++)
    {
        uint16_t r = i >> 1;
        uint16_t g = i >> 0;
        uint16_t b = i >> 1;
        uint16_t rgb = (r << (6 + 5)) | (g << 5) | (b << (0));
        rgb = (rgb >> 8) | (rgb << 8);
        displayData.pPalette[i] = rgb;
    }
    for (int y = 0; y < 240; y++)
    {
        for (int x = 0; x < 64 * 3; x++)
        {
            uint8_t p = x / 3;

            displayData.displayFrameBuffer[displayData.workingBuffer][y * 240 + x] = /*(p == 44 || p == (64 - 13)) ? 0 :*/ p;
        }
    }
    startDisplayRefresh(displayData.workingBuffer);

    while (1)
        ;
#endif
    //
    uint32_t info = NRF_FICR->INFO.VARIANT;
    uint8_t * p = (uint8_t *)&info;
    printf("Doom port to nRF52840. Detecting MCU data\r\n");
    printf("Nordic Semiconductor nRF%x Variant: %c%c%c%c ", NRF_FICR->INFO.PART, p[3], p[2], p[1], p[0]);
    printf("RAM: %dKB Flash: %dKB\r\n", NRF_FICR->INFO.RAM, NRF_FICR->INFO.FLASH);
    printf("Device ID: %x%x\r\n", NRF_FICR->DEVICEID[0], NRF_FICR->DEVICEID[1]);
    //
    //
    uint8_t c = 0;
    getKeys(&c);

    displayPrintln(1, "Key Pressed: %x", c);
    if ((c & (KEY_ALT | KEY_CHGW | KEY_USE)) == (KEY_ALT | KEY_CHGW | KEY_USE))
    {
        enableUsb();
        displayPrintln(1, "Begin YMODEM Wad Upload");
        if (0 == ymodemReceive(4))
        {
            displayPrintln(1, "Wad Upload successful.");
        }
        else
        {
            displayPrintln(1, "YMODEM Error.");
        }
        displayPrintln(1, "Reset in 1 second!");
        delay(1000);
        NVIC_SystemReset();
    }
    //
    disableWirelessAudio();
    //
    Z_Init(); /* 1/18/98 killough: start up memory stuff first */
    //
    InitGlobals();
    //
    D_DoomMain();
    // D_DoomMain() does not return.
}

/*************************** End of file ****************************/