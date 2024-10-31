#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "tusb.h"

#include "ffb_handshake.pio.h"
#include "read_joystick.pio.h"


#define PIN_MIDI_TX 0
#define PIN_TRIGGER 2
#define PIN_CLK     3
#define PIN_D0      4
#define PIN_D1      5
#define PIN_D2      6


struct JoystickState
{
    uint16_t buttons    :  9;
    uint16_t x          : 10;
    uint16_t y          : 10;
    uint8_t  throttle   :  7;
    uint8_t  twist      :  6;
    uint8_t  hat        :  4;
} joystickState;

struct report
{
    uint16_t buttons;
    uint16_t x;
    uint16_t y;
    uint8_t twist;
    uint8_t throttle;
    uint8_t hat;
} report;

uint16_t report_size_bytes = 9; // sizeof doesn't necessarily work well due to packing

uint rxFifoEntries;
uint32_t raw0;
uint32_t raw1;
uint64_t joystickStateRaw;


void midi_start(uart_inst_t *uart)
{
    const uint8_t start0[] = {
        0xc5, 0x01 // <ProgramChange> 0x01
    };

    const uint8_t start1[] = {
        0xf0, // SysEx begin
        0x00, 0x01, 0x0a, 0x01, 0x10, 0x05, 0x6b,
        0xf7  // SysEx end
    };

    const uint8_t start2[] = {
            0xb5, 0x40, 0x7f, // <ControlChange>(Modify, 0x7f)
            0xa5, 0x72, 0x57, // offset 0x72 := 0x57

            0xb5, 0x44, 0x7f,
            0xa5, 0x3c, 0x43,

            0xb5, 0x48, 0x7f,
            0xa5, 0x7e, 0x00,

            0xb5, 0x4c, 0x7f,
            0xa5, 0x04, 0x00,

            0xb5, 0x50, 0x7f,
            0xa5, 0x02, 0x00,

            0xb5, 0x54, 0x7f,
            0xa5, 0x02, 0x00,

            0xb5, 0x58, 0x7f,
            0xa5, 0x00, 0x7e,

            0xb5, 0x5c, 0x7f,
            0xa5, 0x3c, 0x00,

            0xb5, 0x60, 0x7f,
            0xa5, 0x14, 0x65,

            0xb5, 0x64, 0x7f,
            0xa5, 0x7e, 0x6b,

            0xb5, 0x68, 0x7f,
            0xa5, 0x36, 0x00,

            0xb5, 0x6c, 0x7f,
            0xa5, 0x28, 0x00,

            0xb5, 0x70, 0x7f,
            0xa5, 0x66, 0x4c,

            0xb5, 0x74, 0x7f,
            0xa5, 0x7e, 0x01
    };

    uart_write_blocking(uart, start0, sizeof(start0));
    sleep_ms(20);
    uart_write_blocking(uart, start1, sizeof(start1));
    sleep_ms(57);
    uart_write_blocking(uart, start2, sizeof(start2));
}

void midi_autocenter(uart_inst_t *uart, bool enabled)
{
    const uint8_t autocenter_on[] = {
            0xc5, 0x01
    };

    const uint8_t autocenter_off[] = {
            0xb5, 0x7c, 0x7f,
            0xa5, 0x7f, 0x00,
            0xc5, 0x06,
    };

    uart_write_blocking(uart, autocenter_on, sizeof(autocenter_on));

    if (!enabled)
    {
        sleep_ms(70);
        uart_write_blocking(uart, autocenter_off, sizeof(autocenter_off));
    }
}

enum EffectType
{
    SINE            = 0x02,
    SQUARE          = 0x05,
    RAMP            = 0x06,
    TRIANGLE        = 0x08,
    SAWTOOTHDOWN    = 0x0a,
    SAWTOOTHUP      = 0x0b,
    SPRING          = 0x0d,
    DAMPER          = 0x0e,
    INERTIA         = 0x0f,
    FRICTION        = 0x10,
    CONSTANT        = 0x12,
};

#define MODIFY_DURATION     0x40

#define MODIFY_DIRECTION    0x48
#define MODIFY_STRENGTH     0x4c
#define MODIFY_SAMPLE_RATE  0x50
#define MODIFY_ATTACK_TIME  0x5c
#define MODIFY_FADE_TIME    0x60    // TODO verify
#define MODIFY_ATTACK       0x64
#define MODIFY_FADE         0x6c
#define MODIFY_FREQUENCY    0x70
#define MODIFY_AMPLITUDE1   0x74    // TODO figure this one out
#define MODIFY_AMPLITUDE2   0x78    // TODO figure this one out

#define MODIFY_STRENGTH_X   0x48
#define MODIFY_STRENGTH_Y   0x4c
#define MODIFY_OFFSET_X     0x50
#define MODIFY_OFFSET_Y     0x54


/*
TODO investigate: fade time property isn't behaving as expected.
It appears that the entire effect duration is currently being treated
as the fade time, so neither the initial value nor the modified value
has any effect. It's possible that this behavior is enabled by some other setting.
*/

struct Effect
{
    // Common to all effects
    enum EffectType type;
    uint16_t duration; // in 2 ms units; 0 = inf
    uint16_t button_mask; // 9-bit button mask; 0 = must play manually

    // Constant/Sine/Square/Ramp/Triangle only
    uint16_t direction; // in degrees
    uint8_t strength;
    uint8_t sample_rate; // in Hz; default is generally 100
    uint8_t attack;
    uint8_t fade;
    uint16_t attack_time;
    uint16_t fade_time;
    uint16_t frequency;
    uint16_t amplitude;

    // Inertia/Spring/Friction only
    uint8_t strength_x;
    uint8_t strength_y;

    // Inertia/Spring only
    uint16_t offset_x;
    uint16_t offset_y;
};

/*
Since the MSB is reserved, we get 7 bits of real data per MIDI byte.
14-bit values must be split into high and low 7-bit chunks.
*/
static inline uint8_t lo7(uint16_t val) { return val & 0x7f; }
static inline uint8_t hi7(uint16_t val) { return (val >> 7) & 0x7f; }

void midi_define_effect(uart_inst_t *uart, struct Effect *effect)
{
    uint8_t effect_data[33] = {
        0xf0,                           // 0: SysEx start - effect data
        0x00, 0x01, 0x0a, 0x01, 0x23,   // 1..5: Effect header
        effect->type,                   // 6: enumerated effect type
        0x7f,                           // 7: not used?
        lo7(effect->duration),          // 8, 9: duration
        hi7(effect->duration),
        lo7(effect->button_mask),       // 10, 11: button mask
        hi7(effect->button_mask)
    };

    uint8_t next_index;

    switch (effect->type)
    {
        case CONSTANT:
        case SINE:
        case SQUARE:
        case RAMP:
        case TRIANGLE:
        case SAWTOOTHDOWN:
        case SAWTOOTHUP:

            effect_data[12] = lo7(effect->direction);   // 12, 13: direction
            effect_data[13] = hi7(effect->direction);
            effect_data[14] = effect->strength;         // 14: strength
            effect_data[15] = lo7(effect->sample_rate); // 15, 16: sample rate
            effect_data[16] = hi7(effect->sample_rate);
            effect_data[17] = 0x10;
            effect_data[18] = 0x4e;
            effect_data[19] = effect->attack;           // 19: Envelope Attack Start Level
            effect_data[20] = lo7(effect->attack_time); // 20, 21: Envelope Attack Time
            effect_data[21] = hi7(effect->attack_time);
            effect_data[22] = 0x7f;                     // 22: not used; must be 0x7f
            effect_data[23] = lo7(effect->fade_time);   // 23, 24: Envelope Fade Time
            effect_data[24] = hi7(effect->fade_time);
            effect_data[25] = effect->fade;             // 25: Envelope Fade End Level
            effect_data[26] = lo7(effect->frequency);   // 26, 27: Frequency
            effect_data[27] = hi7(effect->frequency);
            effect_data[28] = lo7(effect->amplitude);   // 28, 29: Amplitude
            effect_data[29] = hi7(effect->amplitude);
            effect_data[30] = 0x01;                     // 30, 31: ???
            effect_data[31] = 0x01;
            next_index = 32;

            break;

        case SPRING:
        case DAMPER:
        case INERTIA:

            effect_data[12] = effect->strength_x;
            effect_data[13] = 0x00;
            effect_data[14] = effect->strength_y;
            effect_data[15] = 0x00;
            effect_data[16] = lo7(effect->offset_x);
            effect_data[17] = hi7(effect->offset_x);
            effect_data[18] = lo7(effect->offset_y);
            effect_data[19] = hi7(effect->offset_y);
            next_index = 20;

            break;

        case FRICTION:

            effect_data[12] = effect->strength_x;
            effect_data[13] = 0x00;
            effect_data[14] = effect->strength_y;
            effect_data[15] = 0x00;
            next_index = 16;

            break;
    }

    // Calculate and write checksum
    uint8_t checksum = 0;
    for (int i = 5; i < next_index; i++)
    {
        checksum += effect_data[i];
    }
    effect_data[next_index++] = 0x80 - (checksum & 0x7f);

    effect_data[next_index++] = 0xf7; // SysEx end

    uart_write_blocking(uart, effect_data, next_index);
}

void midi_play(uart_inst_t *uart, uint8_t effect_id)
{
    uint8_t msg[3] = { 0xb5, 0x20, effect_id };
    uart_write_blocking(uart, msg, sizeof(msg));
}

void midi_stop(uart_inst_t *uart, uint8_t effect_id)
{
    uint8_t msg[3] = { 0xb5, 0x30, effect_id };
    uart_write_blocking(uart, msg, sizeof(msg));
}

void midi_modify(uart_inst_t *uart, uint8_t effect_id, uint8_t param, uint16_t value)
{
    uint8_t msg[6] = {
        0xb5, param, effect_id, 0xa5, lo7(value), hi7(value) };
    uart_write_blocking(uart, msg, sizeof(msg));
}

void joystickReadIRQ()
{
    const PIO pio = pio0;
    const uint sm = 0;

    rxFifoEntries = pio_sm_get_rx_fifo_level(pio, sm);

    uint64_t raw0 = pio_sm_get(pio, sm);
    uint64_t raw1 = pio_sm_get(pio, sm);
    uint64_t raw = (raw1 << 16) | (raw0 >> 8);

    joystickState.buttons   = (raw      ) & 0x1ff;
    joystickState.x         = (raw >>  9) & 0x3ff;
    joystickState.y         = (raw >> 19) & 0x3ff;
    joystickState.throttle  = (raw >> 29) & 0x07f;
    joystickState.twist     = (raw >> 36) & 0x03f;
    joystickState.hat       = (raw >> 42) & 0x00f;

    report.buttons = ~joystickState.buttons;
    report.x = joystickState.x;
    report.y = joystickState.y;
    report.twist = joystickState.twist;
    report.throttle = joystickState.throttle;
    report.hat = joystickState.hat;

    pio_interrupt_clear(pio, 0);
}

void hid_task()
{
    if (tud_suspended())
    {
        tud_remote_wakeup();
    }
    else
    {
        if (!tud_hid_ready()) { return; }

        tud_hid_n_report(0x00, 0x01, &report, report_size_bytes);
    }
}


int main()
{
    tud_init(0);

    stdio_init_all();

    // Hardware UART setup.
    // The default is 8 data bits, no parity bit, and 1 stop bit.
    uart_init(uart0, 31250);
    gpio_set_function(PIN_MIDI_TX, UART_FUNCSEL_NUM(uart0, PIN_MIDI_TX));

    // PIO setup
    PIO pio = pio0;
    uint sm = 0;

    // Handshake PIO program setup
    uint offset_handshake = pio_add_program(pio, &ffb_handshake_program);
    uint freq_handshake = 100000;

    ffb_handshake_program_init(pio, sm, offset_handshake,
            freq_handshake, PIN_TRIGGER);

    // Populate the state machine with our pulses/delays.
    // Delays (ms):            7     30    15    78     4    59
    uint delays[7] = { 1000,   70,   300,  150,  780,   40,  590   };
    // Pulses (count):       1     4     3     2     2     3     2
    uint pulses[7] = {       1,    4,    3,    2,    2,    3,    2 };
    for (int i = 0; i < 7; i++)
    {
        uint32_t word = (pulses[i]-1) << 16 | (delays[i] - 1);
        pio_sm_put(pio, sm, word);
    }

    // Activate the state machine for sending the FFB handshake
    pio_sm_set_enabled(pio, sm, true);

    // Wait for the state machine to finish enabling FFB.
    // We could do this via an IRQ, but we know how long it takes.
    sleep_ms(400);

    // The FFB handshake program is done now
    pio_sm_set_enabled(pio, sm, false);

    // Now that the handshake is done, we can send MIDI setup,
    // and disable the built-in auto-centering spring
    midi_start(uart0);
    midi_autocenter(uart0, false);

    // Read-data PIO program setup
    uint offset_readjoy = pio_add_program(pio, &read_joystick_program);
    uint freq_readjoy = 1000000;

    // We blow away the initial FFB-handshake state machine config,
    // because we don't need it anymore.
    read_joystick_program_init(pio, sm, offset_readjoy, freq_readjoy,
            PIN_TRIGGER, PIN_CLK, PIN_D0, PIN_D1, PIN_D2);

    // Set up our IRQ to read the collected joystick data
    uint pio_irq = PIO0_IRQ_0;
    pio_set_irq0_source_enabled(pio0, pis_interrupt0, true);
    irq_set_exclusive_handler(pio_irq, joystickReadIRQ);
    irq_set_enabled(pio_irq, true);

    // Activate the state machine for reading the stick.
    // This one stays on, loops, and keeps firing IRQs.
    pio_sm_set_enabled(pio, sm, true);

    struct Effect sineEffect = {
        .type = SINE,
        .duration = 1000,
        .button_mask = 0,
        .direction = 45,
        .strength = 0x7f,
        .sample_rate = 100,
        .attack = 0x7f,
        .fade = 0x7f,
        .attack_time = 0,
        .fade_time = 0,
        .frequency = 2,
        .amplitude = 0x7f,
    };

    struct Effect wheelEmuSpringEffect = {
        .type = SPRING,
        .duration = 0,
        .button_mask = 0,
        .strength_x = 0,
        .strength_y = 0x7f,
        .offset_x = 0,
        .offset_y = 0,
    };

    struct Effect lightSpringEffect = {
        .type = SPRING,
        .duration = 0,
        .button_mask = 0,
        .strength_x = 0x30,
        .strength_y = 0x30,
        .offset_x = 0,
        .offset_y = 0,
    };

    struct Effect frictionEffect = {
        .type = FRICTION,
        .duration = 0,
        .button_mask = 0,
        .strength_x = 0,
        .strength_y = 0x7f,
    };

    struct Effect kickbackEffect = {
        .type = CONSTANT,
        .duration = 500,
        .button_mask = 0x01,
        .direction = 0,
        .strength = 0x5f,
        .sample_rate = 100,
        .attack = 0x7f,
        .fade = 0x00,
        .attack_time = 0,
        .fade_time = 0,
        .frequency = 1,
        .amplitude = 0x7f,
    };

    struct Effect rumbleEffect = {
        .type = TRIANGLE,
        .duration = 1000,
        .button_mask = 0,
        .direction = 90,
        .strength = 0x7f,
        .sample_rate = 100,
        .attack = 0,
        .fade = 0x7f,
        .attack_time = 0,
        .fade_time = 1,
        .frequency = 2,
        .amplitude = 0x7f,
    };

    struct Effect testEffect = {
        .type = SINE,
        .duration = 0,
        .button_mask = 0x100,
        .direction = 90,
        .strength = 0x7f,
        .sample_rate = 100,
        .attack = 0x7f,
        .fade = 0x7f,
        .attack_time = 0,
        .fade_time = 0,
        .frequency = 2,
        .amplitude = 0x7f,
    };

    midi_define_effect(uart0, &lightSpringEffect); // effect 2
    midi_define_effect(uart0, &kickbackEffect); // effect 3
    midi_define_effect(uart0, &rumbleEffect); // effect 4
    midi_define_effect(uart0, &testEffect); // effect 5

    midi_play(uart0, 2);

    bool fire_old;
    bool btnA_old;
    bool btnB_old;
    bool btnC_old;
    bool btnD_old;

    // Main USB loop
    while (1)
    {
        tud_task(); // tinyusb device task
        hid_task();

        bool btnA = (joystickState.buttons & 0x0010) == 0;
        bool btnB = (joystickState.buttons & 0x0020) == 0;
        bool btnC = (joystickState.buttons & 0x0040) == 0;
        bool btnD = (joystickState.buttons & 0x0080) == 0;

        uint16_t new_value = 0;
        bool change = false;

        if (btnA && !btnA_old)
        {
            new_value = 1;
            change = true;
        }
        if (btnB && !btnB_old)
        {
            new_value = 3;
            change = true;
        }
        if (btnC && !btnC_old)
        {
            new_value = 5;
            change = true;
        }
        if (btnD && !btnD_old)
        {
            new_value = 1000;
            change = true;
        }

        if (change)
        {
            midi_modify(uart0, 5, MODIFY_SAMPLE_RATE, new_value);
        }

        btnA_old = btnA;
        btnB_old = btnB;
        btnC_old = btnC;
        btnD_old = btnD;
    }
}
