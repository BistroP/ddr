// Drives the two TI-RSLK MAX wheel motors through the chassis board's DRV8838 drivers.
//
// Pins (MSP432 names, from the Pololu chassis pinout):
//   left : PWM P2.7 (Timer_A0 CCR4), DIR P5.4, nSLEEP P3.7
//   right: PWM P2.6 (Timer_A0 CCR3), DIR P5.5, nSLEEP P3.6
#pragma once

void motor_init(void);                                 // pins + PWM set up, drivers asleep
void motor_set(int left_pct, int right_pct);           // -100..100, sign = direction, 0 = hold still
void motor_off(void);                                  // both drivers asleep (wheels coast)
void motor_run_ms(int left_pct, int right_pct, int ms); // run, then return; a bump switch stops everything

// Like motor_run_ms, but calls stop() once per millisecond and ends early when it returns
// nonzero (stop may be NULL). Returns how many milliseconds it ran. A bump switch still stops
// everything. The motors are left running at the given speed when it returns.
int motor_run_until(int left_pct, int right_pct, int max_ms, int (*stop)(void));
