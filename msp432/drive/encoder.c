#include "encoder.h"
#include "msp.h"

// If a wheel's count goes DOWN while it drives forward, flip that wheel's sign here.
// Both were -1 after the first demo runs: the demo reported "ERR stall h-21|L-497 R-322",
// i.e. both counts negative while the robot drove forward, so the decoder's idea of
// "forward" is backwards for both wheels. (Spinning a wheel by hand is an unreliable check,
// because it is easy to spin it the wrong way; the demo's own readout is the real test.)
#define LEFT_ENC_SIGN -1
#define RIGHT_ENC_SIGN -1

static volatile int32_t left_count;
static volatile int32_t right_count;

// Interrupt on the next change of the pin at 'bit': if it is high now, wait for it to fall;
// if low, wait for it to rise. Done after every edge, so every edge (both kinds) interrupts.
static void arm_next_edge(uint8_t bit) {
  if (P5->IN & bit) {
    P5->IES |= bit;   // currently high: trigger on falling edge
  } else {
    P5->IES &= ~bit;  // currently low: trigger on rising edge
  }
  P5->IFG &= ~bit;    // drop any flag that changing the edge select may have raised
}

void encoder_init(void) {
  // A channels: plain inputs on port 10 (only read, no interrupt).
  P10->DIR &= ~(BIT4 | BIT5);

  // B channels: inputs on port 5 with interrupts.
  P5->DIR &= ~(BIT0 | BIT2);
  P5->SEL0 &= ~(BIT0 | BIT2);
  P5->SEL1 &= ~(BIT0 | BIT2);
  P5->IE &= ~(BIT0 | BIT2);

  left_count = 0;
  right_count = 0;
  arm_next_edge(BIT0);
  arm_next_edge(BIT2);
  P5->IE |= BIT0 | BIT2;
  NVIC_EnableIRQ(PORT5_IRQn);
}

// Port 5 interrupt: a B channel changed.
void PORT5_IRQHandler(void) {
  uint8_t flags = P5->IFG;

  if (flags & BIT2) {  // left wheel
    int b = (P5->IN & BIT2) != 0;
    int a = (P10->IN & BIT5) != 0;
    left_count += LEFT_ENC_SIGN * encoder_step(a, b);
    arm_next_edge(BIT2);
  }
  if (flags & BIT0) {  // right wheel
    int b = (P5->IN & BIT0) != 0;
    int a = (P10->IN & BIT4) != 0;
    right_count += RIGHT_ENC_SIGN * encoder_step(a, b);
    arm_next_edge(BIT0);
  }
}

int32_t encoder_left(void) {
  return left_count;
}

int32_t encoder_right(void) {
  return right_count;
}
