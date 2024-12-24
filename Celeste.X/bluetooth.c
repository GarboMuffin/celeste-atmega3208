#include "bluetooth.h"
#include "cpufreq.h"
#include "debug.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>

#define RX_BUF_SIZE 256
#define TX_BUF_SIZE 256
#define BLE_RADIO_PROMPT "CMD> "
#define BLE_LEAVE_COMMAND_MODE "END\r\n"

// 1 if we are the host (Madeline)
static uint8_t is_host;

// 1 if we are connected to a peer and the transparent UART connection was set up successfully
static uint8_t uart_connected;

// Receiving buffer
static uint8_t rx_buffer[RX_BUF_SIZE];
static size_t rx_buffer_pos; // Next empty index in rx buffer

// Buffer for most recently received complete message
static uint8_t pending_message[RX_BUF_SIZE];
static uint8_t has_pending_message;

// 1 if message has been received ok during this connection
static uint8_t has_received_ok;

// Size of the messages in our protocol. Controlled by celeste.c.
static uint8_t message_size;

// How many messages we are allowed to send
// Resets when we receive a message from the other end. This way we never
// flood the BLE chip with too much stuff that will never be received (which seems
// to break it in weird ways)
#define REFRESH_MESSAGE_ALLOWANCE 3
static uint8_t message_allowance;

// Sending buffer
static uint8_t tx_buffer[TX_BUF_SIZE];
static size_t tx_buffer_pos;   // Next index to send
static size_t tx_buffer_size;  // Total # of bytes stored in tx buffer
static uint8_t tx_buffer_busy; // 1 if being sent right now

// Magic bytes for our protocol
// They form CELESTE in hex
static const uint8_t magic_header[] = {
    0xCE,
    0x1E,
};
static const uint8_t magic_footer[] = {
    0x57,
    0xE0
};

#define EMERGENCY_RESET_ALLOW_AFTER_FRAMES 60
#define MIN_FRAMES_BETWEEN_TX 1
static uint16_t frames_since_last_tx;

#define RECONENCT_AFTER_FRAMES 150
static uint16_t frames_since_connection_attempt;

void bluetooth_init(void) {
    // From AVR-BLE datasheet
    USART0.BAUD = (uint16_t) USART_BAUD_VALUE(9600);
    USART0.CTRLC |=
            USART_PMODE_DISABLED_gc |
            USART_CMODE_ASYNCHRONOUS_gc |
            USART_SBMODE_1BIT_gc |
            USART_CHSIZE_8BIT_gc;

    // Enable sending, receiving, normal mode
    USART0.CTRLB |= USART_TXEN_bm | USART_RXEN_bm | USART_RXMODE_NORMAL_gc;

    // Enable interrupts fur use after init
    USART0.CTRLA |= USART_RXCIE_bm;

    // Set up ports used for UART communication with BLE
    PORTA.DIR |= PIN0_bm;
    PORTA.DIR &= ~PIN1_bm;

    // Put BLE Radio in "Application Mode" by driving F3 high
    PORTF.DIRSET = PIN3_bm;
    PORTF.OUTSET = PIN3_bm;

    // Reset BLE Module - pull PD3 low, then back high after a delay
    PORTD.DIRSET = PIN3_bm | PIN2_bm;
    PORTD.OUTCLR = PIN3_bm;
    _delay_ms(10); // Leave reset signal pulled low
    PORTD.OUTSET = PIN3_bm;

    // The AVR-BLE hardware guide is wrong. Labels this as D3
    // Tell BLE module to expect data - set D2 low
    PORTD.OUTCLR = PIN2_bm;
    _delay_ms(200); // Give time for RN4870 to boot up

    // Command mode
    bluetooth_write_string("$$$");
    bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);

    // Reset
    bluetooth_write_string("PZ\r\n");
    bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);

    // Get device info
    // buf will contain:
    // BTA=ABCDEFABCDEF\r\n ...
    bluetooth_write_string("D\r\n");
    bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);

    // buf will be something like:
    /*
    BTA=0491629B4E7B\r\n
    Name=Madeline_4E7B\r\n
    Connected=no\r\n
    Authen=2\r\n
    Features=0000\r\n
    Services=40\r\n
    CMD> \0
    */
    /*
    BTA=0491629A1BC0\r\n
    Name=Theo_1BC0\r\n
    Connected=no\r\n
    Authen=2\r\n
    Features=0000\r\n
    Services=40\r\n
    CMD> \0
    */

    int is_madeline = strncmp((char*) (rx_buffer + 4), "0491629B4E7B", 12) == 0;
    if (is_madeline) {
        is_host = 1;
        bluetooth_write_string("S-,Madeline\r\n");
        bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);
    } else {
        is_host = 0;

        bluetooth_write_string("S-,Theo\r\n");
        bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);

        // Start trying to connect to Madeline.
        bluetooth_write_string("C,0,0491629B4E7B\r\n");
    }

    bluetooth_write_string("---\r\n");
    bluetooth_read_until(rx_buffer, BLE_LEAVE_COMMAND_MODE);
}

void bluetooth_reconnect(void) {
    // For now we do this using polling, I don't like this either
    // But it works and that's the most important thing
    // Need to use atomic as otherwise we'll keep activating our interrupts and
    // be unable to actually read or write anything.
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        frames_since_connection_attempt = 0;

        // Switch to command mode. We need a delay before we can do this. But by
        // the time this runs we can be reasonably confident that nothing has been
        // sent for a long time.
        bluetooth_write_string("$$$");
        bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);

        // Cancel any ongoing connection attempt.
        // May return an error, that's fine.
        bluetooth_write_string("Z\r\n");
        bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);

        // Disconnect any existing connection
        // May return an error, that's fine.
        bluetooth_write_string("K,1\r\n");
        bluetooth_read_until(rx_buffer, BLE_RADIO_PROMPT);

        // Start connecting to Madeline again
        bluetooth_write_string("C,0,0491629B4E7B\r\n");

        // Return to data mode.
        bluetooth_write_string("---\r\n");
        bluetooth_read_until(rx_buffer, BLE_LEAVE_COMMAND_MODE);
    }
}

void bluetooth_reset_state(void) {
    USART0.CTRLA &= ~USART_DREIE_bm;
    uart_connected = 0;
    rx_buffer_pos = 0;
    tx_buffer_pos = 0;
    tx_buffer_size = 0;
    tx_buffer_busy = 0;
    has_pending_message = 0;
    has_received_ok = 0;
    message_allowance = 0;
    frames_since_last_tx = 0;
    frames_since_connection_attempt = 0;
}

void bluetooth_write_char(char c) {
    while (!(USART0.STATUS & USART_DREIF_bm)) {
        // wait
    }
    USART0.TXDATAL = c;
}

void bluetooth_write_string(const char* cmd) {
    debug_write_string(">>> ");
    debug_write_string(cmd);
    debug_write_string("\r\n");

    while (*cmd != '\0') {
        bluetooth_write_char(*cmd);
        cmd++;
    }
}

char bluetooth_read_char(void) {
    while (!(USART0.STATUS & USART_RXCIF_bm)) {
        // wait
    }
    return USART0.RXDATAL;
}

void bluetooth_read_until(uint8_t* dest, const char* end_str) {
    // Zero out dest memory so we always have null terminator at end
    memset(dest, 0, RX_BUF_SIZE);
    uint8_t end_len = strlen(end_str);
    uint8_t bytes_read = 0;
    while (bytes_read < end_len || strcmp((char*) dest + bytes_read - end_len, end_str) != 0) {
        char c = bluetooth_read_char();
        dest[bytes_read] = c;
        bytes_read++;
    }

    debug_write_string("From BLE: ");
    debug_write_string((char*) dest);
    debug_write_string("\r\n");
}

ISR(USART0_RXC_vect) {
    char c = USART0.RXDATAL;
    rx_buffer[rx_buffer_pos] = c;
    rx_buffer_pos++;
    uint8_t* rx_head = &rx_buffer[rx_buffer_pos];

//    debug_write_char(c);

    if (c == '%') {
        // % is ending character of messages from BLE chip itself eg.:
        // %CONNECT,0,0491629B4E7B%
        // %STREAM_OPEN%
        // %DISCONNECT%

        if (memcmp(rx_head - 24, "%CONNECT,", strlen("%CONNECT,")) == 0) {
            // Connected, but UART mode not yet ready
            debug_write_string("Connected but no UART yet\r\n");
        } else if (memcmp(rx_head - strlen("%STREAM_OPEN%"), "%STREAM_OPEN%", strlen("%STREAM_OPEN%")) == 0) {
            // Connection successful
            debug_write_string("Stream open\r\n");
            uart_connected = 1;
            message_allowance = is_host ? REFRESH_MESSAGE_ALLOWANCE : 0;
        } else if (memcmp(rx_head - strlen("%DISCONNECT%"), "%DISCONNECT%", strlen("%DISCONNECT%")) == 0) {
            // Lost connection
            debug_write_string("Disconnected\r\n");
            bluetooth_reset_state();
        }
    } else if (c == magic_footer[sizeof(magic_footer) - 1]) {
        // This could be the end of our footer, let's check
        if (memcmp(rx_head - sizeof(magic_footer), magic_footer, sizeof(magic_footer)) == 0) {
            // See if the header is in the right spot
            if (memcmp(rx_head - sizeof(magic_footer) - message_size - sizeof(magic_header), magic_header, sizeof(magic_header)) == 0) {
                // This looks like a valid message from the other board
                // Copy message to the last received message buffer
                memcpy(pending_message, rx_head - sizeof(magic_footer) - message_size, message_size);
                has_pending_message = 1;
                has_received_ok = 1;
                rx_buffer_pos = 0;

                // Receiving a message resets our allowance
                message_allowance = REFRESH_MESSAGE_ALLOWANCE;
            }
        }
    }

    // Wrap around if we never found anything
    // Not ideal -- but this shouldn't happen if above worked correctly
    // Just trying to prevent catastrophic overflowing buffer
    if (rx_buffer_pos >= RX_BUF_SIZE) {
        rx_buffer_pos = 0;
    }
}

ISR(USART0_DRE_vect) {
    USART0.TXDATAL = tx_buffer[tx_buffer_pos];
    tx_buffer_pos++;
    if (tx_buffer_pos >= tx_buffer_size) {
        // Sent entire message, stop further interrupts.
        tx_buffer_busy = 0;
        USART0.CTRLA &= ~USART_DREIE_bm;
    }
}

uint8_t bluetooth_is_exchanging_ok(void) {
    return has_received_ok;
}

void bluetooth_set_message_size(uint8_t new_size) {
    message_size = new_size;
}

uint8_t bluetooth_has_pending_message(void) {
    return has_pending_message;
}

void bluetooth_copy_pending_message(void* dest) {
    // TODO: disable interrupts while we do this...
    memcpy(dest, pending_message, message_size);
    has_pending_message = 0;
}

uint8_t bluetooth_can_send_message(void) {
    if (!uart_connected || frames_since_last_tx <= MIN_FRAMES_BETWEEN_TX) {
        return 0;
    }
    if (message_allowance == 0) {
        if (is_host && frames_since_last_tx >= EMERGENCY_RESET_ALLOW_AFTER_FRAMES) {
            message_allowance = REFRESH_MESSAGE_ALLOWANCE;
        }
    }
    return message_allowance > 0;
}

void bluetooth_start_sending(void) {
//    debug_write_string("Sending message: ");
//    debug_write_bytes(tx_buffer, tx_buffer_size);
//    debug_write_string("\r\n");

    tx_buffer_pos = 0;
    tx_buffer_busy = 1;
    USART0.CTRLA |= USART_DREIE_bm;
}

void bluetooth_send_raw(const void* src, uint8_t size) {
    memcpy(tx_buffer, src, size);
    tx_buffer_size = size;
    bluetooth_start_sending();
}

void bluetooth_send_message(const void* src) {
    // Header
    memcpy(tx_buffer, magic_header, sizeof(magic_header));
    tx_buffer_size = sizeof(magic_header);

    // The payload
    memcpy(tx_buffer + tx_buffer_size, src, message_size);
    tx_buffer_size += message_size;

    // Footer
    memcpy(tx_buffer + tx_buffer_size, magic_footer, sizeof(magic_footer));
    tx_buffer_size += sizeof(magic_footer);

    frames_since_last_tx = 0;
    message_allowance--;
    bluetooth_start_sending();
}

void bluetooth_tick() {
    if (uart_connected) {
        if (frames_since_last_tx < EMERGENCY_RESET_ALLOW_AFTER_FRAMES) {
            frames_since_last_tx++;
        }
    } else {
        if (!is_host) {
            frames_since_connection_attempt++;
            if (frames_since_connection_attempt == RECONENCT_AFTER_FRAMES) {
                bluetooth_reconnect();
            }
        }
    }
}

void bluetooth_debug_poll(void) {
    while (1) {
        if (debug_has_input()) {
            USART0.TXDATAL = debug_read();
        }

        if (USART0.STATUS & USART_RXCIF_bm) {
            debug_write_string("Receive: ");
            debug_write_char(USART0.RXDATAL);
            debug_write_string("\r\n");
        }
    }
}

void bluetooth_debug_interrupt(void) {
    sei();
//    char buf[] = {'A', 'B', 'C', '?'};
    while (1) {
//        if (debug_has_input()) {
//            buf[3] = debug_read();
//            bluetooth_send_message(buf, sizeof(buf));
//        }
   }
}
