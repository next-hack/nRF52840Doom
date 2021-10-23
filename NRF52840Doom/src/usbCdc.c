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
#include "printf.h"
#include "usb.h"
#include <string.h>
#include "nrf.h"
#include "nrf52_erratas.h"
#include "main.h"

#define CDC_IN_BUFFER_SIZE 64
#define CDC_OUT_BUFFER_SIZE 64
#define CDC_SET_LINE_CODING_BREQUEST 0x20
#define CDC_GET_LINE_CODING_BREQUEST 0x21
#define CDC_SET_CONTROL_LINE_STATE_BREQUEST 0x22
#define USB_CDC_CLASS_SPECIFIC_IN_BMREQUESTTYPE 0xA1
#define USB_CDC_CLASS_SPECIFIC_OUT_BMREQUESTTYPE 0x21
#warning ZLP not handled for out transfers equal to 64 bytes! (TODO)
#define dbg_printf(...) 
typedef struct
{
    uint32_t dwDTERate;
    uint8_t bCharFormat;
    uint8_t bParityType;
    uint8_t bDataBits;
} __attribute__ ((packed))   cdc_line_coding_t;
typedef struct
{
    volatile uint8_t inBuffer[CDC_IN_BUFFER_SIZE];
    volatile uint8_t outBuffer[CDC_OUT_BUFFER_SIZE];
    volatile cdc_line_coding_t lineCoding;
    volatile uint8_t dataAvailable;                   // if we can read data
    volatile uint8_t dataWaiting;
    volatile uint8_t outBufferRecvd;                  // how much data we received
    volatile uint8_t outBufferPos;                    // where we are in the read buffer
    volatile uint8_t inBufferBusy;
    volatile uint8_t inBufferPos;
} cdcData_t;
static cdcData_t cdcData = 
{
    .lineCoding = 
    {
        .dwDTERate = 115200*8,
        .bCharFormat = 0,
        .bParityType = 0,
        .bDataBits = 8
    },
    .dataAvailable = 0,
    .outBufferPos = 0,
    .inBufferBusy = 0,
    .inBufferPos = 0
};
void usbCdcEnableOut()
{
    if (nrf52_errata_199())
    {
        *(volatile uint32_t *)0x40027C1C = 0x00000082;
    }
    // note: size must be taken before triggering EPOUT1 Start task.
    // we can safely set it now, because dataAvailable is not 1 yet.
    cdcData.outBufferRecvd = NRF_USBD->SIZE.EPOUT[1]; 
    NRF_USBD->EPOUT[1].MAXCNT = CDC_OUT_BUFFER_SIZE;
    NRF_USBD->EPOUT[1].PTR = (uint32_t) cdcData.outBuffer;
    usbdTaskTrigger(&NRF_USBD->TASKS_STARTEPOUT[1]);
    // Touching USBD registers in this buggy MCU is critical during DMA...
    if (nrf52_errata_199())
    {
        while (!NRF_USBD->EVENTS_ENDEPOUT[1] && !NRF_USBD->EVENTS_USBRESET);
    }

}
static void usbCdcSendBuffer(void)
{
    if (nrf52_errata_199())
    {
        *(volatile uint32_t *)0x40027C1C = 0x00000082;
    }
    // mask buffer as busy
    cdcData.inBufferBusy = 1;
    // Set DMA
    NRF_USBD->EPIN[1].MAXCNT = cdcData.inBufferPos;
    NRF_USBD->EPIN[1].PTR = (uint32_t) cdcData.inBuffer;
    // trigger task
    usbdTaskTrigger(&NRF_USBD->TASKS_STARTEPIN[1]);
    cdcData.inBufferPos = 0;
    // Touching USBD registers in this buggy MCU is critical during DMA...
    if (nrf52_errata_199())
    {
        while (!NRF_USBD->EVENTS_ENDEPIN[1] && !NRF_USBD->EVENTS_USBRESET);
    }
}
void usbCdcClassISR(usbRequest_t * request)
{
    if (request->bValid)
    {
        // class specific request
        if (USB_CDC_CLASS_SPECIFIC_IN_BMREQUESTTYPE == request->bmRequestType && CDC_GET_LINE_CODING_BREQUEST == request->bRequest)
        {
            dbg_printf("Get line codng\r\n");
            COPY_DESCRIPTOR_TO_RAM_AND_SEND(cdcData.lineCoding);
        }
        // class specific request
        if (USB_CDC_CLASS_SPECIFIC_OUT_BMREQUESTTYPE == request->bmRequestType)
        {
            switch (request->bRequest)
            {
                case CDC_SET_LINE_CODING_BREQUEST:
                {
                    volatile uint32_t tmpBuffer[2];
                    NRF_USBD->EPOUT[0].MAXCNT = 7;
                    NRF_USBD->EPOUT[0].PTR = (uint32_t)&cdcData.lineCoding;
                    usbdTaskTrigger(&NRF_USBD->TASKS_EP0RCVOUT);
                    // wait for ep0 data done
                    usbdWaitForEventAndClear(&NRF_USBD->EVENTS_EP0DATADONE, USBD_EVENT_MAX_DELAY_MS);
                    usbdTaskTrigger(&NRF_USBD->TASKS_STARTEPOUT[0]);
                    usbdWaitForEventAndClear(&NRF_USBD->EVENTS_ENDEPOUT[0], USBD_EVENT_MAX_DELAY_MS);
                    for (int i = 0; i < 7; i++)
                    {
                        dbg_printf("%02x ", ((uint8_t *)&cdcData.lineCoding)[i]);
                    }
                    usbdTaskTrigger(&NRF_USBD->TASKS_EP0STATUS);
                    dbg_printf("\r\nSet Line Coding\r\n");
                    break;
                }
                case CDC_SET_CONTROL_LINE_STATE_BREQUEST:
                    usbdTaskTrigger(&NRF_USBD->TASKS_EP0STATUS);
                    dbg_printf("Set control line state\r\n");
                    break;
            }
        }
    }
    // did the DMA finish filling the receive buffer ?
    if (NRF_USBD->EVENTS_ENDEPOUT[1])
    {
        if (cdcData.outBufferRecvd)
        {
            cdcData.dataAvailable = 1;
        }
        cdcData.outBufferPos = 0;
        if (nrf52_errata_199())
        {
            *(volatile uint32_t *)0x40027C1C = 0;
        }
        NRF_USBD->EVENTS_ENDEPOUT[1] = 0;
    }
    if (NRF_USBD->EVENTS_ENDEPIN[1])
    {
        if (nrf52_errata_199())
        {
            *(volatile uint32_t *)0x40027C1C = 0;
        }
        NRF_USBD->EVENTS_ENDEPIN[1] = 0;
        /*if (NRF_USBD->EPIN[1].AMOUNT)
        {
          NRF_USBD->EPIN[1].MAXCNT = cdcData.inBufferPos;
          NRF_USBD->EPIN[1].PTR = (uint32_t) cdcData.inBuffer;
        }*/
    }
    if (request->epDataEvent)
    {
        if ((NRF_USBD->EPDATASTATUS & USBD_EPDATASTATUS_EPOUT1_Msk))
        {
            // is our buffer clear? let's fill it
            if (0 == cdcData.dataAvailable && !cdcData.dataWaiting)
            {
                usbCdcEnableOut();
            }
            else
            {
                // buffer is busy. Transfer will be triggered by getChar
                cdcData.dataWaiting = 1;
            }
            NRF_USBD->EPDATASTATUS = USBD_EPDATASTATUS_EPOUT1_Msk;
        }
        if ((NRF_USBD->EPDATASTATUS & USBD_EPDATASTATUS_EPIN1_Msk))
        {
            NRF_USBD->EPDATASTATUS = USBD_EPDATASTATUS_EPIN1_Msk;
            // is our buffer clear? let's fill it
            cdcData.inBufferBusy = 0;
        }
    }
}
int usbCdcInit()
{
    // set interrupt for EPDATA and specific endpoints
    NRF_USBD->INTENSET = USBD_INTENSET_EPDATA_Msk | USBD_INTENSET_ENDEPOUT1_Msk | USBD_INTENSET_ENDEPIN1_Msk;
    usbCdcEnableOut();
    // enable endpoints
    NRF_USBD->EPINEN |= USBD_EPINEN_IN1_Msk;
    NRF_USBD->EPOUTEN |= USBD_EPOUTEN_OUT1_Msk;
    //
    NRF_USBD->SIZE.EPOUT[1] = 0;

    // enable timeut timer. This allows to send data if there was a too long delay
#if USB_USES_TIMER4
    NRF_TIMER4->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
    NRF_TIMER4->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER4->PRESCALER = 4 << TIMER_PRESCALER_PRESCALER_Pos; // 1 MHz
    NRF_TIMER4->CC[0] = 1000;                                   // 1 ms char 2 char
    NRF_TIMER4->SHORTS =
        (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos) | (TIMER_SHORTS_COMPARE0_STOP_Enabled << TIMER_SHORTS_COMPARE0_STOP_Pos);
    NRF_TIMER4->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
    NVIC_SetPriority(TIMER4_IRQn, 2);
    NVIC_EnableIRQ(TIMER4_IRQn);
#else
    // we use timer 3 compare
    // clear any compare etc.
    NRF_TIMER3->INTENCLR = TIMER_INTENCLR_COMPARE1_Clear << TIMER_INTENCLR_COMPARE1_Pos;
    NRF_TIMER3->EVENTS_COMPARE[1] = 0;
    NVIC_ClearPendingIRQ(TIMER3_IRQn);
    NVIC_SetPriority(TIMER3_IRQn, 2);
    NVIC_EnableIRQ(TIMER3_IRQn);
#endif
}
int usbCdcGetChar()
{
    int c = -1;
    disableUsbIrq();
    if (cdcData.dataAvailable)
    {
        c = cdcData.outBuffer[cdcData.outBufferPos] & 0xFF;
        cdcData.outBufferPos++;
    }
    // did we consume all the buffer? signal the USB device that the
    // driver can use the buffer again!
    if (cdcData.outBufferPos >= cdcData.outBufferRecvd)
    {
        cdcData.dataAvailable = 0;
        if (cdcData.dataWaiting)
        {
            cdcData.dataWaiting = 0;
            usbCdcEnableOut();
        }
    }
    enableUsbIrq();
    return c;
}
void usbCdcFlush()
{
    disableUsbIrq();
    cdcData.dataAvailable = 0;
    if (cdcData.dataWaiting)
    {
        cdcData.dataWaiting = 0;
        usbCdcEnableOut();
        // remove data from endpoint
        usbdWaitForEventAndClear(&NRF_USBD->EVENTS_ENDEPOUT[1], USBD_EVENT_MAX_DELAY_MS);
        cdcData.outBufferPos = 0;
    }
    enableUsbIrq();
}

int usbCdcPutChar(char c)
{
    if (usbGetState() != USB_STATE_CONFIGURED)
    { // this prevents blocking when the device is not connected to a PC port
        return -1;
    }
    // this is blocking.
    while (cdcData.inBufferBusy)
    {
    }
    disableUsbIrq();
    // buffer is available. We can write to it
    cdcData.inBuffer[cdcData.inBufferPos] = c;
    cdcData.inBufferPos++;
    // in the meantime the timer could have been expired. Let's clear it and start again
#if USB_USES_TIMER4
    NRF_TIMER4->TASKS_STOP = 1;
    NRF_TIMER4->TASKS_CLEAR = 1;
    // clear any interrutps
    NRF_TIMER4->EVENTS_COMPARE[0] = 0;
    NVIC_ClearPendingIRQ(TIMER4_IRQn);
#else
    // disable IRQ generation.
    // set next compare period on the next 1 ms
    // set comapre event (if expired) to 0
    // clear any pending interrupt
    NRF_TIMER3->INTENCLR = TIMER_INTENCLR_COMPARE1_Clear << TIMER_INTENCLR_COMPARE1_Pos;
    NRF_TIMER3->CC[1] = NRF_TIMER3->CC[0] + 1000; // have a compare 1000 microseconds from now.
    NRF_TIMER3->EVENTS_COMPARE[1] = 0;
    NVIC_ClearPendingIRQ(TIMER3_IRQn);
#endif
    // now, is the buffer full ? In this case send straight away
    if (cdcData.inBufferPos >= CDC_IN_BUFFER_SIZE)
    {
        usbCdcSendBuffer();
    }
    else
    {
        // start timer so that if within 1 ms no more data is sent, we will send it straight away.
#if USB_USES_TIMER4
        NRF_TIMER4->TASKS_START = 1;
#else
        // enable interrupt generation
        NRF_TIMER3->INTENSET = TIMER_INTENSET_COMPARE1_Set << TIMER_INTENSET_COMPARE1_Pos;
#endif
    }
    enableUsbIrq();
    return c & 0xFF;
}
#if USB_USES_TIMER4
void TIMER4_IRQHandler(void)
{
    // timer already cleared and stopped by shorts
    NRF_TIMER4->EVENTS_COMPARE[0] = 0;
    usbCdcSendBuffer();
}
#else
void TIMER3_IRQHandler(void)
{
    NRF_TIMER3->EVENTS_COMPARE[1] = 0;
    NRF_TIMER3->INTENCLR = TIMER_INTENCLR_COMPARE1_Clear << TIMER_INTENCLR_COMPARE1_Pos;
    NVIC_ClearPendingIRQ(TIMER3_IRQn); // this is because we might have triggered a new compare right after clearing the compare event, if we have disabled the
                                       // ISRs and enabled them again.
    usbCdcSendBuffer();
}
#endif