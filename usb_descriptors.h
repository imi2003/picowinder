#include "common/tusb_common.h"
#include "device/usbd.h"

#define SIDEWINDER_REPORT_DESC(...) \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
        HID_USAGE(HID_USAGE_DESKTOP_JOYSTICK), \
        HID_COLLECTION(HID_COLLECTION_APPLICATION), \
            /* Report ID */__VA_ARGS__ \
            HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON), \
                HID_USAGE_MIN(1), \
                HID_USAGE_MAX(9), \
                HID_LOGICAL_MIN(0), \
                HID_LOGICAL_MAX(1), \
                HID_REPORT_COUNT(9), \
                HID_REPORT_SIZE(1), \
                HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
                \
                HID_REPORT_SIZE(7), \
                HID_REPORT_COUNT(1), \
                HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
                \
            HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
                /* X and Y are both reported as unsigned 10-bit */ \
                HID_LOGICAL_MIN(0), \
                HID_LOGICAL_MAX_N(1023, 2), \
                /* X: 10 data bits, 6 padding bits */ \
                HID_REPORT_COUNT(1), \
                HID_REPORT_SIZE(10), \
                HID_USAGE(HID_USAGE_DESKTOP_X), \
                HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
                HID_REPORT_SIZE(6), \
                HID_REPORT_COUNT(1), \
                HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
                /* Y: 10 data bits, 6 padding bits */ \
                HID_REPORT_COUNT(1), \
                HID_REPORT_SIZE(10), \
                HID_USAGE(HID_USAGE_DESKTOP_Y), \
                HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
                HID_REPORT_SIZE(6), \
                HID_REPORT_COUNT(1), \
                HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
                \
            HID_COLLECTION_END

/* FFB 2 descriptor (before FFB stuff):
Usage Page Generic Desktop Controls (0x01)
Usage Joystick (0x04)
Collection Application (0x01)
    Report ID 1
    Usage Pointer (0x01)
    Collection Physical (0x00)
        Unit 0
        Unit Exponent 0
        Logical Minimum -512
        Logical Maximum 511
        Physical Minimum 0
        Physical Maximum 1023
        Report Count 1

        Report Size 10
        Usage X (0x30)
        Input Data (0x02)

        Report Size 6
        Input Constant (0x01)

        Usage Y (0x31)
        Report Size 10
        Input Data (0x02)

        Report Size 6
        Report Count 1
        Input Constant (0x01)

        Logical Minimum -32
        Logical Maximum 31
        Physical Minimum 0
        Physical Maximum 63
        Unit 20

        Usage Rz (0x35)
        Input Data (0x02)

        Report Size 2
        Input Constant (0x01)

        Unit 0
        End Collection

    Report Size 7
    Report Count 1
    Logical Minimum 0
    Logical Maximum 127
    Physical Minimum 0
    Physical Maximum 127
    Usage Slider (0x36)
    Input Data (0x02)
    Report Size 1
    Input Constant (0x01)
    Usage Hat switch (0x39)
    Logical Minimum 0
    Logical Maximum 7
    Physical Minimum 0
    Physical Maximum 315
    Unit 20
    Report Size 4
    Report Count 1
    Input Data (0x42)
    Report Count 1
    Input Constant (0x01)
    Unit 0
    Usage Page Button (0x09)
    Usage Minimum Button 1 (primary/trigger)(0x01)
    Usage Maximum Button 8 (0x08)
    Logical Minimum 0
    Logical Maximum 1
    Physical Minimum 0
    Physical Maximum 1
    Report Count 8
    Report Size 1
    Input Data (0x02)
    Report Count 4
    Report Size 8
    Input Constant (0x01)
*/
