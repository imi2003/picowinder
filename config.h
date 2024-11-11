#ifndef CONFIG_H
#define CONFIG_H

/*
Configure which pins on the Pico map to which GamePort pins.
These can be configured somewhat, but:
  * PIN_MIDI_TX must be 0, 12, or 16. (Other pins aren't supported for UART0 TX.)
  * PIN_D0, PIN_D1, and PIN_D2 must be consecutive.
*/
#define PIN_MIDI_TX 0
#define PIN_TRIGGER 2
#define PIN_CLK     3
#define PIN_D0      4
#define PIN_D1      5
#define PIN_D2      6

// Disable the Sidewinder's default auto-center effect.
#define DISABLE_AUTO_CENTER

// Add some example effects: a trigger kickback, and a gentler auto-center effect.
#define EXAMPLE_EFFECTS

// If this is defined, the joystick presents 16 buttons over USB, with the upper 8
// actuated by holding down the "shift" button on the base of the joystick.
// If this is not defined, the joystick presents 9 buttons, with the shift button
// acting independently of the others.
// #define FIRMWARE_SHIFT

#endif //CONFIG_H