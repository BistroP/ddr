// Coordinate demo, robot side.
//
// The robot starts at (0, 0) facing +y, and reports its estimated position to the Buckler
// ten times a second as a line like "POS 300 0 90" (x in mm, y in mm, heading in degrees).
// After a 5 second countdown it drives a square, SIDES times:
//     forward one cell (300 mm)  ->  turn right 90 degrees
// so the corners it reports are (0, 1), (1, 1), (1, 0) and finally (0, 0) again (in cells,
// x to the right and y up), and it ends facing the way it started.
// Distances and angles come from the wheel encoders (see odom.c), so what the Buckler shows
// is what the robot BELIEVES. Wheel slip makes it drift from the true position.
//
// Run this on the floor, in a clear area, only after checking the encoders with enctest.
// Safety:
//   * A chassis bump switch stops everything.
//   * If the encoders do not report sensible motion (nothing moves, the robot veers off, or
//     the turn goes the wrong way) the robot
//     stops and sends one line saying which check failed, e.g. "ERR drift h25|L412 R300":
//     the reason (stall, drift, noturn, wrong), the heading in degrees, and the raw
//     left and right encoder counts at that moment. The Buckler shows the part before the
//     "|" on row 0 and the part after it on row 1. After 1.5 s a second line follows, e.g.
//     "LEG 3010ms|L985 R981 p301": how the last leg ended (its running time, the counts, and
//     the distance in mm it covered). The two lines then alternate forever.
//   * Every step also has a timeout.
//   * Straight legs steer to hold their heading (see STEER_GAIN).

#include <math.h>

#include "board.h"
#include "motor.h"
#include "odom.h"
#include "encoder.h"
#include "uart.h"

#define UNIT_MM 300          // one cell
#define SIDES 4              // 4 sides make a square (1 would be just one cell and a turn)
#define DRIVE_SPEED 20       // percent duty while driving straight (30 is fast; lower = slower)
#define TURN_SPEED 22        // percent duty while turning in place (lower = slower)
#define TURN_DEG 90
#define TURN_MARGIN_DEG 0    // stop this many degrees BEFORE the target heading (0 = stop on
                             // reaching it; raise it if the robot overshoots while coasting)
#define LEG_TIMEOUT_MS 15000 // only a safety net: a leg normally ends on reaching UNIT_MM
#define TURN_TIMEOUT_MS 10000 // only a safety net: a turn normally ends on reaching its heading
#define SETTLE_MS 1500       // stand still between moves (long enough to read the display)
#define COUNTDOWN_MS 5000

// Encoder sanity checks, judged CHECK_MS after a step begins:
#define CHECK_MS 800
#define MIN_FORWARD_MM 10    // a leg must have made at least this much forward progress
#define MAX_DRIFT_DEG 25     // a "straight" leg must not turn more than this
#define MIN_TURN_DEG 5       // a turn must have turned at least this much
#define WRONG_WAY_DEG 15     // ...and must not have turned this far the other way

// Steering on the straight legs. The two motors never run at exactly the same speed, so the
// robot would curve. Every ODOM_PERIOD_MS the heading error (from the encoders) becomes a
// small power difference between the wheels: drifted clockwise (positive heading error) ->
// less power to the left wheel, more to the right; drifted anticlockwise -> the opposite.
// Only the straight legs steer; turns and stops are unchanged.
#define STEER_GAIN 1.5f      // percent power difference per degree of heading error
#define STEER_MAX_PCT 5      // never change either wheel by more than this

#define ODOM_PERIOD_MS 20
#define POS_PERIOD_MS 100

#define DEG_TO_RAD 0.0174532925f

static unsigned ms_ticks;
static unsigned step_ms;     // milliseconds since the current step began

// Set when a sanity check fails (0 while all is well), with a snapshot of the numbers.
static const char *fail_reason;
static int fail_left, fail_right, fail_heading;

static int leg_start_x, leg_start_y, leg_start_deg;
static float leg_dir_x, leg_dir_y;  // unit vector of the leg's direction (x right, y up)
static int turn_start_deg;
static int turn_target_deg;         // absolute heading this turn aims for

// How the most recently driven leg ended, kept so a failure report can show it.
static unsigned leg_end_ms;                       // time the leg ran, in ms
static int leg_end_left, leg_end_right;           // encoder counts when it stopped
static int leg_end_progress;                      // distance covered, in mm

static void send_pos(void) {
  uart_puts("POS ");
  uart_put_int(odom_x_mm());
  uart_putc(' ');
  uart_put_int(odom_y_mm());
  uart_putc(' ');
  uart_put_int(odom_heading_deg());
  uart_putc('\n');
}

// Runs once per millisecond while the robot is busy: keeps the position estimate fresh and
// reports it. Returns 0 so it can be passed as a "never stop early" callback.
static int service(void) {
  ms_ticks++;
  if (ms_ticks % ODOM_PERIOD_MS == 0) {
    odom_update();
  }
  if (ms_ticks % POS_PERIOD_MS == 0) {
    send_pos();
  }
  return 0;
}

static int countdown_service(void) {
  if (ms_ticks % 500 == 0) {
    led_toggle();
  }
  return service();
}

static int wrap180(int deg) {
  while (deg > 180) deg -= 360;
  while (deg <= -180) deg += 360;
  return deg;
}

// Remember which check failed and what the encoders said at that moment.
static void note_failure(const char *reason) {
  fail_reason = reason;
  fail_left = (int)encoder_left();
  fail_right = (int)encoder_right();
  fail_heading = odom_heading_deg();
}

// The encoders disagree with what the motors are doing: stop and say so, forever.
// Line format (see the header comment): ERR <reason> h<heading>|L<left> R<right>
static void fail_forever(void) {
  motor_off();
  while (1) {
    // Line 1: which check failed, and the encoder counts at that moment.
    uart_puts("ERR ");
    uart_puts(fail_reason);
    uart_puts(" h");
    uart_put_int(fail_heading);
    uart_putc('|');
    uart_putc('L');
    uart_put_int(fail_left);
    uart_puts(" R");
    uart_put_int(fail_right);
    uart_putc('\n');
    led_toggle();
    delay_ms(1500);

    // Line 2: how the leg that was driven last ended: time, counts, distance covered.
    uart_puts("LEG ");
    uart_put_uint(leg_end_ms);
    uart_puts("ms|L");
    uart_put_int(leg_end_left);
    uart_puts(" R");
    uart_put_int(leg_end_right);
    uart_puts(" p");
    uart_put_int(leg_end_progress);
    uart_putc('\n');
    delay_ms(1500);
  }
}

// Forward progress along the leg's direction, in mm (negative if going backwards).
static float leg_progress(void) {
  return (float)(odom_x_mm() - leg_start_x) * leg_dir_x +
         (float)(odom_y_mm() - leg_start_y) * leg_dir_y;
}

static int leg_done(void) {
  service();
  step_ms++;
  if (ms_ticks % ODOM_PERIOD_MS != 0) {
    return 0;  // judge only right after the estimate was refreshed
  }
  float progress = leg_progress();
  if (progress >= (float)UNIT_MM) {
    return 1;
  }
  int drift = wrap180(odom_heading_deg() - leg_start_deg);
  if (step_ms >= CHECK_MS && progress < (float)MIN_FORWARD_MM) {
    note_failure("stall");
    return 1;
  }
  if (drift > MAX_DRIFT_DEG || drift < -MAX_DRIFT_DEG) {
    note_failure("drift");
    return 1;
  }

  // Steer back toward the heading the leg started with.
  int correction = (int)lroundf(STEER_GAIN * (float)drift);
  if (correction > STEER_MAX_PCT) {
    correction = STEER_MAX_PCT;
  }
  if (correction < -STEER_MAX_PCT) {
    correction = -STEER_MAX_PCT;
  }
  motor_set(DRIVE_SPEED - correction, DRIVE_SPEED + correction);
  return 0;
}

static int turn_done(void) {
  service();
  step_ms++;
  if (ms_ticks % ODOM_PERIOD_MS != 0) {
    return 0;
  }
  int turned = wrap180(odom_heading_deg() - turn_start_deg);
  // Degrees still to go (zero or negative once the target is reached or passed).
  if (wrap180(turn_target_deg - odom_heading_deg()) <= TURN_MARGIN_DEG) {
    return 1;
  }
  if (step_ms >= CHECK_MS && turned < MIN_TURN_DEG) {
    note_failure("noturn");
    return 1;
  }
  if (turned < -WRONG_WAY_DEG) {
    note_failure("wrong");
    return 1;
  }
  return 0;
}

static void drive_one_cell(void) {
  leg_start_x = odom_x_mm();
  leg_start_y = odom_y_mm();
  leg_start_deg = odom_heading_deg();
  leg_dir_x = sinf((float)leg_start_deg * DEG_TO_RAD);
  leg_dir_y = cosf((float)leg_start_deg * DEG_TO_RAD);
  step_ms = 0;
  motor_run_until(DRIVE_SPEED, DRIVE_SPEED, LEG_TIMEOUT_MS, leg_done);
  leg_end_ms = step_ms;
  leg_end_left = (int)encoder_left();
  leg_end_right = (int)encoder_right();
  leg_end_progress = (int)lroundf(leg_progress());
  motor_run_until(0, 0, SETTLE_MS, service);  // brake and keep reporting
  if (fail_reason) {
    fail_forever();
  }
}

// Turn right until the heading reaches target_deg (an absolute heading, so a turn that lands
// a little short or long does not carry its error into the next side).
static void turn_right(int target_deg) {
  turn_start_deg = odom_heading_deg();
  turn_target_deg = wrap180(target_deg);
  step_ms = 0;
  motor_run_until(TURN_SPEED, -TURN_SPEED, TURN_TIMEOUT_MS, turn_done);
  motor_run_until(0, 0, SETTLE_MS, service);
  if (fail_reason) {
    fail_forever();
  }
}

int main(void) {
  board_init();
  motor_init();
  uart_init();
  encoder_init();
  odom_reset();

  // Sit still for 5 seconds, already reporting the start position (0, 0).
  motor_run_until(0, 0, COUNTDOWN_MS, countdown_service);

  for (int side = 0; side < SIDES; side++) {
    drive_one_cell();
    turn_right(TURN_DEG * (side + 1));  // 90, 180, 270, 360 degrees
  }
  motor_off();

  // Done: keep reporting the final position.
  led(1);
  while (1) {
    delay_ms(1);
    service();
  }
}
