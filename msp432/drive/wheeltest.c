// Wheel test. Put the robot on blocks so the wheels spin in the air.
//
// After a 5 second countdown (LED blinks once per second; nothing moves) it runs:
//   1. left wheel forward   2. right wheel forward   3. both forward
//   4. both backward        5. spin in place (left forward, right backward)
// with 1 second of stillness between steps, then turns the motors off.
//
// Watch which wheel moves and which way. If "forward" turns a wheel backwards,
// flip LEFT_FWD_LEVEL / RIGHT_FWD_LEVEL in motor.c.
// Pressing any chassis bump switch stops everything immediately.

#include "board.h"
#include "motor.h"

#define SPEED 30     // percent duty; low on purpose
#define STEP_MS 2000
#define REST_MS 1000

int main(void) {
  board_init();
  motor_init();
  countdown(5);

  motor_run_ms(SPEED, 0, STEP_MS);        // left forward
  motor_run_ms(0, 0, REST_MS);
  motor_run_ms(0, SPEED, STEP_MS);        // right forward
  motor_run_ms(0, 0, REST_MS);
  motor_run_ms(SPEED, SPEED, STEP_MS);    // both forward
  motor_run_ms(0, 0, REST_MS);
  motor_run_ms(-SPEED, -SPEED, STEP_MS);  // both backward
  motor_run_ms(0, 0, REST_MS);
  motor_run_ms(SPEED, -SPEED, STEP_MS);   // spin in place
  motor_off();

  // Done: slow blink forever.
  while (1) {
    led_toggle();
    delay_ms(500);
  }
}
