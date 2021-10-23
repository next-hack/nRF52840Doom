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
#ifndef USB_H
#define USB_H
#include "nrf.h"
#include <stdbool.h>
#include <stdint.h>
// some USB defines
#define WLANGID_ENGLISH_US 0x0409

/* Misc.USB constants */
#define DEV_DESCR_LEN 18 // device descriptor length
#define CONF_DESCR_LEN 9 // configuration descriptor length
#define INTR_DESCR_LEN 9 // interface descriptor length
#define EP_DESCR_LEN 7   // endpoint descriptor length

/* Standard Device Requests constants */

#define USB_REQUEST_GET_STATUS 0        // Standard Device Request - GET STATUS
#define USB_REQUEST_CLEAR_FEATURE 1     // Standard Device Request - CLEAR FEATURE
#define USB_REQUEST_SET_FEATURE 3       // Standard Device Request - SET FEATURE
#define USB_REQUEST_SET_ADDRESS 5       // Standard Device Request - SET ADDRESS
#define USB_REQUEST_GET_DESCRIPTOR 6    // Standard Device Request - GET DESCRIPTOR
#define USB_REQUEST_SET_DESCRIPTOR 7    // Standard Device Request - SET DESCRIPTOR
#define USB_REQUEST_GET_CONFIGURATION 8 // Standard Device Request - GET CONFIGURATION
#define USB_REQUEST_SET_CONFIGURATION 9 // Standard Device Request - SET CONFIGURATION
#define USB_REQUEST_GET_INTERFACE 10    // Standard Device Request - GET INTERFACE
#define USB_REQUEST_SET_INTERFACE 11    // Standard Device Request - SET INTERFACE
#define USB_REQUEST_SYNCH_FRAME 12      // Standard Device Request - SYNCH FRAME

#define USB_FEATURE_ENDPOINT_HALT 0        // CLEAR/SET FEATURE - Endpoint Halt
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP 1 // CLEAR/SET FEATURE - Device remote wake-up
#define USB_FEATURE_TEST_MODE 2            // CLEAR/SET FEATURE - Test mode

/* Setup Data Constants */

#define USB_SETUP_HOST_TO_DEVICE                                                                   \
    0x00 // Device Request bmRequestType transfer direction - host to device transfer
#define USB_SETUP_DEVICE_TO_HOST                                                                   \
    0x80 // Device Request bmRequestType transfer direction - device to host transfer
#define USB_SETUP_TYPE_STANDARD 0x00       // Device Request bmRequestType type - standard
#define USB_SETUP_TYPE_CLASS 0x20          // Device Request bmRequestType type - class
#define USB_SETUP_TYPE_VENDOR 0x40         // Device Request bmRequestType type - vendor
#define USB_SETUP_RECIPIENT_DEVICE 0x00    // Device Request bmRequestType recipient - device
#define USB_SETUP_RECIPIENT_INTERFACE 0x01 // Device Request bmRequestType recipient - interface
#define USB_SETUP_RECIPIENT_ENDPOINT 0x02  // Device Request bmRequestType recipient - endpoint
#define USB_SETUP_RECIPIENT_OTHER 0x03     // Device Request bmRequestType recipient - other

/* USB descriptors  */

#define USB_DESCRIPTOR_DEVICE 0x01           // bDescriptorType for a Device Descriptor.
#define USB_DESCRIPTOR_CONFIGURATION 0x02    // bDescriptorType for a Configuration Descriptor.
#define USB_DESCRIPTOR_STRING 0x03           // bDescriptorType for a String Descriptor.
#define USB_DESCRIPTOR_INTERFACE 0x04        // bDescriptorType for an Interface Descriptor.
#define USB_DESCRIPTOR_ENDPOINT 0x05         // bDescriptorType for an Endpoint Descriptor.
#define USB_DESCRIPTOR_DEVICE_QUALIFIER 0x06 // bDescriptorType for a Device Qualifier.
#define USB_DESCRIPTOR_OTHER_SPEED 0x07      // bDescriptorType for a Other Speed Configuration.
#define USB_DESCRIPTOR_INTERFACE_POWER 0x08  // bDescriptorType for Interface Power.
#define USB_DESCRIPTOR_OTG 0x09              // bDescriptorType for an OTG Descriptor.

#define USB_DESCRIPTOR_HID 0x21


/* USB Endpoint Transfer Types  */
#define USB_TRANSFER_TYPE_CONTROL 0x00     // Endpoint is a control endpoint.
#define USB_TRANSFER_TYPE_ISOCHRONOUS 0x01 // Endpoint is an isochronous endpoint.
#define USB_TRANSFER_TYPE_BULK 0x02        // Endpoint is a bulk endpoint.
#define USB_TRANSFER_TYPE_INTERRUPT 0x03   // Endpoint is an interrupt endpoint.


/* Standard Feature Selectors for CLEAR_FEATURE Requests    */
#define USB_FEATURE_ENDPOINT_STALL 0       // Endpoint recipient
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP 1 // Device recipient
#define USB_FEATURE_TEST_MODE 2            // Device recipient

// USB Device Classes
#define USB_CLASS_USE_CLASS_INFO 0x00    // Use Class Info in the Interface Descriptors
#define USB_CLASS_AUDIO 0x01             // Audio
#define USB_CLASS_COM_AND_CDC_CTRL 0x02  // Communications and CDC Control
#define USB_CLASS_HID 0x03               // HID
#define USB_CLASS_PHYSICAL 0x05          // Physical
#define USB_CLASS_IMAGE 0x06             // Image
#define USB_CLASS_PRINTER 0x07           // Printer
#define USB_CLASS_MASS_STORAGE 0x08      // Mass Storage
#define USB_CLASS_HUB 0x09               // Hub
#define USB_CLASS_CDC_DATA 0x0a          // CDC-Data
#define USB_CLASS_SMART_CARD 0x0b        // Smart-Card
#define USB_CLASS_CONTENT_SECURITY 0x0d  // Content Security
#define USB_CLASS_VIDEO 0x0e             // Video
#define USB_CLASS_PERSONAL_HEALTH 0x0f   // Personal Healthcare
#define USB_CLASS_DIAGNOSTIC_DEVICE 0xdc // Diagnostic Device
#define USB_CLASS_WIRELESS_CTRL 0xe0     // Wireless Controller
#define USB_CLASS_MISC 0xef              // Miscellaneous
#define USB_CLASS_APP_SPECIFIC 0xfe      // Application Specific
#define USB_CLASS_VENDOR_SPECIFIC 0xff   // Vendor Specific
//
#define USB_SUBCLASS_ACM 2
//
#define USBD_EVENT_MAX_DELAY_MS 10000
#define MAX_USB_DESCRIPTOR_STRING_SIZE 32
// Usb states. There are many more, but we are interested only if the device has been enumerated.
enum
{
    USB_STATE_RESET = 0,
    USB_STATE_CONFIGURED = 1
};
//

uint32_t usbSendDescriptor(volatile void * descriptor, uint32_t descriptorSize);
#define COPY_DESCRIPTOR_TO_RAM_AND_SEND(d)                                                         \
    do                                                                                             \
    {                                                                                              \
        uint8_t ramDescriptor[sizeof(d)];                                                          \
        memcpy(ramDescriptor, (void *)&d, sizeof(d));                                              \
        usbSendDescriptor(&ramDescriptor, sizeof(d));                                              \
    } while (0)
#define COPY_DESCRIPTOR_TO_RAM_AND_SEND_SIZE_SPECIFY(d, sz)                                        \
    do                                                                                             \
    {                                                                                              \
        uint8_t ramDescriptor[sizeof(d)];                                                          \
        memcpy(ramDescriptor, &d, sizeof(d));                                                      \
        usbSendDescriptor(&ramDescriptor, sz);                                                     \
    } while (0)

// descriptors

/* Device descriptor structure */
typedef struct
{
    uint8_t bLength;            // Length of this descriptor.
    uint8_t bDescriptorType;    // DEVICE descriptor type (USB_DESCRIPTOR_DEVICE).
    uint16_t bcdUSB;            // USB Spec Release Number (BCD).
    uint8_t bDeviceClass;       // Class code (assigned by the USB-IF). 0xFF-Vendor specific.
    uint8_t bDeviceSubClass;    // Subclass code (assigned by the USB-IF).
    uint8_t bDeviceProtocol;    // Protocol code (assigned by the USB-IF). 0xFF-Vendor specific.
    uint8_t bMaxPacketSize0;    // Maximum packet size for endpoint 0.
    uint16_t idVendor;          // Vendor ID (assigned by the USB-IF).
    uint16_t idProduct;         // Product ID (assigned by the manufacturer).
    uint16_t bcdDevice;         // Device release number (BCD).
    uint8_t iManufacturer;      // Index of String Descriptor describing the manufacturer.
    uint8_t iProduct;           // Index of String Descriptor describing the product.
    uint8_t iSerialNumber;      // Index of String Descriptor with the device's serial number.
    uint8_t bNumConfigurations; // Number of possible configurations.
} __attribute__((packed)) usbDeviceDescriptor_t;
typedef struct
{
    uint8_t bLength; //
    uint8_t bDescriptorType;
} usbDescriptor_t;
/* Configuration descriptor structure */
typedef struct
{
    uint8_t bLength;             // Length of this descriptor.
    uint8_t bDescriptorType;     // CONFIGURATION descriptor type (USB_DESCRIPTOR_CONFIGURATION).
    uint16_t wTotalLength;       // Total length of all descriptors for this configuration.
    uint8_t bNumInterfaces;      // Number of interfaces in this configuration.
    uint8_t bConfigurationValue; // Value of this configuration (1 based).
    uint8_t iConfiguration;      // Index of String Descriptor describing the configuration.
    uint8_t bmAttributes;        // Configuration characteristics.
    uint8_t bMaxPower;           // Maximum power consumed by this configuration.
} __attribute__((packed)) usbConfigurationDescriptor_t;

/* Interface descriptor structure */
typedef struct
{
    uint8_t bLength;            // Length of this descriptor.
    uint8_t bDescriptorType;    // INTERFACE descriptor type (USB_DESCRIPTOR_INTERFACE).
    uint8_t bInterfaceNumber;   // Number of this interface (0 based).
    uint8_t bAlternateSetting;  // Value of this alternate interface setting.
    uint8_t bNumEndpoints;      // Number of endpoints in this interface.
    uint8_t bInterfaceClass;    // Class code (assigned by the USB-IF).  0xFF-Vendor specific.
    uint8_t bInterfaceSubClass; // Subclass code (assigned by the USB-IF).
    uint8_t bInterfaceProtocol; // Protocol code (assigned by the USB-IF).  0xFF-Vendor specific.
    uint8_t iInterface;         // Index of String Descriptor describing the interface.
} __attribute__((packed)) usbInterfaceDescriptor_t;

/* Endpoint descriptor structure */
typedef struct
{
    uint8_t bLength;          // Length of this descriptor.
    uint8_t bDescriptorType;  // ENDPOINT descriptor type (USB_DESCRIPTOR_ENDPOINT).
    uint8_t bEndpointAddress; // Endpoint address. Bit 7 indicates direction (0=OUT, 1=IN).
    uint8_t bmAttributes;     // Endpoint transfer type.
    uint16_t wMaxPacketSize;  // Maximum packet size.
    uint8_t bInterval;        // Polling interval in frames.
} __attribute__((packed)) usbEndpointDescriptor_t;

typedef struct
{
    uint8_t bLength;         //
    uint8_t bDescriptorType; // 3 for string descriptor
    uint16_t wLANGID[1];     //

} __attribute__((packed)) usbStringDescriptorZero_t;
typedef struct
{
    uint8_t bLength;         //
    uint8_t bDescriptorType; // 3 for string descriptor
    char String[0];          //

} __attribute__((packed)) usbStringDescriptor_t;
// requests. Note, this assme little endian. Is there someone still using big endian in 2021 ?
// (unfortunately yes, but luckily we aren't!)
typedef struct
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint8_t bValid; // 0 if it has been already handled.
    uint8_t epDataEvent;
} __attribute__((packed)) usbRequest_t;

// INLINE USBD UTIITY FUNCTIONS
// wait for usb event and clear it
static inline uint32_t usbdWaitForEventAndClear(volatile uint32_t * eventReg, uint32_t timeoutMs)
{
    // note: timeout not implemented yet.
    (void)timeoutMs;
    while (!*eventReg)
    {
    }
    *eventReg = 0;
    return 0;
}
// wait for a specific event cause and clear it
static inline void usbdEventcauseWaitAndClear(uint32_t eventcause)
{
    while (0 == (eventcause & NRF_USBD->EVENTCAUSE))
    {
        // do nothing
    }
    // clear event cause
    NRF_USBD->EVENTCAUSE = eventcause;
    __ISB();
    __DSB();
}
static inline void usbdTaskTrigger(volatile uint32_t * pTask)
{
    *pTask = 1;
    __ISB();
    __DSB();
}
static inline void disableUsbIrq()
{
    NVIC_DisableIRQ(USBD_IRQn);
}
static inline void enableUsbIrq()
{
    NVIC_EnableIRQ(USBD_IRQn);
}

uint8_t usbGetState();

bool enableUsb(void);
#endif