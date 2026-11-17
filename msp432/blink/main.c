// MSP432 LaunchPad LED blink: first test that the toolchain and flashing work.
// Blinks the red LED1 (P1.0) and cycles the RGB LED2 (P2.0, P2.1, P2.2).
// Does not touch P2.6 / P2.7 (motor PWM on the chassis board).

#include "msp.h"
#include <stdint.h>

static void delay(volatile uint32_t n) {
  while (n--) {
  }
}

int main(void) {
  // Stop the watchdog so the chip does not keep resetting.
  WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;

  P1->DIR |= BIT0;
  P1->OUT &= ~BIT0;
  P2->DIR |= BIT0 | BIT1 | BIT2;
  P2->OUT &= ~(BIT0 | BIT1 | BIT2);

  uint8_t color = 0;
  while (1) {
    P1->OUT ^= BIT0;
    P2->OUT = (P2->OUT & ~(BIT0 | BIT1 | BIT2)) | (1u << color);
    color = (color + 1) % 3;
    delay(250000);
  }
}
