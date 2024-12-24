#include "joystick.h"
#include "debug.h"
#include <avr/io.h>
#include <stdio.h>

#define HORIZONTAL_PORT PORTD
#define HORIZONTAL_PINCTRL PORTD.PIN7CTRL
#define HORIZONTAL_MUXPOS ADC_MUXPOS_AIN7_gc

#define VERTICAL_PORT PORTD
#define VERTICAL_PINCTRL PORTD.PIN1CTRL
#define VERTICAL_MUXPOS ADC_MUXPOS_AIN1_gc

void joystick_init(void) {
    HORIZONTAL_PINCTRL &= ~PORT_ISC_gm;
    HORIZONTAL_PINCTRL &= ~PORT_PULLUPEN_bm;
    HORIZONTAL_PINCTRL |= PORT_ISC_INPUT_DISABLE_gc;

    VERTICAL_PINCTRL &= ~PORT_ISC_gm;
    VERTICAL_PINCTRL &= ~PORT_PULLUPEN_bm;
    VERTICAL_PINCTRL |= PORT_ISC_INPUT_DISABLE_gc;

    // Speed is more important than precision for how we use the ADC.
    ADC0.CTRLA |= ADC_ENABLE_bm | ADC_RESSEL_8BIT_gc;
    // Use 3.3V as reference.
    // Prescale was determined experimentally by increasing until it worked
    ADC0.CTRLC |= ADC_REFSEL_VDDREF_gc | ADC_PRESC_DIV8_gc;
}

void joystick_start_horizontal(void) {
    ADC0.MUXPOS = HORIZONTAL_MUXPOS;
    ADC0.COMMAND = ADC_STCONV_bm;
}

void joystick_start_vertical(void) {
    ADC0.MUXPOS = VERTICAL_MUXPOS;
    ADC0.COMMAND = ADC_STCONV_bm;
}

uint8_t joystick_finish_reading(void) {
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm)) {
        // Wait
    }
    return ADC0.RESL;
}

void joystick_test() {
    while (1) {
        joystick_start_horizontal();
        uint8_t h = joystick_finish_reading();

        joystick_start_vertical();
        uint8_t v = joystick_finish_reading();

        char buf[100];
        sprintf(buf, "Horizontal: %d Vertical: %d\r\n", h, v);
        debug_write_string(buf);
    }
}
