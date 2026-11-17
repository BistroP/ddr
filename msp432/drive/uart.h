// UART link to the Buckler: eUSCI_A2, 115200 baud, 8 data bits, no parity, 1 stop bit.
// Pins: P3.2 = receive (from the Buckler), P3.3 = transmit (to the Buckler).
// Needs the 3 MHz SMCLK that board_init() sets up.
#pragma once

void uart_init(void);
void uart_putc(char c);              // waits until the byte is sent
void uart_puts(const char *s);
void uart_put_uint(unsigned value);  // decimal, no leading zeros
void uart_put_int(int value);        // decimal with a leading '-' when negative
int  uart_getc(void);                // next received byte, or -1 if none is waiting
