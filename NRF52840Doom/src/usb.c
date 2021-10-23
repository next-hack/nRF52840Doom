/**
 *  USB Device driver for Doom Port to nRF52840.
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
 *  Simple and lightweight USB device driver. It supports multiple
 *  class-devices, if, in the future, one wants to implement MSC too. 
 *
 */
#include "usb.h"
#include "nrf.h"
#include "nrf52_erratas.h"
#include "printf.h"
#include "usbDeviceImplementation.h"
#include <string.h>
#define dbg_printf(...)
static uint8_t usbState = USB_STATE_RESET;
//
/**
 * @brief Begin errata 171.
 */
static inline void usbd_errata_171_begin(void)
{
    if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000)
    {
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
        *((volatile uint32_t *)(0x4006EC14)) = 0x000000C0;
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
    }
    else
    {
        *((volatile uint32_t *)(0x4006EC14)) = 0x000000C0;
    }
}

/**
 * @brief End errata 171.
 */
static inline void usbd_errata_171_end(void)
{
    if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000)
    {
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
        *((volatile uint32_t *)(0x4006EC14)) = 0x00000000;
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
    }
    else
    {
        *((volatile uint32_t *)(0x4006EC14)) = 0x00000000;
    }
}

/**
 * @brief Begin erratas 187 and 211.
 */
static inline void usbd_errata_187_211_begin(void)
{
    if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000)
    {
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
        *((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
    }
    else
    {
        *((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
    }
}

/**
 * @brief End erratas 187 and 211.
 */
static inline void usbd_errata_187_211_end(void)
{
    if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000)
    {
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
        *((volatile uint32_t *)(0x4006ED14)) = 0x00000000;
        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
    }
    else
    {
        *((volatile uint32_t *)(0x4006ED14)) = 0x00000000;
    }
}
//
bool enableUsb(void)
{
    static uint8_t enabled = 0;
    if (enabled)
        return true;
    enabled = 1;
    // wait for USB detection
    while (0 == NRF_POWER->EVENTS_USBDETECTED)
    {
    }

    if (nrf52_errata_187())
    {
        usbd_errata_187_211_begin();
    }

    if (nrf52_errata_171())
    {
        usbd_errata_171_begin();
    }

    /* Enable the peripheral */
    NRF_USBD->ENABLE = USBD_ENABLE_ENABLE_Enabled << USBD_ENABLE_ENABLE_Pos;
    __ISB();
    __DSB();

    // wait until ready
    usbdEventcauseWaitAndClear(USBD_EVENTCAUSE_READY_Msk);


    if (nrf52_errata_171())
    {
        usbd_errata_171_end();
    }

    if (nrf52_errata_187())
    {
        usbd_errata_187_211_end();
    }

    // wait for USB power ready event
    while (0 == NRF_POWER->EVENTS_USBPWRRDY)
    {
    }
    // wait for ready
    NRF_USBD->USBPULLUP = USBD_USBPULLUP_CONNECT_Enabled << USBD_USBPULLUP_CONNECT_Pos;
    // Enable ISR
    NRF_USBD->INTENSET = USBD_INTENSET_EP0SETUP_Msk | USBD_INTENSET_USBRESET_Msk;
    NVIC_SetPriority(USBD_IRQn,2);
    NVIC_EnableIRQ(USBD_IRQn);
    return true;
}
//
inline uint8_t usbGetState() { return usbState; }
//

uint32_t usbSendDescriptor(volatile void * descriptor, uint32_t descriptorSize)
{
    NRF_USBD->EPIN[0].PTR = (uint32_t)descriptor;
    NRF_USBD->EPIN[0].MAXCNT = descriptorSize;
    if (nrf52_errata_199())
    {
        *(volatile uint32_t *)0x40027C1C = 0x00000082;
    }
    dbg_printf("ds = %d. Data:\r\n", descriptorSize);
    for (int i = 0; i < descriptorSize; i++)
    {
        dbg_printf("%02x ", ((uint8_t *)descriptor)[i]);
    }
    dbg_printf("\r\n");
    usbdTaskTrigger(&NRF_USBD->TASKS_STARTEPIN[0]);
    usbdWaitForEventAndClear(&NRF_USBD->EVENTS_STARTED, USBD_EVENT_MAX_DELAY_MS);
    if (NRF_USBD->EPSTATUS & USBD_EPSTATUS_EPIN0_Msk)
        NRF_USBD->EPSTATUS = USBD_EPSTATUS_EPIN0_Msk;
    {
        usbdWaitForEventAndClear(&NRF_USBD->EVENTS_ENDEPIN[0], USBD_EVENT_MAX_DELAY_MS);
        usbdWaitForEventAndClear(&NRF_USBD->EVENTS_EP0DATADONE, USBD_EVENT_MAX_DELAY_MS);
        if (nrf52_errata_199())
        {
            *(volatile uint32_t *)0x40027C1C = 0x00000000;
        }
        dbg_printf("Catpured\r\n");
        usbdTaskTrigger(&NRF_USBD->TASKS_EP0STATUS);
    }
}
// TODO: enumeration to be handled in ISR AS WELL!
static inline void usbHandleCommonRequests(usbRequest_t * request)
{
    if (request->bValid)
    {
        // we got a setup
        if ((request->bmRequestType & USBD_BMREQUESTTYPE_DIRECTION_Msk) ==
            (USBD_BMREQUESTTYPE_DIRECTION_DeviceToHost << USBD_BMREQUESTTYPE_DIRECTION_Pos))
        {
            // we were asked for something
            switch (request->bmRequestType & USBD_BMREQUESTTYPE_TYPE_Msk)
            {
                case (USBD_BMREQUESTTYPE_TYPE_Standard << USBD_BMREQUESTTYPE_TYPE_Pos):
                    switch (request->bmRequestType & USBD_BMREQUESTTYPE_RECIPIENT_Msk)
                    {
                        case (USBD_BMREQUESTTYPE_RECIPIENT_Device
                              << USBD_BMREQUESTTYPE_RECIPIENT_Pos):
                            switch (request->bRequest)
                            {
                                case USBD_BREQUEST_BREQUEST_STD_GET_DESCRIPTOR:
                                    switch (request->wValue >> 8)
                                    {
                                        case USB_DESCRIPTOR_DEVICE:
                                        {
                                            dbg_printf(" get device descriptor\r\n");
                                            COPY_DESCRIPTOR_TO_RAM_AND_SEND(usbDeviceDescriptor);
                                            break;
                                        }
                                        case USB_DESCRIPTOR_CONFIGURATION:
                                            if (sizeof(cdc_configDescriptor) > request->wLength)
                                            {
                                                COPY_DESCRIPTOR_TO_RAM_AND_SEND_SIZE_SPECIFY(
                                                    cdc_configDescriptor, request->wLength);
                                            }
                                            else
                                            {
                                                COPY_DESCRIPTOR_TO_RAM_AND_SEND(
                                                    cdc_configDescriptor);
                                            }
                                            dbg_printf(" Get configuration descriptor\r\n");
                                            break;
                                        case USB_DESCRIPTOR_STRING:
                                        {
                                            uint32_t index = request->wValue & 0xFF;
                                            dbg_printf("Get string descriptor %d\r\n", index);
                                            if (index == 0)
                                            {
                                                uint32_t sz =
                                                    request->wLength >
                                                            sizeof(usbStringDescriptorZero)
                                                        ? sizeof(usbStringDescriptorZero)
                                                        : request->wLength;
                                                COPY_DESCRIPTOR_TO_RAM_AND_SEND_SIZE_SPECIFY(
                                                    usbStringDescriptorZero, sz);
                                            }
                                            else
                                            {
                                                index--;
                                                uint8_t
                                                    tmpString[2 * MAX_USB_DESCRIPTOR_STRING_SIZE] =
                                                        {0}; // this is a memset
                                                if (index >= getUsbNumStrings())
                                                {
                                                    index = getUsbNumStrings() - 1;
                                                }
                                                const char * stringPtr = usbStrings[index];
                                                int i = 2;
                                                tmpString[1] = USB_DESCRIPTOR_STRING;
                                                for (const char * p = stringPtr;
                                                     *p != 0 &&
                                                     i < 2 * MAX_USB_DESCRIPTOR_STRING_SIZE;
                                                     p++, i += 2)
                                                {
                                                    dbg_printf("p %d, %c\r\n", *p, *p);
                                                    tmpString[i] = *p;
                                                }
                                                tmpString[0] = i; // set size
                                                usbSendDescriptor(tmpString, i);
                                            }
                                        }
                                        break;
                                    }
                                    break;
                            }
                            break;
                    }
                    break;
            }
        }
        else
        {
            // we will get some data
            switch (request->bmRequestType & USBD_BMREQUESTTYPE_TYPE_Msk)
            {
                case (USBD_BMREQUESTTYPE_TYPE_Standard << USBD_BMREQUESTTYPE_TYPE_Pos):
                    //  standard request;
                    switch (request->bmRequestType & USBD_BMREQUESTTYPE_RECIPIENT_Msk)
                    {
                        case (USBD_BMREQUESTTYPE_RECIPIENT_Device
                              << USBD_BMREQUESTTYPE_RECIPIENT_Pos):
                            // device request
                            switch (request->bRequest)
                            {
                                case USBD_BREQUEST_BREQUEST_STD_SET_ADDRESS:
                                    dbg_printf("Got address %d\r\n", NRF_USBD->USBADDR);
                                    break;

                                case USBD_BREQUEST_BREQUEST_STD_SET_CONFIGURATION:
                                    enableUsbDeviceImplementation();
                                    dbg_printf("Set configuration %d\r\n", request->wValue);
                                    // no data phase
                                    usbdTaskTrigger(&NRF_USBD->TASKS_EP0STATUS);
                                    usbState = USB_STATE_CONFIGURED;
                                    break;
                            }
                            break;
                        case (USBD_BMREQUESTTYPE_RECIPIENT_Endpoint
                              << USBD_BMREQUESTTYPE_RECIPIENT_Pos):
                            if (request->bmRequestType == USBD_BREQUEST_BREQUEST_STD_CLEAR_FEATURE)
                            {
// TODO: unstall not implemented
                                usbdTaskTrigger(&NRF_USBD->TASKS_EP0STATUS);
                            }
                            break;
                    }
                    break;
            }
        }
    }
}
void USBD_IRQHandler(void)
{
    usbRequest_t request;
    request.bValid = 0;
    // did we get a new setup request ? let's fill the request data!
    if (NRF_USBD->EVENTS_USBRESET)
    {
        // TODO
        NRF_USBD->EVENTS_USBRESET = 0;
    }
    if (NRF_USBD->EVENTS_EP0SETUP)
    {
        request.bRequest = NRF_USBD->BREQUEST;
        request.bmRequestType = NRF_USBD->BMREQUESTTYPE;
        request.wIndex = (NRF_USBD->WINDEXH << 8) | NRF_USBD->WINDEXL;
        request.wLength = (NRF_USBD->WLENGTHH << 8) | NRF_USBD->WLENGTHL;
        request.wValue = (NRF_USBD->WVALUEH << 8) | NRF_USBD->WVALUEL;
        request.bValid = 1; // signal that we have a setup request.
        NRF_USBD->EVENTS_EP0SETUP = 0;
        dbg_printf("Got Setup:\r\n");
        dbg_printf("bmRequestType = 0x%02x\r\n", request.bmRequestType);
        dbg_printf("bRequest = 0x%02x\r\n", request.bRequest);
        dbg_printf("wValue = 0x%04x\r\n", request.wValue);
        dbg_printf("wIndex = 0x%04x\r\n", request.wIndex);
        dbg_printf("wLength = 0x%04x\r\n", request.wLength);
    }
    uint8_t eventsEpdata = NRF_USBD->EVENTS_EPDATA;
    request.epDataEvent = eventsEpdata;
    usbHandleCommonRequests(&request); // we handle basically enumeration here
    // EPDATA can change anytime even in the ISR. 
    for (int i = 0; i < USB_NUM_FUNCTIONS; i++)
    {
        usbClassFunctions[i].classIrqHandler(&request);
    }
    if (eventsEpdata)
    {
        if (NRF_USBD->EPDATASTATUS == 0)
        {
            NRF_USBD->EVENTS_EPDATA = 0;
        }
        else
        {
            dbg_printf("Some unhandled events. Will be dealt later\r\n");
        }
    }
}