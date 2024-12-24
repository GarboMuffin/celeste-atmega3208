#include "frametimer.h"
#include "cpufreq.h"
#include "debug.h"
#include <avr/io.h>

// Approximately 30Hz timer

void frametimer_init(void) {
    // 20,000,000 Hz / 30 Hz / 256
    TCA0.SINGLE.PER = 41666;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc;
}

void frametimer_start(void) {
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

void frametimer_wait(void) {
    if (TCA0.SINGLE.INTFLAGS & TCA_SINGLE_OVF_bm) {
        debug_write_string("FRAME DEADLINE MISSED\r\n");
        TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
        return;
    }

    while (!(TCA0.SINGLE.INTFLAGS & TCA_SINGLE_OVF_bm)) {
        // wait
    }
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}
