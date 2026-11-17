// Drive in a square: forward, turn right 90 degrees, four times. Open loop (timed only).
//
// Run wheeltest first to check wheel directions. Then calibrate on the floor:
//   SIDE_MS - how long to drive straight for one side of the square
//   TURN_MS - how long to spin in place for a 90 degree right turn
// Both depend on the battery charge and floor, so expect to adjust them a few times.
// A chassis bump switch stops everything immediately. Use a clear, flat area.

#include "board.h"
#include "motor.h"

#define SPEED 30        // percent duty for driving straight
#define SIDE_MS 2000    // CALIBRATE: time per side
#define TURN_SPEED 30   // percent duty while turning
#define TURN_MS 1200    // CALIBRATE: time for a 90 degree turn (600 gave about 45 degrees)
#define PAUSE_MS 400    // stand still between moves so the robot settles

int main(void) {
  board_init();
  motor_init();
  countdown(5);

  for (int side = 0; side < 4; side++) {
    motor_run_ms(SPEED, SPEED, SIDE_MS);
    motor_run_ms(0, 0, PAUSE_MS);
    motor_run_ms(TURN_SPEED, -TURN_SPEED, TURN_MS);  // right turn: left forward, right back
    motor_run_ms(0, 0, PAUSE_MS);
  }
  motor_off();

  while (1) {
    led_toggle();
    delay_ms(500);
  }
}
