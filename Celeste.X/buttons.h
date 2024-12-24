#include <stdint.h>

#ifndef BUTTONS_H
#define	BUTTONS_H

// Initialize buttons
void buttons_init(void);

// Returns 1 if jump button was just pressed
uint8_t buttons_jump(void);

// Returns 1 if dash button was just pressed
uint8_t buttons_dash(void);

// Button testing mode
void buttons_test(void);

#endif
