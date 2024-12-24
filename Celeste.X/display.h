#include <stdint.h>

#ifndef DISPLAY_H
#define	DISPLAY_H

// PICO-8 palette color names
#define PALETTE_TRANSPARENT 0
#define PALETTE_DARK_BLUE   1
#define PALETTE_MAROON      2
#define PALETTE_DARK_GREEN  3
#define PALETTE_BROWN       4
#define PALETTE_DARK_GRAY   5
#define PALETTE_LIGHT_GRAY  6
#define PALETTE_WHITE       7
#define PALETTE_RED         8
#define PALETTE_ORANGE      9
#define PALETTE_YELLOW      10
#define PALETTE_LIGHT_GREEN 11
#define PALETTE_BLUE        12
#define PALETTE_VIOLET      13
#define PALETTE_PINK        14
#define PALETTE_TAN         15

// Initialize the display
void display_init(void);

// Send a command to the display
void display_write_command(uint8_t command, uint8_t num_data, uint8_t* data);

// Send command to display to prepare sending pixels
void display_set_draw_window(int x, int y, int w, int h);

// Draw a sprite on display using default palette from sprite data stored in progmem
// The data is packed pixels (ie. 2 pixels per byte)
void display_draw_packed_sprite(int x, int y, const uint8_t* progmem_sprite);

// Draw pixel region at given spot.
// Pixels stored in RAM, 1 pixel per byte (so yes half the space is wasted)
void display_draw_pixels(int x, int y, int w, int h, const uint8_t* pixels);

// Display test mode
void display_test(void);

#endif	/* DISPLAY_H */
