/**
 *  USB device implementation data for Doom Port to nRF52840.
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
 *  mainly descriptors and device enable function  call.
 */
#ifndef USBDEVICEIMPLEMENTATION_H
#define USB_DEVICEIMPLEMENTATION_H
#include <stdint.h>
#include "usb.h"
#define USE_FUNC_DESC 1
typedef struct
{
    usbConfigurationDescriptor_t configurationDescriptor;
    usbInterfaceDescriptor_t interface0Descriptor;
    #if USE_FUNC_DESC
    uint8_t functionalDescriptorData[19];
    #endif
    usbInterfaceDescriptor_t interface1Descriptor;
    usbEndpointDescriptor_t inEndpointDescriptor;
    usbEndpointDescriptor_t outEndpointDescriptor;
}  __attribute__ ((packed))  cdc_configDescriptor_t;
enum 
{
    USB_FUNCTION_CDC = 0,
    USB_NUM_FUNCTIONS
};

typedef struct
{
    void (*classIrqHandler)(usbRequest_t *); // class specific and endpoint management
    void (*enablefiguration)();                  // to enable endpoints and interrupts for them
} usbClassFunctions_t;


extern const char * usbStrings[];
extern const usbDeviceDescriptor_t usbDeviceDescriptor;
extern const cdc_configDescriptor_t cdc_configDescriptor;
extern const usbStringDescriptorZero_t usbStringDescriptorZero;
extern const usbStringDescriptorZero_t usbStringDescriptorZero;
extern const usbClassFunctions_t usbClassFunctions[USB_NUM_FUNCTIONS];
//
int getUsbNumStrings();
void enableUsbDeviceImplementation();
#endif