// UART link to the ESP32 radio board: eUSCI_A1, 115200 baud, 8 data bits, no parity, 1 stop bit.
// Pins: P2.2 = receive (from the ESP32's TX pin), P2.3 = transmit (to the ESP32's RX pin).
// Needs the 3 MHz SMCLK that board_init() sets up.
//
// Unlike uart.c (which only sends), this port also RECEIVES, and bytes arrive too fast to
// poll safely (one every ~87 us), so a receive interrupt stores them in a buffer and
// esp_poll_line() hands them out one complete line at a time.
#pragma once

void esp_init(void);
void esp_putc(char c);              // waits until the byte is sent
void esp_puts(const char *s);
void esp_put_int(int value);        // decimal with a leading '-' when negative

// If a complete line has arrived from the ESP32, copies it (without the newline, ended with
// a '\0', at most cap-1 characters) into 'line' and returns its length. Returns 0 when no
// complete line is waiting yet. Call it often; cap must be at least 1.
int esp_poll_line(char *line, int cap);
