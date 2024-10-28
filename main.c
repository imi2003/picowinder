#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "ffb_handshake.pio.h"

#define PIN_MIDI_TX 0
#define PIN_TRIGGER 2
#define PIN_CLK     3
#define PIN_D0      4
#define PIN_D1      5
#define PIN_D2      6


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


int main()
{
    // Hardware UART setup.
    // The default is 8 data bits, no parity bit, and 1 stop bit.
    uart_init(uart0, 31250);
    gpio_set_function(PIN_MIDI_TX, UART_FUNCSEL_NUM(uart0, PIN_MIDI_TX));

    // PIO setup
    PIO pio = pio0;
    uint sm = 0;

    // PIO program setup
    uint offset = pio_add_program(pio, &ffb_handshake_program);
    uint freq = 100000;
    ffb_handshake_program_init(pio, sm, offset, freq, PIN_TRIGGER);

    // Populate the state machine with our pulses/delays.
    // Delays (ms):             7     35    15    78     4    59
    uint delays[7] = { 1000,   70,   350,  150,  780,   40,  590   };
    // Pulses (count):       1     4     3     2     2     3     2
    uint pulses[7] = {       1,    4,    3,    2,    2,    3,    2 };
    for (int i = 0; i < 7; i++)
    {
        uint32_t word = (pulses[i]-1) << 16 | (delays[i] - 1);
        pio_sm_put(pio, sm, word);
    }

    // Activate
    pio_sm_set_enabled(pio, sm, true);

    // Wait for the state machine to finish enabling FFB.
    // We could do this via an IRQ, but we know how long it takes.
    sleep_ms(250);

    pio_sm_set_enabled(pio, sm, false);

    midi_start(uart0);
    midi_autocenter(uart0, false);
}
