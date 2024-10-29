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
    uint16_t throttle   :  7;
    uint16_t twist      :  6;
    uint8_t  hat        :  4;
} joystickState;

struct report
{
    uint16_t buttons;
} report;

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

        tud_hid_n_report(0x00, 0x01, &report, sizeof(report));
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

    while (1)
    {
        sleep_ms(100);
        printf("x %04d, y %04d, throttle %03d, twist %03d, hat %01d, buttons %03x\n",
            joystickState.x,
            joystickState.y,
            joystickState.throttle,
            joystickState.twist,
            joystickState.hat,
            joystickState.buttons
            );

        tud_task(); // tinyusb device task
        hid_task();
    }
}
