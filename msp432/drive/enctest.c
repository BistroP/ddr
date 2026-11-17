// Encoder bench test. The wheels are NOT driven: turn them by hand.
//
// Sends "ENC <left> <right>" ten times a second to the Buckler (it shows the raw line).
// What to check:
//   * Turn the left wheel one full turn forward (top of the wheel moving toward the bump
//     switches): the left count should rise by about 360. Backwards: it should fall.
//   * Same for the right wheel.
// If a count goes DOWN when the wheel turns forward, flip that wheel's sign in encoder.c.
// If a count rises by some other amount per turn, the counts-per-turn constant in odom.c
// needs adjusting (it was measured as 360 from how far the robot really drives).

#include "board.h"
#include "encoder.h"
#include "motor.h"
#include "uart.h"

int main(void) {
  board_init();
  motor_init();  // only to keep the motor drivers asleep
  uart_init();
  encoder_init();

  while (1) {
    uart_puts("ENC ");
    uart_put_int((int)encoder_left());
    uart_putc(' ');
    uart_put_int((int)encoder_right());
    uart_putc('\n');
    led_toggle();
    delay_ms(100);
  }
}
