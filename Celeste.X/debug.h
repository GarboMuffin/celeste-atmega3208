#include <stdint.h>

#ifndef DEBUG_H
#define	DEBUG_H

// Initialize debugging support
void debug_init(void);

// Write character to debugger's serial port
void debug_write_char(char);

// Write string to debugger's serial port
void debug_write_string(const char*);

// Write byte array to debugger's serial port in format friendly for humans
void debug_write_bytes(const uint8_t* array, size_t n);

// Returns 1 if there is anything to read
uint8_t debug_has_input(void);

// Use polling to read character from debugger's serial port
// Read character automatically echoed
char debug_read(void);

#endif
