/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"

#include "usb_descriptors.h"
#include "bsp/board_api.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xcafe,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

/*
    Note: hid-pidff.c in the Linux kernel will "oops" due to a null-pointer deref
    if any of its "optional" reports are not present. These include:
    Set Envelope, Set Condition, Set Periodic, Set Constant, Set Ramp.
*/

uint8_t const desc_hid_report[] =
{
    // Input Reports
    SIDEWINDER_REPORT_DESC_INPUT(HID_REPORT_ID(1)),
    // Output Reports
    SIDEWINDER_REPORT_DESC_OUTPUT_SET_EFFECT            (HID_REPORT_ID(1)),
    SIDEWINDER_REPORT_DESC_OUTPUT_SET_ENVELOPE          (HID_REPORT_ID(2)),
    SIDEWINDER_REPORT_DESC_OUTPUT_SET_CONDITION         (HID_REPORT_ID(3)),
    SIDEWINDER_REPORT_DESC_OUTPUT_SET_PERIODIC          (HID_REPORT_ID(4)),
    SIDEWINDER_REPORT_DESC_OUTPUT_SET_CONSTANT          (HID_REPORT_ID(5)),
    SIDEWINDER_REPORT_DESC_OUTPUT_SET_RAMP              (HID_REPORT_ID(6)),
    SIDEWINDER_REPORT_DESC_OUTPUT_EFFECT_OPERATION      (HID_REPORT_ID(10)),
    SIDEWINDER_REPORT_DESC_OUTPUT_BLOCK_FREE            (HID_REPORT_ID(11)),
    SIDEWINDER_REPORT_DESC_OUTPUT_DEVICE_CONTROL        (HID_REPORT_ID(12)),
    SIDEWINDER_REPORT_DESC_OUTPUT_DEVICE_GAIN           (HID_REPORT_ID(13)),
    // Feature Reports
    SIDEWINDER_REPORT_DESC_FEATURE_CREATE_NEW_EFFECT    (HID_REPORT_ID(1)),
    SIDEWINDER_REPORT_DESC_FEATURE_BLOCK_LOAD           (HID_REPORT_ID(2)),
    SIDEWINDER_REPORT_DESC_FEATURE_POOL_REPORT          (HID_REPORT_ID(3)),
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID   0x81

uint8_t const desc_configuration[] =
{
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1)
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// String Descriptor Index
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

// array of pointer to string descriptors
char const *string_desc_arr[] =
{
    (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
    "Nolbinsoft",                  // 1: Manufacturer
    "Picowinder",                  // 2: Product
    NULL,                          // 3: Serials will use unique ID if possible
};

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t _desc_str[32];
    const size_t max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type

    (void) langid;
    size_t chr_count;

    switch ( index )
    {
        case STRID_LANGID:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;

            break;

        case STRID_SERIAL:
            chr_count = board_usb_get_serial(_desc_str + 1, 32);

            break;

        default:

            if ( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) { return NULL; }

            const char *str = string_desc_arr[index];

            // Cap at max char
            chr_count = strlen(str);
            if ( chr_count > max_count ) { chr_count = max_count; }

            // Convert ASCII string into UTF-16
            for ( size_t i = 0; i < chr_count; i++ )
            {
                _desc_str[1 + i] = str[i];
            }

            break;
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}