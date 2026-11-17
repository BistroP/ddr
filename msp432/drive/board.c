#include "board.h"
#include "msp.h"

// Chassis bump switches BMP0..BMP5 on P4.0, P4.2, P4.3, P4.5, P4.6, P4.7.
// They connect the pin to ground when pressed, so we use internal pull-ups.
#define BUMP_MASK (BIT0 | BIT2 | BIT3 | BIT5 | BIT6 | BIT7)

#define CORE_HZ 3000000u

void board_init(void) {
  // Watchdog off (TI's SystemInit already does this; harmless to repeat).
  WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;

  // MCLK, HSMCLK, SMCLK all from the DCO (3 MHz at reset), every divider = 1.
  // The PWM timer runs from SMCLK, so this fixes the PWM frequency.
  CS->KEY = CS_KEY_VAL;
  CS->CTL1 = CS_CTL1_SELA_2 | CS_CTL1_SELS_3 | CS_CTL1_SELM_3;
  CS->KEY = 0;

  P1->DIR |= BIT0;
  P1->OUT &= ~BIT0;

  P4->DIR &= ~BUMP_MASK;
  P4->REN |= BUMP_MASK;
  P4->OUT |= BUMP_MASK;
}

void delay_ms(uint32_t ms) {
  SysTick->LOAD = (CORE_HZ / 1000u) - 1u;
  SysTick->VAL = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
  while (ms--) {
    // Reading CTRL clears COUNTFLAG; it sets again once per millisecond.
    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) {
    }
  }
  SysTick->CTRL = 0;
}

void delay_ms_poll(uint32_t ms, void (*poll)(void)) {
  SysTick->LOAD = (CORE_HZ / 1000u) - 1u;
  SysTick->VAL = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
  while (ms) {
    poll();
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {  // one millisecond passed
      ms--;
    }
  }
  SysTick->CTRL = 0;
}

int bump_pressed(void) {
  return (P4->IN & BUMP_MASK) != BUMP_MASK;
}

void led(int on) {
  if (on) {
    P1->OUT |= BIT0;
  } else {
    P1->OUT &= ~BIT0;
  }
}

void led_toggle(void) {
  P1->OUT ^= BIT0;
}

void countdown(int seconds) {
  for (int i = 0; i < seconds; i++) {
    led(1);
    delay_ms(100);
    led(0);
    delay_ms(900);
  }
}
