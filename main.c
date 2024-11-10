#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "tusb.h"

#include "ffb_handshake.pio.h"
#include "read_joystick.pio.h"

#include "ffb_midi.h"


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
    ffb_midi_init(uart0);
    ffb_midi_set_autocenter(uart0, false);

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

    struct Effect wheelEmuSpringEffect = {
        .play_immediately = false,
        .type = MIDI_ET_SPRING,
        .duration = 0,
        .button_mask = 0,
        .strength_x = 0,
        .strength_y = 0x7f,
        .offset_x = 0,
        .offset_y = 0,
    };

    struct Effect lightSpringEffect = {
        .play_immediately = false,
        .type = MIDI_ET_SPRING,
        .duration = 0,
        .button_mask = 0,
        .strength_x = 0x30,
        .strength_y = 0x30,
        .offset_x = 0,
        .offset_y = 0,
    };

    struct Effect frictionEffect = {
        .play_immediately = false,
        .type = MIDI_ET_FRICTION,
        .duration = 0,
        .button_mask = 0,
        .strength_x = 0,
        .strength_y = 0x7f,
    };

    struct Effect kickbackEffect = {
        .play_immediately = false,
        .type = MIDI_ET_CONSTANT,
        .duration = 0,
        .button_mask = 0x00,
        .direction = 0,
        .gain = 0x7f,
        .sample_rate = 100,
        .attack_level = 0x7f,
        .sustain_level = 0x20,
        .fade_level = 0x00,
        .attack_time = 400,
        .fade_time = 0,
        .frequency = 1,
        .amplitude = 0x7f,
    };

    int effect_id_spring = ffb_midi_define_effect(uart0, &lightSpringEffect);
    //ffb_midi_play(uart0, effect_id_spring);

    int effect_id_kickback = ffb_midi_define_effect(uart0, &kickbackEffect);

    bool fire_old;

    // Main USB loop
    while (1)
    {
        tud_task(); // tinyusb device task
        hid_task();

        bool fire = (joystickState.buttons & 0x0001) == 0;
        if (fire && !fire_old)
        {
            ffb_midi_play(uart0, effect_id_kickback);
        }
        else if (!fire && fire_old)
        {
            ffb_midi_pause(uart0, effect_id_kickback);
        }
        fire_old = fire;
    }
}
