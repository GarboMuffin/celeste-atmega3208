#include "display.h"
#include "cpufreq.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

// My display implementation is heavily based on Adafruit's reference SPI TFT code
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_SPITFT.cpp

// ST7735 Data sheet section 10
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A
#define ST7735_FRMCTR1 0xB1
#define ST7735_INVCTR  0xB4
#define ST7735_PWCTL1  0xC0
#define ST7735_PWCTL2  0xC1
#define ST7735_PWCTL3  0xC2
#define ST7735_PWCTL4  0xC3
#define ST7735_PWCTL5  0xC4
#define ST7735_VMCTR1  0xC5

void display_init(void) {
    PORTA.DIR |= PIN4_bm;  // MOSI
    PORTA.DIR &= ~PIN5_bm; // MISO
    PORTA.DIR |= PIN6_bm;  // SCK
    PORTA.DIR |= PIN7_bm;  // CS for display
    PORTA.DIR |= PIN2_bm;  // D/C - Low for command, high for data

    // Set CS low to activate display. We never use any other SPI devices so we
    // can just leave it like this forever.
    PORTA.OUTCLR = PIN7_bm;

    // MSB first so don't enable DORD
    // Display is much faster than us, so run as fast as possible
    SPI0.CTRLA = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_CLK2X_bm | SPI_PRESC_DIV4_gc;

    // Reset display registers etc. (based on Adafruit library)
    display_write_command(ST7735_SWRESET, 0, 0);
    _delay_ms(150);

    // Display is in sleep mode by default; leave it (based on Adafruit library)
    display_write_command(ST7735_SLPOUT, 0, 0);
    _delay_ms(500);

    // Here Adafruit reference library sets up FRMCTR1 but the defaults seem to
    // work fine for us so not trying it.

    // No inversion (based on Adafruit library)
    uint8_t invctr[] = {0x07};
    display_write_command(ST7735_INVCTR, 1, invctr);

    // Configure various power settings, matching Adafruit library
    // Too scared to touch these
    uint8_t pwctr1[] = {0xA2, 0x02, 0x84};
    display_write_command(ST7735_PWCTL1, 3, pwctr1);
    uint8_t pwctr2[] = {0xC5};
    display_write_command(ST7735_PWCTL2, 1, pwctr2);
    uint8_t pwctr3[] = {0x0A, 0x00};
    display_write_command(ST7735_PWCTL3, 2, pwctr3);
    uint8_t pwctr4[] = {0x8A, 0x2A};
    display_write_command(ST7735_PWCTL4, 2, pwctr4);
    uint8_t pwctr5[] = {0x8A, 0xEE};
    display_write_command(ST7735_PWCTL5, 2, pwctr5);
    uint8_t vmctr1[] = {0x0E};
    display_write_command(ST7735_VMCTR1, 1, vmctr1);

    // Disable invert (based on Adafruit library)
    display_write_command(ST7735_INVOFF, 0, 0);
    
    // Adafruit library sends MADCTL here, but by default the coordinate system
    // is already set up how we want.

    // Enable 16 bit pixel mode (RGB 5-6-5) (based on Adafruit library)
    uint8_t colmod[] = {0x05};
    display_write_command(ST7735_COLMOD, 1, colmod);
    
    // Reference Adafruit library has some big scary gamma arrays here.
    // The colors seem fine already, so no need to try that.

    // Enable normal display mode (based on Adafruit library)
    display_write_command(ST7735_NORON, 0, 0);
    _delay_ms(10);

    // Turn on the display (based on Adafruit library)
    display_write_command(ST7735_DISPON, 0, 0);
    _delay_ms(100);
}

void display_write_command(uint8_t command, uint8_t num_data, uint8_t* data) {
    PORTA.OUTCLR = PIN2_bm; // D/C low

    SPI0.DATA = command;
    while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
        // Wait
    }

    PORTA.OUTSET = PIN2_bm; // D/C high
    for (uint8_t i = 0; i < num_data; i++) {
        SPI0.DATA = data[i];

        while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
            // Wait
        }
    }
}

// Maps the 16 possible PICO-8 pixel colors to the color we actually send to
// the display. Based on https://pico-8.fandom.com/wiki/Palette.
// Generated with help of palette-tool.py
// The meaning of each of the 16 bits in the color is:
//
// upper   lower
// 1234567812345678
//            ~~~~~ red (5 bits)
//      ~~~~~~      green (6 bits)
// ~~~~~            blue (5 bits)
static const uint16_t palette[] = {
    0x0000,
    0x5143,
    0x512f,
    0x5420,
    0x3295,
    0x4aab,
    0xc618,
    0xef9f,
    0x481f,
    0x051f,
    0x277f,
    0x3720,
    0xfd65,
    0x9bb0,
    0xabbf,
    0xae7f
};

void display_set_draw_window(int x, int y, int w, int h) {
    // Magic offsets to make 0, 0 actually be in the corner and not offscreen or
    // in the middle. Based on the Adafruit library but also determined experimentally.
    int x_start = x + 2;
    int y_start = y + 1;

    // Subtract 1 because end is inclusive.
    int x_end = x_start + w - 1;
    int y_end = y_start + h - 1;

    // Update column range
    // We know all of our values will fit inside 8 bits so we can set the upper
    // portions to zero all the time.
    uint8_t data[4];
    data[0] = 0;
    data[1] = x_start;
    data[2] = 0;
    data[3] = x_end;
    display_write_command(ST7735_CASET, 4, data);

    // Update row range
    data[0] = 0;
    data[1] = y_start;
    data[2] = 0;
    data[3] = y_end;
    display_write_command(ST7735_RASET, 4, data);

    // Enter write mode
    display_write_command(ST7735_RAMWR, 0, 0);
}

void display_draw_packed_sprite(int x, int y, const uint8_t* progmem_sprite) {
    display_set_draw_window(x, y, 8, 8);

    const uint8_t* sprite_data_end = progmem_sprite + 32;
    while (progmem_sprite < sprite_data_end) {
        uint8_t packed = pgm_read_byte(progmem_sprite);
        progmem_sprite++;

        uint16_t color = palette[packed >> 4];
        uint8_t upper = color >> 8;

        while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
            // Wait
        }
        SPI0.DATA = upper;

        while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
            // Wait
        }
        SPI0.DATA = color;

        color = palette[packed & 0xF];
        upper = color >> 8;
        while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
            // Wait
        }
        SPI0.DATA = upper;

        while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
            // Wait
        }
        SPI0.DATA = color;
    }

    while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
        // Wait
    }
}

void display_draw_pixels(int x, int y, int w, int h, const uint8_t* pixels) {
    display_set_draw_window(x, y, w, h);

    const uint8_t* pixels_end = pixels + w * h;
    while (pixels < pixels_end) {
        uint16_t color = palette[*pixels];
        uint8_t upper = color >> 8;

        while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
            // Wait
        }
        SPI0.DATA = upper;

        while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
            // Wait
        }
        SPI0.DATA = color;

        pixels++;
    }

    while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
        // Wait
    }
}

void display_test() {
//    uint8_t sprite = 0;
    while (1) {
//        for (int x = 0; x < 128; x += 8) {
//            for (int y = 0; y < 128; y += 8) {
//                display_draw_sprite(x, y, sprite);
//
//                sprite++;
//                if (sprite == 127) {
//                    sprite = 0;
//                }
//            }
//        }
    }
}
