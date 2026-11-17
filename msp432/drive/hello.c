// Serial hello test, MSP432 side. Does NOT move the wheels.
//
// Once a second it sends a line like "HELLO 12 rx3" to the Buckler over the UART:
//   12  = how many hellos this program has sent
//   rx3 = how many lines it has received back from the Buckler so far
// LEDs on the LaunchPad:
//   red LED1 (P1.0)       toggles every second (heartbeat: the program is running)
//   green RGB LED (P2.1)  toggles each time a line arrives from the Buckler
//
// motor_init() is called only to keep the motor drivers asleep and the PWM off.

#include "board.h"
#include "motor.h"
#include "msp.h"
#include "uart.h"

static unsigned lines_received;

// Called continuously while waiting, so no incoming byte is missed.
static void poll_rx(void) {
  int c = uart_getc();
  if (c == '\n') {
    lines_received++;
    P2->OUT ^= BIT1;
  }
}

int main(void) {
  board_init();
  motor_init();
  uart_init();

  P2->DIR |= BIT0 | BIT1 | BIT2;
  P2->OUT &= ~(BIT0 | BIT1 | BIT2);

  for (unsigned n = 1;; n++) {
    uart_puts("HELLO ");
    uart_put_uint(n);
    uart_puts(" rx");
    uart_put_uint(lines_received);
    uart_putc('\n');

    led_toggle();
    delay_ms_poll(1000, poll_rx);
  }
}
