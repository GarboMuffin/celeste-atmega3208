#include "cpufreq.h"
#include "main.h"
#include "debug.h"
#include "joystick.h"
#include "bluetooth.h"
#include "buttons.h"
#include "display.h"
#include "celeste.h"
#include "frametimer.h"
#include <avr/interrupt.h>

int main(void) {
    // Initialize modules
    // CPU frequency should be first so that eg. baud rate calculations are correct
    cpufreq_init();
    // Debug should be second so we can debug the rest of the modules
    debug_init();
    joystick_init();
    buttons_init();
    display_init();
    frametimer_init();
    celeste_init();
    bluetooth_init();

    sei();

    // Start the game.
    frametimer_start();
    while (1) {
        celeste_tick();
        bluetooth_tick();
        frametimer_wait();
    }
}
