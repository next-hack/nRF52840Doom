/**
 *  Display utility functions for Doom Port to nRF52840 MCU.
 *  It also contains the interrupt-assisted DMA update of the display
 *  and double buffer display data structure.
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
 *  Interrupt assisted DMA-based display update engine, with double buffering 
 *  256-color functions to print general purpose texts.
 *  
 */


#include "graphics.h"
#include "display.h"
#include "font8x8.h"
#include "printf.h"
#include <string.h>
//
#define LE2BE16(x) ((uint16_t)(((x >> 8) & 0xFF) | (x << 8)))
#define RGB(r, g, b) (((r >> 3) << (6 + 5)) | ((g >> 2) << 5) | ((g >> 3) << (0)))
//
#define MAX_STRING_SIZE ( SCREEN_WIDTH / FONT_WIDTH + 1)
//
#define XSTR(s) STR(s)
#define STR(s) #s
// note! LDR instructions are pipelined!
#define CONVERT_4_PIX(START_PIX)  "LDRB %[pix0], [%[buff], #0 +" XSTR(START_PIX) "]\n\t" \
                                  "LDRB %[pix1], [%[buff], #1 +" XSTR(START_PIX) "]\n\t" \
                                  "LDRB %[pix2], [%[buff], #2 +" XSTR(START_PIX) "]\n\t" \
                                  "LDRB %[pix3], [%[buff], #3 +" XSTR(START_PIX) "]\n\t" \
                                  "LDRH %[pix0], [%[palette], %[pix0], LSL #1]\n\t" \
                                  "LDRH %[pix1], [%[palette], %[pix1], LSL #1]\n\t" \
                                  "LDRH %[pix2], [%[palette], %[pix2], LSL #1]\n\t" \
                                  "LDRH %[pix3], [%[palette], %[pix3], LSL #1]\n\t" \
                                  "ORR %[pix0], %[pix0], %[pix1], LSL #16\n\t" \
                                  "STR.w %[pix0], [%[dmabuff], #0 + " XSTR(2 * START_PIX) "]\n\t" \
                                  "ORR %[pix2], %[pix2], %[pix3], LSL #16\n\t" \
                                  "STR.w %[pix2], [%[dmabuff], #4 + " XSTR(2 * START_PIX) "]\n\t"
//
static inline void fillNextDmaLineBuffer();
//
const uint16_t palette16[] =    // a gift who those who recognize this palette!
{ 
    LE2BE16(RGB(170, 170, 170)), 
    LE2BE16(RGB(0, 0, 0)), 
    LE2BE16(RGB(255, 255, 255)),  
    LE2BE16(RGB(86, 119, 170)), 
};
//
typedef enum
{
    dm_4bpp = 4, dm_8bpp = 8
} displayMode_t;
//

__attribute__ ((section(".displayData_bss"))) displayData_t displayData;//
void initGraphics()
{
    memset(&displayData, 0, sizeof(displayData));
    displayData.pPalette = (uint16_t * ) palette16;
    displayData.displayMode = dm_8bpp;
    displayData.displayDmaLineBuffersSent = NUMBER_OF_DMA_LINES + 1;
    setDisplayPen(1,0);
}
//
void startDisplayRefresh(uint8_t bufferNumber)
{
#if NEW_DISPLAY_UPDATE_WAY
    displayData.nextBufferIndex = bufferNumber;
    //
    uint8_t early = 0;
    while (displayData.displayDmaLineBuffersSent < NUMBER_OF_DMA_LINES - 1)
    {
      early = 1;
    }
    if (early)
    {
        displayData.doNotDisableShorts = 1;
    }
    else
#endif
    {
 
        // normal mode
        while (displayData.dmaBusy)
        {
        }
        displayData.dmaBusy = 1;
        displayData.displayDmaLineBuffersSent = 0;
        displayData.currentDmaFrameBuffer = displayData.displayFrameBuffer[bufferNumber];
        fillNextDmaLineBuffer();
        // enable shorts
        NRF_SPIM3->EVENTS_STARTED = 0;
        NRF_SPIM3->EVENTS_END = 0;
        #if DISPLAY_SPIM_USES_SHORTS
            NRF_SPIM3->SHORTS = SPIM_SHORTS_END_START_Enabled << SPIM_SHORTS_END_START_Pos; 
            NRF_SPIM3->INTENSET = SPIM_INTENSET_STARTED_Msk;
        #else
            NRF_SPIM3->INTENSET = SPIM_INTENSET_STARTED_Msk | SPIM_INTENSET_END_Msk;
        #endif
        //
        NRF_SPIM3->TASKS_START = 1;
    }
}
//
static uint8_t penColor = 1;
static uint8_t penBackground = 0;
static uint8_t line = 0;
//
void setDisplayMode(displayMode_t dm)
{
    displayData.displayMode = dm;
}
void setPixel(unsigned int x, unsigned int y, int c)
{
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
    {
        return;
    }
    switch (displayData.displayMode)
    {
        case dm_8bpp:
            displayData.displayFrameBuffer[displayData.workingBuffer][x  + SCREEN_WIDTH * y] = c;
            break;
    }
}
void displayPutChar(char c, int x, int y)
{
    for (int cy = 0; cy < FONT_HEIGHT; cy++)
    {
        uint8_t fb = font8x8_basic[0x7F & c][cy];
        for (int cx = 0; cx < FONT_WIDTH; cx++)
        {
            setPixel(x + cx, y + cy, (fb & 1) ? penColor : penBackground);
            fb >>= 1;
        }
    }
}
void setDisplayPen(int color, int background)
{
    penColor = color;
    penBackground = background;
}
void displayVPrintf(int x, int y, const char *format, va_list va)
{
    char outString[MAX_STRING_SIZE];
    vsnprintf(outString, MAX_STRING_SIZE, format, va);
    for (int i = 0;
            i < MAX_STRING_SIZE && x < SCREEN_WIDTH && outString[i] <= 0x7F && outString[i] > 0;
            i++)
    {
        displayPutChar(outString[i], x, y);
        x += FONT_WIDTH;
    }
}
void displayPrintf(int x, int y, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    displayVPrintf(x, y, format, va);
    va_end(va);
}


void displayPrintln(bool update, const char *format, ...)
{
    int y;
    // check if we have space to print.
    if (line < SCREEN_HEIGHT / FONT_HEIGHT)
    {
        y = line * FONT_HEIGHT;
        line++;
    }
    else
    {
        y = SCREEN_HEIGHT - FONT_HEIGHT;
        // printing at the bottom of the screen. move everything up.
        uint32_t *d = (uint32_t*) displayData.displayFrameBuffer[displayData.workingBuffer];
        uint32_t *s = (uint32_t*) (&displayData.displayFrameBuffer[displayData.workingBuffer][SCREEN_WIDTH * FONT_HEIGHT]);
#if 0        
        for (int i = 0; i < SCREEN_WIDTH * (SCREEN_HEIGHT - FONT_HEIGHT) / 4;  i++)
        {
            *d++ = *s++;
        }
#else  
        memcpy(d, s, SCREEN_WIDTH * (SCREEN_HEIGHT - FONT_HEIGHT));
        d += SCREEN_WIDTH * (SCREEN_HEIGHT - FONT_HEIGHT) / 4;
#endif
        // clear last row
        uint32_t color = penBackground | (penBackground << 8) | (penBackground << 16) | (penBackground << 24);
        for (int i = 0; i < SCREEN_WIDTH * FONT_HEIGHT / 4; i++)
        {
            *d++ = color;
        }
    }
    // printf will not wrap around
    va_list va;
    va_start(va, format);
    displayVPrintf(0, y, format, va);
    va_end(va);
    if (update)
    {
        startDisplayRefresh(displayData.workingBuffer);
    }
}
#pragma GCC optimize ("Ofast")  // we need to compile this code to be as fast as possible.
static inline void fillNextDmaLineBuffer()
{
    uint16_t * pLineBuffer = displayData.displayDmaLineBuffer[displayData.currentDisplayDmaLineBuffer];
// displayData.pPalette is volatile.
#if SLOW_WAY // left only for reference
    uint16_t * pPalette = (uint16_t *)displayData.pPalette;

    for (int i = 0; i < PIXELS_PER_DMA_LINE / 16; i++)
    {
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        //
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        //
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        //
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
        *pLineBuffer++ = pPalette[*displayData.currentDmaFrameBuffer++];
    }
#else
    unsigned int pix0, pix1, pix2, pix3, count = 256 / 16;
    __asm volatile
    (
        ".align(4)\n\t"
        "convert8to16bpploop%=:\n\t"
        CONVERT_4_PIX(0)
        CONVERT_4_PIX(4)
        CONVERT_4_PIX(8)
        CONVERT_4_PIX(12)
        "ADD %[dmabuff], #32\n\t"
        "ADD %[buff], #16\n\t"
        "SUBS %[cnt], #1\n\t"
        "BNE convert8to16bpploop%=\n\t"
        :
        [pix0] "+r" (pix0),
        [pix1] "+r" (pix1),
        [pix2] "+r" (pix2),
        [pix3] "+r" (pix3),
        [buff] "+r"  (displayData.currentDmaFrameBuffer),
        [cnt] "+r"  (count),
        [dmabuff] "+r" (pLineBuffer)
        :
        [palette] "r" (displayData.pPalette)
        :
        "cc"
    );
#endif
    displayData.displayDmaLineBuffersSent++;
    NRF_SPIM3->TXD.MAXCNT = PIXELS_PER_DMA_LINE * 2;
    NRF_SPIM3->TXD.PTR = (uint32_t) displayData.displayDmaLineBuffer[displayData.currentDisplayDmaLineBuffer];
    displayData.currentDisplayDmaLineBuffer = 1 - displayData.currentDisplayDmaLineBuffer;
}
void SPIM3_IRQHandler(void)
{
    // fill next line. Enable shorts only if we have still more lines to print
#if DISPLAY_SPIM_USES_SHORTS
    if (NRF_SPIM3->EVENTS_END)
    {
        NRF_SPIM3->EVENTS_END = 0;
        if (displayData.displayDmaLineBuffersSent > NUMBER_OF_DMA_LINES)
        {
            // it was an end
            NRF_SPIM3->EVENTS_STARTED = 0; // clear flags
            // disable end transmission interrupt
            NRF_SPIM3->INTENCLR = SPIM_INTENCLR_END_Msk;
            displayData.dmaBusy = 0;
        }
    }
    if (NRF_SPIM3->EVENTS_STARTED)
    {
        NRF_SPIM3->EVENTS_STARTED = 0; // clear flags
        if (displayData.displayDmaLineBuffersSent >= NUMBER_OF_DMA_LINES)
        {
#if NEW_DISPLAY_UPDATE_WAY
            if (displayData.doNotDisableShorts)
            {
                displayData.doNotDisableShorts = 0;
                //
                displayData.currentDmaFrameBuffer = displayData.displayFrameBuffer[displayData.nextBufferIndex];
                displayData.displayDmaLineBuffersSent = 0;
                fillNextDmaLineBuffer();
            }
            else
#endif
            {
                // disable shorts.
                displayData.displayDmaLineBuffersSent++;
                NRF_SPIM3->SHORTS = 0;
                NRF_SPIM3->INTENSET = SPIM_INTENSET_END_Msk;
            }
        }
        else
        {
            fillNextDmaLineBuffer();
        }
    }
#else

    if (NRF_SPIM3->EVENTS_STARTED)
    {
        NRF_SPIM3->EVENTS_STARTED = 0; // clear flags
        if (displayData.displayDmaLineBuffersSent >= NUMBER_OF_DMA_LINES)
        {
            displayData.displayDmaLineBuffersSent++;
        }
        else
        {
            fillNextDmaLineBuffer();
        }
    }
    if (NRF_SPIM3->EVENTS_END)
    {
        NRF_SPIM3->EVENTS_END = 0;
        if (displayData.displayDmaLineBuffersSent > NUMBER_OF_DMA_LINES)
        {
            displayData.dmaBusy = 0;
        }
        else
        {
            NRF_SPIM3->TASKS_START = 1;
        }
    }
#endif
}