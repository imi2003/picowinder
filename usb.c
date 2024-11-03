#include "tusb.h"
#include "usb_report_ids.h"


uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
        hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    switch (report_type)
    {
        case HID_REPORT_TYPE_FEATURE:
            switch (report_id)
            {
                // Block Load Request
                case REPORT_ID_FEATURE_BLOCK_LOAD:
                    buffer[0] = 0x00; // block index
                    buffer[1] = 0x02; // block full (test for now)
                    buffer[2] = 0x00; // RAM pool available
                    buffer[3] = 0x00;
                    return 4;

                // Pool size, max simultaneous effects, etc.
                case REPORT_ID_FEATURE_POOL_REPORT:
                    buffer[0] = 0x00; // RAM pool size
                    buffer[1] = 0x00;
                    buffer[2] = 40; // simultaneous effects
                    buffer[3] = 0x80;
                    return 4;
            }

            break;
    }

    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
        hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    bool echo = true;

    switch (report_type)
    {
        case HID_REPORT_TYPE_FEATURE:
            switch(report_id)
            {
                case REPORT_ID_FEATURE_CREATE_NEW_EFFECT:
                    // Only one byte: the effect type
                    break;
            }
            break;
    }

    // Echo back anything we received from the host
    if (echo) { tud_hid_report(0, buffer, bufsize); }
}