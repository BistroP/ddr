// Basic LaunchPad / chassis helpers: clocks, LED, millisecond delay, bump switches.
#pragma once

#include <stdint.h>

void board_init(void);          // watchdog off, 3 MHz clocks, LED and bump inputs ready
void delay_ms(uint32_t ms);     // busy-wait using SysTick
void delay_ms_poll(uint32_t ms, void (*poll)(void));  // same, but calls poll() nonstop while waiting
int  bump_pressed(void);        // nonzero if any chassis bump switch is pressed
void led(int on);               // LaunchPad red LED (LED1, P1.0)
void led_toggle(void);
void countdown(int seconds);    // blink the LED once per second; robot stays still
