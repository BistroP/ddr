// Odometry: estimate where the robot is from the wheel encoder counts (dead reckoning).
//
// Coordinates: the robot starts at x = 0, y = 0 facing "up" (+y). x grows to the right.
// Heading is in degrees, clockwise from +y, so a right turn makes it positive.
// Everything is an ESTIMATE: wheel slip and measurement errors add up over distance.
#pragma once

void odom_reset(void);          // here and now becomes (0, 0), heading 0
void odom_update(void);         // read the encoders and update the estimate (call ~every 20 ms)

int odom_x_mm(void);
int odom_y_mm(void);
int odom_heading_deg(void);     // -180..180
int odom_distance_mm(void);     // total path length driven since reset (always counts up)
