#include "uart.h"
#include "msp.h"

void uart_init(void) {
  // P3.2 = UCA2RXD, P3.3 = UCA2TXD (primary module function; the port mapper default).
  P3->SEL0 |= BIT2 | BIT3;
  P3->SEL1 &= ~(BIT2 | BIT3);

  // Configure while held in reset: UART, 8N1, clocked from SMCLK (3 MHz).
  EUSCI_A2->CTLW0 = EUSCI_A_CTLW0_SWRST | EUSCI_A_CTLW0_SSEL__SMCLK;

  // 115200 baud from 3 MHz with 16x oversampling: N = 3000000 / 115200 = 26.04
  //   UCBR = 1, UCBRF = 10, UCBRS = 0  ->  divisor 26, i.e. 115385 baud (0.16 % fast).
  EUSCI_A2->BRW = 1;
  EUSCI_A2->MCTLW = (0u << 8) | (10u << 4) | EUSCI_A_MCTLW_OS16;

  EUSCI_A2->CTLW0 &= ~EUSCI_A_CTLW0_SWRST;  // release: UART runs
}

void uart_putc(char c) {
  while (!(EUSCI_A2->IFG & EUSCI_A_IFG_TXIFG)) {
  }
  EUSCI_A2->TXBUF = (uint8_t)c;
}

void uart_puts(const char *s) {
  while (*s) {
    uart_putc(*s++);
  }
}

void uart_put_uint(unsigned value) {
  char digits[10];
  int n = 0;
  do {
    digits[n++] = (char)('0' + value % 10);
    value /= 10;
  } while (value);
  while (n) {
    uart_putc(digits[--n]);
  }
}

void uart_put_int(int value) {
  if (value < 0) {
    uart_putc('-');
    // Negate as unsigned so the most negative int does not overflow.
    uart_put_uint(0u - (unsigned)value);
  } else {
    uart_put_uint((unsigned)value);
  }
}

int uart_getc(void) {
  if (EUSCI_A2->IFG & EUSCI_A_IFG_RXIFG) {
    return (uint8_t)EUSCI_A2->RXBUF;  // reading clears the flag
  }
  return -1;
}
