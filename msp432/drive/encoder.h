// Wheel encoders (the Hall-effect sensors on the two gearmotors).
//
// Each wheel has two signals, A and B, 90 degrees out of phase (quadrature). From the Pololu
// chassis pinout:   left : A = P10.5  B = P5.2        right: A = P10.4  B = P5.0
// The B signals (on port 5, which can interrupt) fire an interrupt on EVERY edge. In the
// interrupt we read A to learn the direction. On this robot that gives about 360 counts per
// wheel revolution (measured from how far the robot really drives; see odom.c).
#pragma once

#include <stdint.h>

void encoder_init(void);        // set up the pins and the port 5 interrupt, counts start at 0
int32_t encoder_left(void);     // counts so far; positive = wheel turning forward
int32_t encoder_right(void);

// One decoding step. Call it when B has just changed, with the levels (0 or 1) of A and B
// right after the change. With A leading B (the wheel turning one way), B rises while A is
// high and falls while A is low, so A == B. The other way round, A != B.
static inline int encoder_step(int a, int b) {
  return (a == b) ? 1 : -1;
}
