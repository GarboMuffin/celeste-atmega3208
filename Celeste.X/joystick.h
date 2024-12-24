#include <stdint.h>

#ifndef JOYSTICK_H
#define	JOYSTICK_H

// Initialize joysticks
void joystick_init(void);

// Begin a read of the horizontal joystick
// Get result with joystick_finish_reading (allows doing other stuff in the mean time)
void joystick_start_horizontal(void);

// Begin a read of the vertical joystick
// Get result with joystick_finish_reading (allows doing other stuff in the mean time)
void joystick_start_vertical(void);

// Returns the result of the most recent joystick_start_* call
// Uses polling
uint8_t joystick_finish_reading(void);

// Joystick testing mode
void joystick_test(void);

#endif

