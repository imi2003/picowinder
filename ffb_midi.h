#ifndef FFB_MIDI_H
#define FFB_MIDI_H

#include "pico/stdlib.h"
#include "hardware/uart.h"


enum MidiEffectType
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


/*
TODO investigate: fade time property isn't behaving as expected.
It appears that the entire effect duration is currently being treated
as the fade time, so neither the initial value nor the modified value
has any effect. It's possible that this behavior is enabled by some other setting.
*/

struct Effect
{
    // Common to all effects
    enum MidiEffectType type;
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


void ffb_midi_init(uart_inst_t *uart);
void ffb_midi_set_autocenter(uart_inst_t *uart, bool enabled);
void ffb_midi_define_effect(uart_inst_t *uart, struct Effect *effect);
void ffb_midi_play(uart_inst_t *uart, uint8_t effect_id);
void ffb_midi_stop(uart_inst_t *uart, uint8_t effect_id);
void ffb_midi_modify(uart_inst_t *uart, uint8_t effect_id, uint8_t param, uint16_t value);


#endif //FFB_MIDI_H