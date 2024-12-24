#include "cpufreq.h"
#include "debug.h"
#include <avr/io.h>

static const char debug_hex_lookup[16] = {
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    'A',
    'B',
    'C',
    'D',
    'E',
    'F'
};

void debug_init(void) {
    USART2.BAUD = (uint16_t) USART_BAUD_VALUE(115200);
    USART2.CTRLC |=
            USART_PMODE_DISABLED_gc |
            USART_CMODE_ASYNCHRONOUS_gc |
            USART_SBMODE_1BIT_gc |
            USART_CHSIZE_8BIT_gc;
    USART2.CTRLB |= USART_TXEN_bm | USART_RXEN_bm | USART_RXMODE_NORMAL_gc;
    PORTF.DIR |= PIN0_bm;
    PORTF.DIR &= ~PIN1_bm;
    PORTF.PIN1CTRL |= PORT_PULLUPEN_bm;
}

void debug_write_char(char c) {
    while (!(USART2.STATUS & USART_DREIF_bm)) {
        // wait
    }
    USART2.TXDATAL = c;
}

void debug_write_string(const char* s) {
    while (*s != '\0') {
        debug_write_char(*s);
        s++;
    }
}

void debug_write_bytes(const uint8_t* array, size_t n) {
    char buf[5];
    buf[0] = '0';
    buf[1] = 'x';
    buf[4] = '\0';
    for (size_t i = 0; i < n; i++) {
        buf[2] = debug_hex_lookup[array[i] / 16];
        buf[3] = debug_hex_lookup[array[i] % 16];
        debug_write_string(buf);
        debug_write_char(' ');
    }
}

uint8_t debug_has_input(void) {
    return !!(USART2.STATUS & USART_RXCIF_bm);
}

char debug_read(void) {
    while (!(USART2.STATUS & USART_RXCIF_bm)) {
        // wait
    }
    char result = USART2.RXDATAL;
    debug_write_char(result);
    debug_write_string("\r\n");
    return result;
}
