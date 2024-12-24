#include <avr/io.h>
#include "cpufreq.h"

void cpufreq_init(void) {
    // Disable prescaler so we get the full 20 MHz that the CPU can do.
    // MCLKCTRLB is a protected register so need to use CCP mode to write to it.
    // Once set, we have 4 CPU cycles to actually set it, so we have to compute
    // the new value ahead of time.
    uint8_t new_value = CLKCTRL.MCLKCTRLB & ~CLKCTRL_PEN_bm;
    CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKCTRLB = new_value;
}
