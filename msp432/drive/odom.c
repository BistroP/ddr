#include "odom.h"

#include <math.h>
#include <stdint.h>

#include "encoder.h"

#define PI_F 3.14159265f

// Robot measurements. Wheels: 70 mm. Distance between the wheels: 141 mm (Romi chassis;
// not checked on this chassis, but the turns come out about right with it).
// Counts per wheel turn is PER ROBOT, so measure it for each one (command a 300 mm leg and
// measure how far it really goes; a 90 degree turn is wrong by the same factor):
//   * robot 1: 360. With 720 its "300 mm" legs went 600 mm and its "90 degree" turns were
//     about 190 degrees. (Why that robot gives half the expected counts is not known.)
//   * robot 2: 720, which is what two edges of a 360-pulse channel give. With 360 its legs
//     went about 150 mm and its turns were about 45 degrees.
#define WHEEL_DIAMETER_MM 70.0f
#define TRACK_MM 141.0f
#define COUNTS_PER_REV 720.0f
#define MM_PER_COUNT (PI_F * WHEEL_DIAMETER_MM / COUNTS_PER_REV)

static float x_mm, y_mm;
static float theta;      // radians, clockwise from +y
static float dist_mm;
static int32_t last_left, last_right;

void odom_reset(void) {
  x_mm = 0.0f;
  y_mm = 0.0f;
  theta = 0.0f;
  dist_mm = 0.0f;
  last_left = encoder_left();
  last_right = encoder_right();
}

void odom_update(void) {
  int32_t left = encoder_left();
  int32_t right = encoder_right();
  float d_left = (float)(left - last_left) * MM_PER_COUNT;
  float d_right = (float)(right - last_right) * MM_PER_COUNT;
  last_left = left;
  last_right = right;

  float d_center = 0.5f * (d_left + d_right);
  // Left wheel travelling further than the right = turning right = clockwise = positive.
  float d_theta = (d_left - d_right) / TRACK_MM;

  // Move along the average heading of this small step.
  float mid = theta + 0.5f * d_theta;
  x_mm += d_center * sinf(mid);
  y_mm += d_center * cosf(mid);
  theta += d_theta;
  dist_mm += fabsf(d_center);

  // Keep theta in -pi..pi.
  if (theta > PI_F) {
    theta -= 2.0f * PI_F;
  } else if (theta < -PI_F) {
    theta += 2.0f * PI_F;
  }
}

int odom_x_mm(void) {
  return (int)lroundf(x_mm);
}

int odom_y_mm(void) {
  return (int)lroundf(y_mm);
}

int odom_heading_deg(void) {
  int deg = (int)lroundf(theta * (180.0f / PI_F));
  if (deg > 180) deg -= 360;
  if (deg < -180) deg += 360;
  return deg;
}

int odom_distance_mm(void) {
  return (int)lroundf(dist_mm);
}
