#include "buttons.h"
#include "debug.h"
#include <avr/io.h>
#include <stdio.h>

#define JUMP_PORT PORTA
#define JUMP_PINCTRL PORTA.PIN3CTRL
#define JUMP_PIN_bm PIN3_bm

#define DASH_PORT PORTC
#define DASH_PINCTRL PORTC.PIN0CTRL
#define DASH_PIN_bm PIN0_bm

static uint8_t old_jump;
static uint8_t old_dash;

void buttons_init(void) {
    JUMP_PORT.DIR &= ~JUMP_PIN_bm;
    JUMP_PINCTRL |= PORT_PULLUPEN_bm;

    DASH_PORT.DIR &= ~DASH_PIN_bm;
    DASH_PINCTRL |= PORT_PULLUPEN_bm;
}

uint8_t buttons_jump(void) {
    uint8_t latest = (JUMP_PORT.IN & JUMP_PIN_bm) == 0;
    uint8_t just_pressed = !old_jump && latest;
    old_jump = latest;
    return just_pressed;
}

uint8_t buttons_dash(void) {
    uint8_t latest = (DASH_PORT.IN & DASH_PIN_bm) == 0;
    uint8_t just_pressed = !old_dash && latest;
    old_dash = latest;
    return just_pressed;
}

void buttons_test(void) {
    while (1) {
        char buf[100];
        sprintf(buf, "Jump: %d Dash: %d\r\n", buttons_jump(), buttons_dash());
        debug_write_string(buf);
    }
}
