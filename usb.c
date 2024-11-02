#include "tusb.h"

#define REPORT_ID_FEATURE_POOL_REPORT 3

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
        hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void) reqlen;

    switch (report_type)
    {
        case HID_REPORT_TYPE_FEATURE:
            switch (report_id)
            {
                // Feature Report 3: Pool size, max simultaneous effects, etc.
                case REPORT_ID_FEATURE_POOL_REPORT:
                    buffer[0] = 40; // simultaneous effects
                    buffer[1] = 0x80;
                    return 2;
            }
            break;
    }

    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
        hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    bool echo = true;

    // Echo back anything we received from the host
    if (echo) { tud_hid_report(0, buffer, bufsize); }
}