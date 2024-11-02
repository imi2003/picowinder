#include "common/tusb_common.h"
#include "device/usbd.h"

#include "hid_pid.h"

#define SIDEWINDER_REPORT_DESC_INPUT(...) \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
    HID_USAGE(HID_USAGE_DESKTOP_JOYSTICK), \
    HID_COLLECTION(HID_COLLECTION_APPLICATION), \
        /* Report ID */ __VA_ARGS__ \
        HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON), \
            HID_USAGE_MIN(1), \
            HID_USAGE_MAX(9), \
            HID_LOGICAL_MIN(0), \
            HID_LOGICAL_MAX(1), \
            HID_REPORT_COUNT(9), \
            HID_REPORT_SIZE(1), \
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
            \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(7), \
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
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(6), \
            HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
            /* Y: 10 data bits, 6 padding bits */ \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(10), \
            HID_USAGE(HID_USAGE_DESKTOP_Y), \
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(6), \
            HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
            /* Rz (joystick twist): 6 data bits, 2 padding bits */ \
            HID_LOGICAL_MIN(0), \
            HID_LOGICAL_MAX(63), \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(6), \
            HID_USAGE(HID_USAGE_DESKTOP_RZ), \
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(2), \
            HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
            /* Slider (throttle; kind of a dial): 7 data bits, 1 padding bit */ \
            HID_LOGICAL_MIN(0), \
            HID_LOGICAL_MAX(127), \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(7), \
            HID_USAGE(HID_USAGE_DESKTOP_SLIDER), \
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(1), \
            HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
            /* Hat switch: 4 data bits, 4 padding bits */ \
            HID_LOGICAL_MIN(1), \
            HID_LOGICAL_MAX(8), \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(4), \
            HID_USAGE(HID_USAGE_DESKTOP_HAT_SWITCH), \
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
            HID_REPORT_COUNT(1), \
            HID_REPORT_SIZE(4), \
            HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE), \
            \
    HID_COLLECTION_END


/////////////////////////////////////////////////////////////////////
// Output Report 1: Set Effect - define params for an effect
/////////////////////////////////////////////////////////////////////

#define SIDEWINDER_REPORT_DESC_SET_EFFECT(...) \
    HID_USAGE_PAGE(HID_USAGE_PAGE_PID), \
    HID_USAGE(HID_USAGE_PID_SET_EFFECT_REPORT), \
    HID_COLLECTION(HID_COLLECTION_LOGICAL), \
        /* Report ID */ __VA_ARGS__ \
        \
        /* Block Index */ \
        HID_USAGE(HID_USAGE_PID_EFFECT_PARAM_BLOCK_INDEX), \
        HID_LOGICAL_MIN(1), \
        HID_LOGICAL_MAX(40), \
        HID_PHYSICAL_MIN(1), \
        HID_PHYSICAL_MAX(40), \
        HID_REPORT_SIZE(8), \
        HID_REPORT_COUNT(1), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Effect Type */ \
        HID_USAGE(HID_USAGE_PID_EFFECT_TYPE), \
        HID_COLLECTION(HID_COLLECTION_LOGICAL), \
            HID_USAGE(HID_USAGE_PID_ET_CONSTANT_FORCE), \
            HID_USAGE(HID_USAGE_PID_ET_RAMP), \
            HID_USAGE(HID_USAGE_PID_ET_SQUARE), \
            HID_USAGE(HID_USAGE_PID_ET_SINE), \
            HID_USAGE(HID_USAGE_PID_ET_TRIANGLE), \
            HID_USAGE(HID_USAGE_PID_ET_SAWTOOTH_UP), \
            HID_USAGE(HID_USAGE_PID_ET_SAWTOOTH_DOWN), \
            HID_USAGE(HID_USAGE_PID_ET_SPRING), \
            HID_USAGE(HID_USAGE_PID_ET_DAMPER), \
            HID_USAGE(HID_USAGE_PID_ET_INERTIA), \
            HID_USAGE(HID_USAGE_PID_ET_FRICTION), \
            /* Unlike the FF2, ET_CUSTOM_FORCE is not supported here. */ \
            HID_LOGICAL_MIN(1), \
            HID_LOGICAL_MAX(11), \
            HID_PHYSICAL_MIN(1), \
            HID_PHYSICAL_MAX(11), \
            HID_REPORT_SIZE(8), \
            HID_REPORT_COUNT(1), \
            HID_OUTPUT(HID_DATA | HID_ARRAY | HID_ABSOLUTE), \
        HID_COLLECTION_END, \
        \
        /* Duration, Trigger Repeat, Sample Period */ \
        HID_USAGE(HID_USAGE_PID_DURATION), \
        HID_USAGE(HID_USAGE_PID_TRIGGER_REPEAT_INTERVAL), \
        HID_USAGE(HID_USAGE_PID_SAMPLE_PERIOD), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX_N(32767, 2), \
        HID_PHYSICAL_MIN(0), \
        HID_PHYSICAL_MAX_N(32767, 2), \
        HID_UNIT_N(4099, 2), \
        HID_UNIT_EXPONENT(-3), \
        HID_REPORT_SIZE(16), \
        HID_REPORT_COUNT(3), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Gain */ \
        HID_USAGE(HID_USAGE_PID_GAIN), \
        HID_UNIT_EXPONENT(0), \
        HID_UNIT(0), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX_N(255, 2), \
        HID_PHYSICAL_MIN(0), \
        HID_PHYSICAL_MAX_N(10000, 2), \
        HID_REPORT_SIZE(8), \
        HID_REPORT_COUNT(1), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Trigger Button */ \
        HID_USAGE(HID_USAGE_PID_TRIGGER_BUTTON), \
        HID_LOGICAL_MIN(1), \
        HID_LOGICAL_MAX(9), \
        HID_PHYSICAL_MIN(1), \
        HID_PHYSICAL_MAX(9), \
        HID_REPORT_SIZE(8), \
        HID_REPORT_COUNT(1), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Axes Enable */ \
        HID_USAGE(HID_USAGE_PID_AXES_ENABLE), \
        HID_COLLECTION(HID_COLLECTION_LOGICAL), \
            HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
            HID_USAGE(HID_USAGE_DESKTOP_X), \
            HID_USAGE(HID_USAGE_DESKTOP_Y), \
            HID_LOGICAL_MIN(0), \
            HID_LOGICAL_MAX(1), \
            HID_REPORT_SIZE(1), \
            HID_REPORT_COUNT(2), \
            HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        HID_COLLECTION_END, \
        \
        /* Direction Enable */ \
        HID_USAGE_PAGE(HID_USAGE_PAGE_PID), \
        HID_USAGE(HID_USAGE_PID_DIRECTION_ENABLE), \
        HID_REPORT_COUNT(1), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        HID_REPORT_COUNT(5), \
        HID_OUTPUT(HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Direction */ \
        HID_USAGE(HID_USAGE_PID_DIRECTION), \
        HID_COLLECTION(HID_COLLECTION_LOGICAL), \
            /* TODO: this part is different from the FFB 2 */ \
            HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
            HID_USAGE(HID_USAGE_DESKTOP_X), \
            HID_USAGE(HID_USAGE_DESKTOP_Y), \
            /* end TODO */ \
            HID_UNIT(20), \
            HID_UNIT_EXPONENT(-2), \
            HID_LOGICAL_MIN(0), \
            HID_LOGICAL_MAX_N(255, 2), \
            HID_PHYSICAL_MIN(0), \
            HID_PHYSICAL_MAX(0), \
            HID_UNIT(0), \
            HID_REPORT_SIZE(8), \
            HID_REPORT_COUNT(2), \
            HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
            HID_UNIT_EXPONENT(0), \
            HID_UNIT(0), \
        HID_COLLECTION_END, \
        \
        /* Start Delay */ \
        HID_USAGE_PAGE(HID_USAGE_PAGE_PID), \
        HID_USAGE(HID_USAGE_PID_START_DELAY), \
        HID_UNIT_N(4099, 2), \
        HID_UNIT_EXPONENT(-3), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX_N(32767, 2), \
        HID_PHYSICAL_MIN(0), \
        HID_PHYSICAL_MAX_N(32767, 2), \
        HID_REPORT_SIZE(16), \
        HID_REPORT_COUNT(1), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        HID_UNIT(0), \
        HID_UNIT_EXPONENT(0), \
    HID_COLLECTION_END

/////////////////////////////////////////////////////////////////////
// Output Report 10: Effect Operation - stop/start effects
/////////////////////////////////////////////////////////////////////

#define SIDEWINDER_REPORT_DESC_EFFECT_OPERATION(...) \
    HID_USAGE_PAGE(HID_USAGE_PAGE_PID), \
    HID_USAGE(HID_USAGE_PID_EFFECT_OPERATION_REPORT), \
    HID_COLLECTION(HID_COLLECTION_LOGICAL), \
        /* Report ID */ __VA_ARGS__ \
        \
        /* Block Index */ \
        HID_USAGE(HID_USAGE_PID_EFFECT_PARAM_BLOCK_INDEX), \
        HID_LOGICAL_MIN(1), \
        HID_LOGICAL_MAX(40), \
        HID_PHYSICAL_MIN(1), \
        HID_PHYSICAL_MAX(40), \
        HID_REPORT_SIZE(8), \
        HID_REPORT_COUNT(1), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Effect Operation */ \
        HID_USAGE(HID_USAGE_PID_EFFECT_OPERATION), \
        HID_COLLECTION(HID_COLLECTION_LOGICAL), \
            HID_USAGE(HID_USAGE_PID_OP_EFFECT_START), \
            HID_USAGE(HID_USAGE_PID_OP_EFFECT_START_SOLO), \
            HID_USAGE(HID_USAGE_PID_OP_EFFECT_STOP), \
            HID_LOGICAL_MIN(1), \
            HID_LOGICAL_MAX(3), \
            HID_REPORT_SIZE(8), \
            HID_REPORT_COUNT(1), \
            HID_OUTPUT(HID_DATA | HID_ARRAY | HID_ABSOLUTE), \
        HID_COLLECTION_END, \
        \
        /* Loop Count */ \
        HID_USAGE(HID_USAGE_PID_LOOP_COUNT), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX_N(255, 2), \
        HID_PHYSICAL_MIN(0), \
        HID_PHYSICAL_MAX_N(255, 2), \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
    HID_COLLECTION_END


/////////////////////////////////////////////////////////////////////
// Feature Report 3: Pool size, max simultaneous effects, etc.
/////////////////////////////////////////////////////////////////////

#define SIDEWINDER_REPORT_DESC_FEATURE_POOL_REPORT(...) \
    HID_USAGE_PAGE(HID_USAGE_PAGE_PID), \
    HID_COLLECTION(HID_COLLECTION_LOGICAL), \
        /* Report ID */ __VA_ARGS__ \
        \
        /* RAM Pool Size */ \
        HID_USAGE(HID_USAGE_PID_RAM_POOL_SIZE), \
        HID_REPORT_SIZE(16), \
        HID_REPORT_COUNT(1), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX(0), \
        HID_PHYSICAL_MIN(0), \
        HID_PHYSICAL_MAX(0), \
        HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Simultaneous Maximum Effects */ \
        HID_USAGE(HID_USAGE_PID_SIMULTANEOUS_EFFECTS_MAX), \
        HID_LOGICAL_MAX_N(255, 2), \
        HID_PHYSICAL_MAX_N(255, 2), \
        HID_REPORT_SIZE(8), \
        HID_REPORT_COUNT(1), \
        HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* Managed Pool, Shared Blocks */ \
        HID_USAGE(HID_USAGE_PID_DEVICE_MANAGED_POOL), \
        HID_USAGE(HID_USAGE_PID_SHARED_PARAMETER_BLOCKS), \
        HID_LOGICAL_MAX(1), \
        HID_PHYSICAL_MAX(1), \
        HID_REPORT_SIZE(1), \
        HID_REPORT_COUNT(2), \
        HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
        \
        /* padding */ \
        HID_REPORT_SIZE(6), \
        HID_REPORT_COUNT(1), \
        HID_FEATURE(HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE), \
        \
    HID_COLLECTION_END

/*
Usage PID Pool Report (0x7f)
Collection Logical (0x02)

    Usage Device Managed Pool (0xa9)
    Usage Shared Parameter Blocks (0xaa)
    Report Size 1
    Report Count 2
    Logical Minimum 0 Logical Maximum 1
    Physical Minimum 0 Physical Maximum 1
    Feature Data (0x02)

    Report Size 6
    Report Count 1
    Feature Constant (0x03)

End Collection
*/