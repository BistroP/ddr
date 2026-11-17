// Drive-and-watch test: repeats the demo's moves (a straight run, then a turn) with nothing
// else running: no odometry, no checks, no steering. It shows what the encoders do WHILE the
// motors run, which enctest (wheels not driven) cannot.
//
// The robot sends a line 10 times a second, e.g.   RUN 4200ms|L391 R386
// The Buckler shows "RUN 4200ms" on row 0 and the left/right encoder counts on row 1.
//
// Timeline (all times from power-up):
//   WAIT   3 s   motors off
//   RUN    3 s   both wheels forward at 30%     (counts should rise on both sides)
//   PAUSE  0.5 s motors off (brake)
//   TURN   2 s   left wheel forward, right wheel backward at 25%
//                (left count should rise, right count should FALL)
//   STOP         motors off, numbers hold still forever
// Run it with the robot held in the air or on blocks.
//
// To run it again without flashing, press the LaunchPad's RESET button.

#include "board.h"
#include "motor.h"
#include "encoder.h"
#include "uart.h"

#define SPEED 30
#define TURN_SPEED 25
#define WAIT_MS 3000
#define RUN_MS 3000
#define PAUSE_MS 500
#define TURN_MS 2000
#define REPORT_MS 100

static unsigned ms;               // milliseconds since power-up
static const char *phase = "WAIT";

static void report(void) {
  uart_puts(phase);
  uart_putc(' ');
  uart_put_uint(ms);
  uart_puts("ms|L");
  uart_put_int((int)encoder_left());
  uart_puts(" R");
  uart_put_int((int)encoder_right());
  uart_putc('\n');
}

// Called once per millisecond while we wait or drive.
static int tick(void) {
  ms++;
  if (ms % REPORT_MS == 0) {
    report();
  }
  return 0;
}

int main(void) {
  board_init();
  motor_init();
  uart_init();
  encoder_init();

  motor_run_until(0, 0, WAIT_MS, tick);

  phase = "RUN";
  motor_run_until(SPEED, SPEED, RUN_MS, tick);

  phase = "PAUSE";
  motor_run_until(0, 0, PAUSE_MS, tick);

  phase = "TURN";
  motor_run_until(TURN_SPEED, -TURN_SPEED, TURN_MS, tick);

  motor_off();
  phase = "STOP";
  while (1) {
    delay_ms(1);
    tick();
  }
}
