#include "ffb_midi.h"

enum MidiEffectType effects_assigned[EFFECT_MEMORY_START + EFFECT_MEMORY_SIZE] = { 0 };

int ffb_midi_get_free_effect_id()
{
    for (int i = EFFECT_MEMORY_START; i < EFFECT_MEMORY_START + EFFECT_MEMORY_SIZE; i++)
    {
        if (effects_assigned[i] == MIDI_ET_NONE) { return i; }
    }

    return -1;
}

size_t ffb_midi_get_num_available_effects()
{
    size_t num_free = 0;

    for (int i = EFFECT_MEMORY_START; i < EFFECT_MEMORY_START + EFFECT_MEMORY_SIZE; i++)
    {
        if (effects_assigned[i] == MIDI_ET_NONE) { num_free++; }
    }

    return num_free;
}

bool last_add_succeeded;

bool ffb_midi_last_add_succeeded()
{
    return last_add_succeeded;
}

uint8_t last_assigned_effect_id;

uint8_t ffb_midi_last_assigned_effect_id()
{
    return last_assigned_effect_id;
}

#define MIDI_ALL_EFFECTS 0x7f

void ffb_midi_init(uart_inst_t *uart)
{
    const uint8_t start0[] = {
        0xc5, 0x01 // <ProgramChange> 0x01
    };

    const uint8_t start1[] = {
        0xf0, // SysEx begin
        0x00, 0x01, 0x0a, 0x01, // Effect header
        0x10, 0x05, 0x6b,
        0xf7  // SysEx end
    };

    uart_write_blocking(uart, start0, sizeof(start0));
    sleep_ms(20);
    uart_write_blocking(uart, start1, sizeof(start1));
    sleep_ms(57);

    // Set default values for each effect parameter, for all effects.
    // TODO: are these really necessary, given we always set all relevant values for each effect we define?
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_DURATION,        11250);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_BUTTON_MASK,     0x21bc); // TODO: ???
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_DIRECTION,       0x7e);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_GAIN,            0x04);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_OFFSET_X,        0x02);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_OFFSET_Y,        0x02);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, 0x58,                   0x3f00); // TODO: 0x58?
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_ATTACK_TIME,     0x3c);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_FADE_TIME,       0x3294);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_ATTACK_LEVEL,    0x35fe); // TODO: ???
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_SUSTAIN_LEVEL,   0x36);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_FADE_LEVEL,      0x28);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_FREQUENCY,       0x2666); // TODO: isn't this way too high?
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_AMPLITUDE,       0xfe);
    ffb_midi_modify(uart, MIDI_ALL_EFFECTS, MODIFY_DEVICE_GAIN,     0x7f);
}

void ffb_midi_set_autocenter(uart_inst_t *uart, bool enabled)
{
    const uint8_t autocenter_on[] = {
            0xc5, 0x01
    };

    const uint8_t autocenter_off[] = {
            0xc5, 0x06,
    };

    uart_write_blocking(uart, autocenter_on, sizeof(autocenter_on));

    if (!enabled)
    {
        sleep_ms(70);
        uart_write_blocking(uart, autocenter_off, sizeof(autocenter_off));
    }
}

/*
Since the MSB is reserved, we get 7 bits of real data per MIDI byte.
14-bit values must be split into high and low 7-bit chunks.
*/
static inline uint8_t lo7(uint16_t val) { return val & 0x7f; }
static inline uint8_t hi7(uint16_t val) { return (val >> 7) & 0x7f; }

int ffb_midi_define_effect(uart_inst_t *uart, struct Effect *effect)
{
    // Get an effect id (and make sure we have enough room)
    int effect_id = ffb_midi_get_free_effect_id();
    if (effect_id < 0)
    {
        last_add_succeeded = false;
        return effect_id;
    }

    // At a glance, 0x24 through 0x2f will start the effect immediately,
    // 0x20 through 0x23 will wait for it to be started,
    // and any upper nibble besides 0x2* will fail.
    uint8_t flags = effect->play_immediately ? 0x24 : 0x23;

    uint8_t effect_data[33] = {
        0xf0,                           // 0: SysEx start - effect data
        0x00, 0x01, 0x0a, 0x01,         // 1..4: Effect header
        flags,                          // 5: Effect flags?
        effect->type,                   // 6: enumerated effect type
        0x7f,                           // 7: Overwrite existing effect ID (0x7f to claim new effect)
        lo7(effect->duration),          // 8, 9: duration
        hi7(effect->duration),
        lo7(effect->button_mask),       // 10, 11: button mask
        hi7(effect->button_mask)
    };

    uint8_t next_index;

    switch (effect->type)
    {
        case MIDI_ET_CONSTANT:
        case MIDI_ET_SINE:
        case MIDI_ET_SQUARE:
        case MIDI_ET_RAMP:
        case MIDI_ET_TRIANGLE:
        case MIDI_ET_SAWTOOTHDOWN:
        case MIDI_ET_SAWTOOTHUP:

            effect_data[12] = lo7(effect->direction);   // 12, 13: direction
            effect_data[13] = hi7(effect->direction);
            effect_data[14] = effect->gain;             // 14: gain
            effect_data[15] = lo7(effect->sample_rate); // 15, 16: sample rate
            effect_data[16] = hi7(effect->sample_rate);
            effect_data[17] = 0x10;                     // 17, 18: truncate; 0x4e10 = 10000 = full waveform
            effect_data[18] = 0x4e;
            effect_data[19] = effect->attack_level;     // 19: Envelope Attack Start Level
            effect_data[20] = lo7(effect->attack_time); // 20, 21: Envelope Attack Time
            effect_data[21] = hi7(effect->attack_time);
            effect_data[22] = effect->sustain_level;    // 22: Envelope Sustain Level
            effect_data[23] = lo7(effect->fade_time);   // 23, 24: Envelope Fade Time
            effect_data[24] = hi7(effect->fade_time);
            effect_data[25] = effect->fade_level;       // 25: Envelope Fade End Level
            effect_data[26] = lo7(effect->frequency);   // 26, 27: Frequency
            effect_data[27] = hi7(effect->frequency);
            effect_data[28] = lo7(effect->amplitude);   // 28, 29: Amplitude
            effect_data[29] = hi7(effect->amplitude);
            effect_data[30] = 0x01;                     // 30, 31: ???
            effect_data[31] = 0x01;
            next_index = 32;

            break;

        case MIDI_ET_SPRING:
        case MIDI_ET_DAMPER:
        case MIDI_ET_INERTIA:

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

        case MIDI_ET_FRICTION:

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

    last_add_succeeded = true;
    last_assigned_effect_id = effect_id;
    effects_assigned[effect_id] = effect->type;
    return effect_id;
}

void ffb_midi_erase(uart_inst_t *uart, int effect_id)
{
    if (effect_id < 0) { return; }

    uint8_t msg[3] = { 0xb5, 0x10, effect_id & 0x7f };
    uart_write_blocking(uart, msg, sizeof(msg));

    effects_assigned[effect_id] = MIDI_ET_NONE;
}

void ffb_midi_play(uart_inst_t *uart, int effect_id)
{
    if (effect_id < 0) { return; }

    // Note: 0x00 also works. TODO: is 0x00 different (play solo?)
    uint8_t msg[3] = { 0xb5, 0x20, effect_id & 0x7f };
    uart_write_blocking(uart, msg, sizeof(msg));
}

void ffb_midi_pause(uart_inst_t *uart, int effect_id)
{
    if (effect_id < 0) { return; }

    uint8_t msg[3] = { 0xb5, 0x30, effect_id & 0x7f };
    uart_write_blocking(uart, msg, sizeof(msg));
}

void ffb_midi_modify(uart_inst_t *uart, int effect_id, uint8_t param, uint16_t value)
{
    if (effect_id < 0) { return; }

    uint8_t msg[6] = {
        0xb5, param, effect_id & 0x7f, 0xa5, lo7(value), hi7(value) };
    uart_write_blocking(uart, msg, sizeof(msg));
}