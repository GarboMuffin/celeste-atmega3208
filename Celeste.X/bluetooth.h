#include <stdint.h>

#ifndef BLUETOOTH_H
#define	BLUETOOTH_H

// Initialize the bluetooth module
void bluetooth_init(void);

// Reset all communication state
void bluetooth_reset_state(void);

// Attempt to connect to BLE
void bluetooth_reconnect(void);

// Write a character to the BLE peripheral using polling
void bluetooth_write_char(char c);

// Write a string to the BLE peripheral using polling
void bluetooth_write_string(const char* cmd);

// Read a character from BLE peripheral using polling
char bluetooth_read_char(void);

// Read from BLE peripheral until stop message seen using polling
void bluetooth_read_until(uint8_t* dest, const char* end_str);

void bluetooth_set_message_size(uint8_t size);

// Returns if the connection is connected and at least 1 message has been
// received and processed during this connection
uint8_t bluetooth_is_exchanging_ok(void);

// Returns 1 if a message has been fully received from the other board and is
// ready to be received
uint8_t bluetooth_has_pending_message(void);

// Copy most recent message into another place
void bluetooth_copy_pending_message(void* dest);

// Returns 1 if a message can be sent
uint8_t bluetooth_can_send_message(void);

// Starts loop of interrupts to send message
void bluetooth_start_sending(void);

// Start sending a message to the other board using our integrity and flow control
// protocols. It is sent in the background with interrupts. Message is copied so
// you can do whatever with input buffer after
void bluetooth_send_message(const void* src);

// Send a raw message to the board using interrupts. Message gets copied.
void bluetooth_send_raw(const void* src, uint8_t size);

// Call once each frame
void bluetooth_tick(void);

// Bluetooth debug mode using polling
void bluetooth_debug_poll(void);

// Bluetooth debug mode using interrupts
void bluetooth_debug_interrupt(void);

#endif	/* BLUETOOTH_H */

