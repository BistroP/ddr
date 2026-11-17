#include "motor.h"
#include "board.h"
#include "msp.h"

// PWM: 3 MHz SMCLK / 3000 counts = 1 kHz. One percent of duty = 30 counts.
#define PWM_PERIOD 3000
#define COUNTS_PER_PERCENT (PWM_PERIOD / 100)

// Safety cap on duty cycle, whatever the caller asks for.
#define MAX_PERCENT 60

// DIR pin level that makes each wheel turn forward. Both 0 in TI's own driver.
// If a wheel turns the wrong way in the wheel test, flip that wheel's value here.
#define LEFT_FWD_LEVEL 0
#define RIGHT_FWD_LEVEL 0

void motor_init(void) {
  // nSLEEP low (drivers asleep) and DIR low, as outputs.
  P3->DIR |= BIT6 | BIT7;
  P3->OUT &= ~(BIT6 | BIT7);
  P5->DIR |= BIT4 | BIT5;
  P5->OUT &= ~(BIT4 | BIT5);

  // P2.6 / P2.7 become Timer_A0 outputs (CCR3 / CCR4).
  P2->DIR |= BIT6 | BIT7;
  P2->SEL0 |= BIT6 | BIT7;
  P2->SEL1 &= ~(BIT6 | BIT7);

  TIMER_A0->CTL = TIMER_A_CTL_TASSEL_2 | TIMER_A_CTL_MC__UP | TIMER_A_CTL_CLR;
  TIMER_A0->CCR[0] = PWM_PERIOD - 1;
  TIMER_A0->CCTL[3] = TIMER_A_CCTLN_OUTMOD_7;  // reset/set: duty = CCR / PWM_PERIOD
  TIMER_A0->CCTL[4] = TIMER_A_CCTLN_OUTMOD_7;
  TIMER_A0->CCR[3] = 0;
  TIMER_A0->CCR[4] = 0;
}

static void wheel(int is_left, int pct) {
  int forward = (pct >= 0);
  // Clamp before negating: negating the most negative int would overflow.
  if (pct > MAX_PERCENT) {
    pct = MAX_PERCENT;
  }
  if (pct < -MAX_PERCENT) {
    pct = -MAX_PERCENT;
  }
  if (!forward) {
    pct = -pct;
  }

  uint8_t dir_bit = is_left ? BIT4 : BIT5;
  uint8_t sleep_bit = is_left ? BIT7 : BIT6;
  int fwd_level = is_left ? LEFT_FWD_LEVEL : RIGHT_FWD_LEVEL;
  int dir_level = forward ? fwd_level : !fwd_level;

  // Set direction first, then the speed, then wake the driver.
  if (dir_level) {
    P5->OUT |= dir_bit;
  } else {
    P5->OUT &= ~dir_bit;
  }
  TIMER_A0->CCR[is_left ? 4 : 3] = pct * COUNTS_PER_PERCENT;
  P3->OUT |= sleep_bit;  // awake: with 0% duty the DRV8838 brakes the wheel
}

void motor_set(int left_pct, int right_pct) {
  wheel(1, left_pct);
  wheel(0, right_pct);
}

void motor_off(void) {
  TIMER_A0->CCR[3] = 0;
  TIMER_A0->CCR[4] = 0;
  P3->OUT &= ~(BIT6 | BIT7);
}

// Emergency stop: motors off, then blink the LED fast forever. Reset to start over.
static void emergency_stop(void) {
  motor_off();
  while (1) {
    led_toggle();
    delay_ms(100);
  }
}

int motor_run_until(int left_pct, int right_pct, int max_ms, int (*stop)(void)) {
  if (bump_pressed()) {
    emergency_stop();
  }
  motor_set(left_pct, right_pct);
  for (int i = 0; i < max_ms; i++) {
    if (bump_pressed()) {
      emergency_stop();
    }
    if (stop && stop()) {
      return i;
    }
    delay_ms(1);
  }
  return max_ms;
}

void motor_run_ms(int left_pct, int right_pct, int ms) {
  motor_run_until(left_pct, right_pct, ms, 0);
}
