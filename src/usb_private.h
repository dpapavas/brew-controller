/* Copyright (C) 2024 Papavasileiou Dimitris
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _USB_PRIVATE_H_
#define _USB_PRIVATE_H_

#include "stdint.h"

#define ENDPOINTS_N 3
#define CONTROL_BUFFER_SIZE 64
#define NOTIFICATION_BUFFER_SIZE 16
#define DATA_BUFFER_SIZE 64

#define COMMUNICATION_INTERFACE 0
#define DATA_INTERFACE 1

#define CONTROL_ENDPOINT 0
#define NOTIFICATION_ENDPOINT 1
#define DATA_ENDPOINT 2

#define BDT_DESC_OWN ((uint32_t)1 << 7)
#define BDT_DESC_DATA1 ((uint32_t)1 << 6)
#define BDT_DESC_DTS ((uint32_t)1 << 3)
#define BDT_DESC_STALL ((uint32_t)1 << 2)
#define BDT_DESC_PID(n) ((n >> 2) & 0b1111)
#define BDT_DESC_BYTE_COUNT(n) ((n >> 16) & ((1 << 10) - 1))

#define BDT_PID_OUT 0x1
#define BDT_PID_IN 0x9
#define BDT_PID_SETUP 0xd

#define SETUP_DEVICE_GET_STATUS (0x0080)
#define SETUP_DEVICE_CLEAR_FEATURE (0x0100)
#define SETUP_DEVICE_SET_FEATURE (0x0300)
#define SETUP_DEVICE_SET_ADDRESS (0x0500)
#define SETUP_DEVICE_GET_DESCRIPTOR (0x0680)
#define SETUP_DEVICE_SET_DESCRIPTOR (0x0700)
#define SETUP_DEVICE_GET_CONFIGURATION (0x0880)
#define SETUP_DEVICE_SET_CONFIGURATION (0x0900)

#define SETUP_INTERFACE_GET_STATUS (0x0081)
#define SETUP_INTERFACE_CLEAR_FEATURE (0x0101)
#define SETUP_INTERFACE_SET_FEATURE (0x0301)
#define SETUP_INTERFACE_GET_INTERFACE (0x0a81)
#define SETUP_INTERFACE_SET_INTERFACE (0x1101)

#define SETUP_ENDPOINT_GET_STATUS (0x0082)
#define SETUP_ENDPOINT_CLEAR_FEATURE (0x0102)
#define SETUP_ENDPOINT_SET_FEATURE (0x0302)
#define SETUP_ENDPOINT_SYNCH_FRAME (0x1282)

#define SETUP_SET_LINE_CODING 0x2021
#define SETUP_GET_LINE_CODING 0x21a1
#define SETUP_SET_CONTROL_LINE_STATE 0x2221
#define SETUP_SEND_BREAK 0x2321

#define DESCRIPTOR_TYPE(value) ((uint8_t)((value) >> 8))
#define DESCRIPTOR_TYPE_DEVICE 1
#define DESCRIPTOR_TYPE_DEVICE_CONFIGURATION 2
#define DESCRIPTOR_TYPE_DEVICE_QUALIFIER 6

#define oddbit(endpoint, tx) ((oddbits & ((1 << (2 * (endpoint) + (tx))))) != 0)
#define toggle_oddbit(endpoint, tx) oddbits ^= ((1 << (2 * (endpoint) + (tx))))
#define reset_oddbit(endpoint, tx) oddbits &= ~((1 << (2 * (endpoint) + (tx))))

#define bdt_descriptor(count, data1)                                    \
    (((data1) ? BDT_DESC_DATA1 : 0) | BDT_DESC_DTS |                    \
     (((uint32_t)(count)) << 16))

#define bdt_entry(n, tx, odd) (&bdt[((n) << 2) | ((tx) << 1) | ((odd) != 0)])

#define LSB(s) ((s) & 0xff)
#define MSB(s) (((s) >> 8) & 0xff)

static uint8_t device_descriptor[] = {
    18,                         /* bLength */
    1,                          /* bDescriptorType */
    0x00, 0x02,                 /* bcdUSB */
    0x02,                       /* bDeviceClass (2 = CDC) */
    0,                          /* bDeviceSubClass */
    0,                          /* bDeviceProtocol */
    CONTROL_BUFFER_SIZE,        /* bMaxPacketSize0 */
    0xc0, 0x16,                 /* idVendor */
    0x86, 0x04,                 /* idProduct */
    0x00, 0x01,                 /* bcdDevice */
    0,                          /* iManufacturer */
    0,                          /* iProduct */
    0,                          /* iSerial */
    1,                          /* bNumConfigurations */
};

#define CONFIGURATION_DESCRIPTOR_SIZE (9+9+5+5+4+5+7+9+7+7)
static uint8_t configuration_descriptor[CONFIGURATION_DESCRIPTOR_SIZE] = {
    /* Configuration descriptor. */

    9,                                  /* bLength */
    2,                                  /* bDescriptorType */
    LSB(CONFIGURATION_DESCRIPTOR_SIZE), /* wTotalLength */
    MSB(CONFIGURATION_DESCRIPTOR_SIZE),
    2,                                  /* bNumInterfaces */
    1,                                  /* bconfigurationvalue */
    0,                                  /* iConfiguration */
    0xc0,                               /* bmAttributes */
    250,                                /* bMaxPower */

    /* Interface descriptor. */

    9,                          /* bLength */
    4,                          /* bDescriptorType */
    COMMUNICATION_INTERFACE,    /* bInterfaceNumber */
    0,                          /* bAlternateSetting */
    1,                          /* bNumEndpoints */
    0x02,    /* bInterfaceClass (2 = Communication Interface Class) */
    0x02,    /* bInterfaceSubClass (2 = ACM subclass) */
    0x00,    /* bInterfaceProtocol */
    0,       /* iInterface */

    /* CDC Header Functional Descriptor. */

    5,                          /* bFunctionLength */
    0x24,                       /* bDescriptorType */
    0x00,                       /* bDescriptorSubtype */
    0x10, 0x01,                 /* bcdCDC */

    /* Call Management Functional Descriptor */

    5,                          /* bFunctionLength */
    0x24,                       /* bDescriptorType */
    0x01,                       /* bDescriptorSubtype */
    0x00,                       /* bmCapabilities */
    1,                          /* bDataInterface */

    /* Abstract Control Management Functional Descriptor. */

    4,                          /* bFunctionLength */
    0x24,                       /* bDescriptorType */
    0x02,                       /* bDescriptorSubtype */
    0x02,                       /* bmCapabilities */

    /* Union Functional Descriptor. */

    5,                          /* bFunctionLength */
    0x24,                       /* bDescriptorType */
    0x06,                       /* bDescriptorSubtype */
    0,                          /* bMasterInterface */
    1,                          /* bSlaveInterface0 */

    /* Endpoint descriptor. */

    7,                            /* bLength */
    5,                            /* bDescriptorType */
    NOTIFICATION_ENDPOINT | 0x80, /* bEndpointAddress */
    0x03,                         /* bmAttributes (0x03=intr) */
    NOTIFICATION_BUFFER_SIZE, 0,  /* wMaxPacketSize */
    64,

    /* Interface descriptor. */

    9,               /* bLength */
    4,               /* bDescriptorType */
    DATA_INTERFACE,  /* bInterfaceNumber */
    0,               /* bAlternateSetting */
    2,               /* bNumEndpoints */
    0x0a,            /* bInterfaceClass (0a = Data Interface Class) */
    0x00,            /* bInterfaceSubClass */
    0x00,            /* bInterfaceProtocol */
    0,

    /* Endpoint descriptor. */

    7,                          /* bLength */
    5,                          /* bDescriptorType */
    DATA_ENDPOINT,              /* bEndpointAddress */
    0x02,                       /* bmAttributes (0x02=bulk) */
    DATA_BUFFER_SIZE, 0,        /* wMaxPacketSize */
    0,

    /* Endpoint descriptor. */

    7,                          /* bLength */
    5,                          /* bDescriptorType */
    DATA_ENDPOINT | 0x80,       /* bEndpointAddress */
    0x02,                       /* bmAttributes (0x02=bulk) */
    DATA_BUFFER_SIZE, 0,        /* wMaxPacketSize */
    0                           /* bInterval */
};

struct bdtentry {
    uint32_t desc;
    void *buffer;
};

#endif
