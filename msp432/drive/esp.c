#include "esp.h"
#include "msp.h"

// Receive ring buffer: the interrupt writes at rx_head, esp_poll_line() reads at rx_tail.
// The size must be a power of two. If it ever fills up, new bytes are dropped.
#define RX_SIZE 128
static volatile uint8_t rx_buf[RX_SIZE];
static volatile unsigned rx_head;
static volatile unsigned rx_tail;

void esp_init(void) {
  // P2.2 = UCA1RXD, P2.3 = UCA1TXD (primary module function).
  P2->SEL0 |= BIT2 | BIT3;
  P2->SEL1 &= ~(BIT2 | BIT3);

  // Same settings as uart.c: configure while held in reset, 115200 baud from 3 MHz with
  // 16x oversampling (UCBR = 1, UCBRF = 10, UCBRS = 0).
  EUSCI_A1->CTLW0 = EUSCI_A_CTLW0_SWRST | EUSCI_A_CTLW0_SSEL__SMCLK;
  EUSCI_A1->BRW = 1;
  EUSCI_A1->MCTLW = (0u << 8) | (10u << 4) | EUSCI_A_MCTLW_OS16;
  EUSCI_A1->CTLW0 &= ~EUSCI_A_CTLW0_SWRST;

  EUSCI_A1->IFG &= ~EUSCI_A_IFG_RXIFG;
  EUSCI_A1->IE |= EUSCI_A_IE_RXIE;  // interrupt on every received byte
  NVIC_EnableIRQ(EUSCIA1_IRQn);
}

// Runs for every byte the ESP32 sends.
void EUSCIA1_IRQHandler(void) {
  if (EUSCI_A1->IFG & EUSCI_A_IFG_RXIFG) {
    uint8_t c = (uint8_t)EUSCI_A1->RXBUF;  // reading clears the flag
    unsigned next = (rx_head + 1) & (RX_SIZE - 1);
    if (next != rx_tail) {  // not full
      rx_buf[rx_head] = c;
      rx_head = next;
    }
  }
}

void esp_putc(char c) {
  while (!(EUSCI_A1->IFG & EUSCI_A_IFG_TXIFG)) {
  }
  EUSCI_A1->TXBUF = (uint8_t)c;
}

void esp_puts(const char *s) {
  while (*s) {
    esp_putc(*s++);
  }
}

static void esp_put_uint(unsigned value) {
  char digits[10];
  int n = 0;
  do {
    digits[n++] = (char)('0' + value % 10);
    value /= 10;
  } while (value);
  while (n) {
    esp_putc(digits[--n]);
  }
}

void esp_put_int(int value) {
  if (value < 0) {
    esp_putc('-');
    // Negate as unsigned so the most negative int does not overflow.
    esp_put_uint(0u - (unsigned)value);
  } else {
    esp_put_uint((unsigned)value);
  }
}

int esp_poll_line(char *line, int cap) {
  static char buf[64];
  static int len;
  static int overflow;  // the line in progress was too long: throw it away at its newline

  while (rx_tail != rx_head) {
    char c = (char)rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & (RX_SIZE - 1);

    if (c == '\n') {
      int n = len;
      int bad = overflow;
      len = 0;
      overflow = 0;
      if (n > 0 && !bad) {
        int i = 0;
        while (i < n && i < cap - 1) {
          line[i] = buf[i];
          i++;
        }
        line[i] = '\0';
        return i;
      }
    } else if (c != '\r') {
      if (len < (int)sizeof buf) {
        buf[len++] = c;
      } else {
        overflow = 1;
      }
    }
  }
  return 0;
}
