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
#include "usbDeviceImplementation.h"
#include "usb.h"
#include "usbCdc.h"
const usbClassFunctions_t usbClassFunctions[USB_NUM_FUNCTIONS] = 
{
  [USB_FUNCTION_CDC] = {.classIrqHandler = usbCdcClassISR}
};
const char * usbStrings[] =
{
    "next-hack.com",
    "USB DOOM WAD UPLOAD CDC",
    "USB DOOM DEVICE",
    "1",
    "CONF 1",
    "MSC IF"
};
int getUsbNumStrings()
{
  return sizeof(usbStrings)/sizeof(usbStrings[0]);
}
const usbDeviceDescriptor_t usbDeviceDescriptor =
{
    .bLength = sizeof(usbDeviceDescriptor_t),               // Length of this descriptor.
    .bDescriptorType = USB_DESCRIPTOR_DEVICE,       // DEVICE descriptor type (USB_DESCRIPTOR_DEVICE).
    .bcdUSB = 0x0101,				   // USB Spec Release Number (BCD).
    .bDeviceClass = USB_CLASS_COM_AND_CDC_CTRL,          // Class code (assigned by the USB-IF). 0xFF-Vendor specific.
    .bDeviceSubClass = 0,       // Subclass code (assigned by the USB-IF).
    .bDeviceProtocol = 0,       // Protocol code (assigned by the USB-IF). 0xFF-Vendor specific.
    .bMaxPacketSize0 = 64,       // Maximum packet size for endpoint 0.
    .idVendor = 0x16c0,			   // Vendor ID (assigned by the USB-IF).
    .idProduct = 0x05e1,			   // Product ID (assigned by the manufacturer).
    .bcdDevice = 0x0001,			   // Device release number (BCD).
    .iManufacturer = 1,         // Index of String Descriptor describing the manufacturer.
    .iProduct = 2,              // Index of String Descriptor describing the product.
    .iSerialNumber = 3,      // Index of String Descriptor with the device's serial number.
    .bNumConfigurations =1    // Number of possible configurations.
};
const cdc_configDescriptor_t cdc_configDescriptor =
{
    .configurationDescriptor = 
    {
        .bLength = sizeof (usbConfigurationDescriptor_t),  // Length of this descriptor.
        .bDescriptorType = USB_DESCRIPTOR_CONFIGURATION,      // CONFIGURATION descriptor type (USB_DESCRIPTOR_CONFIGURATION).
        .wTotalLength = sizeof(cdc_configDescriptor_t),         // Total length of all descriptors for this configuration.
        .bNumInterfaces = 2,        // Number of interfaces in this configuration.
        .bConfigurationValue = 1,   // Value of this configuration (1 based).
        .iConfiguration = 4,        // Index of String Descriptor describing the configuration.
        .bmAttributes = 0x80,       // Configuration characteristics. USB POWERED
        .bMaxPower = 200,             // Maximum power consumed by this configuration.
    },
    .interface0Descriptor =
    {
        .bLength = sizeof(usbInterfaceDescriptor_t),              // Length of this descriptor.
        .bDescriptorType = USB_DESCRIPTOR_INTERFACE,       // INTERFACE descriptor type (USB_DESCRIPTOR_INTERFACE).
        .bInterfaceNumber = 0,      // Number of this interface (0 based).
        .bAlternateSetting = 0,     // Value of this alternate interface setting.
        .bNumEndpoints = 0,         // Number of endpoints in this interface.
        .bInterfaceClass = USB_CLASS_COM_AND_CDC_CTRL,       // Class code (assigned by the USB-IF).  0xFF-Vendor specific.
        .bInterfaceSubClass = USB_SUBCLASS_ACM,   // Subclass code (assigned by the USB-IF). 6 = SCSI transparent command set
        .bInterfaceProtocol = 0,    // Protocol code (assigned by the USB-IF).  0xFF-Vendor specific.
        .iInterface = 5,            // Index of String Descriptor describing the interface.    
    },
#if USE_FUNC_DESC
    .functionalDescriptorData =
    {
         /* Header Functional Descriptor */
        0x05, // bFunction Length
        0x24, // bDescriptor type: CS_INTERFACE
        0x00, // bDescriptor subtype: Header Func Desc
        0x10, // bcdCDC:1.1
        0x01,

        /* ACM Functional Descriptor */
        0x04, // bFunctionLength
        0x24, // bDescriptor Type: CS_INTERFACE
        0x02, // bDescriptor Subtype: ACM Func Desc
        0x00, // bmCapabilities

        /* Union Functional Descriptor */
        0x05, // bFunctionLength
        0x24, // bDescriptorType: CS_INTERFACE
        0x06, // bDescriptor Subtype: Union Func Desc
        0x00, // bMasterInterface: Communication Class Interface
        0x01, // bSlaveInterface0: Data Class Interface

        /* Call Management Functional Descriptor */
        0x05, // bFunctionLength
        0x24, // bDescriptor Type: CS_INTERFACE
        0x01, // bDescriptor Subtype: Call Management Func Desc
        0x00, // bmCapabilities: D1 + D0
        0x01, // bDataInterface: Data Class Interface 1
    },
#endif
    .interface1Descriptor =
    {
        .bLength = sizeof(usbInterfaceDescriptor_t),              // Length of this descriptor.
        .bDescriptorType = USB_DESCRIPTOR_INTERFACE,       // INTERFACE descriptor type (USB_DESCRIPTOR_INTERFACE).
        .bInterfaceNumber = 1,      // Number of this interface (0 based).
        .bAlternateSetting = 0,     // Value of this alternate interface setting.
        .bNumEndpoints = 2,         // Number of endpoints in this interface.
        .bInterfaceClass = USB_CLASS_CDC_DATA,       // Class code (assigned by the USB-IF).  0xFF-Vendor specific.
        .bInterfaceSubClass = 0,   // Subclass code (assigned by the USB-IF). 6 = SCSI transparent command set
        .bInterfaceProtocol = 0,    // Protocol code (assigned by the USB-IF).  0xFF-Vendor specific.
        .iInterface = 5,            // Index of String Descriptor describing the interface.    
    },
    .inEndpointDescriptor =
    {
    	.bLength = sizeof(usbEndpointDescriptor_t),               // Length of this descriptor.
        .bDescriptorType = USB_DESCRIPTOR_ENDPOINT,       // ENDPOINT descriptor type (USB_DESCRIPTOR_ENDPOINT).
        .bEndpointAddress = 0x81,      // Endpoint address. Bit 7 indicates direction (0=OUT, 1=IN).
        .bmAttributes = USB_TRANSFER_TYPE_BULK,          // Endpoint transfer type.
        .wMaxPacketSize = 0x40       // Maximum packet size.
    },
    .outEndpointDescriptor =
    {
        .bLength = sizeof(usbEndpointDescriptor_t),               // Length of this descriptor.
        .bDescriptorType = USB_DESCRIPTOR_ENDPOINT,       // ENDPOINT descriptor type (USB_DESCRIPTOR_ENDPOINT).
        .bEndpointAddress = 0x01,      // Endpoint address. Bit 7 indicates direction (0=OUT, 1=IN).
        .bmAttributes = USB_TRANSFER_TYPE_BULK,          // Endpoint transfer type.
        .wMaxPacketSize = 0x40       // Maximum packet size.
    },
};
const usbStringDescriptorZero_t usbStringDescriptorZero =
{
    .bLength = 4,
    .bDescriptorType = USB_DESCRIPTOR_STRING,
    .wLANGID[0] = WLANGID_ENGLISH_US,
};
void enableUsbDeviceImplementation()
{
    // here we must call all the init functions for each class
    usbCdcInit();
}
