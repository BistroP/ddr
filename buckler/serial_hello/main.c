// Serial hello test, Buckler side.
//
// Listens on the UART link to the MSP432 (robot) and shows what arrives on the display:
//   row 0: the last line received, e.g. "HELLO 12 rx3"
//   row 1: counters, "rx <lines received> tx <pings sent>"
// About every 2 seconds it sends "PING\n" back to the robot, whose green LED toggles on it.
//
// Wiring (through the adapter):  MSP432 P3.3 (TX) -> nRF P0.06
//                                nRF P0.08 -> MSP432 P3.2 (RX)
// So the nRF receives on P0.06 and transmits on P0.08. That is the OPPOSITE of the
// BUCKLER_UART_RX / BUCKLER_UART_TX names (made for the Kobuki). If nothing ever shows up
// on the display, set LINK_SWAPPED to 1 below and flash again.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "app_error.h"
#include "app_timer.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_serial.h"
#include "nrfx_gpiote.h"
#include "nrf_drv_spi.h"

#include "buckler.h"
#include "display.h"

#define LINK_SWAPPED 0 // What is LINK_SWAPPED?

#if LINK_SWAPPED
#define LINK_RX_PIN NRF_GPIO_PIN_MAP(0, 8) 
#define LINK_TX_PIN NRF_GPIO_PIN_MAP(0, 6)
#else
#define LINK_RX_PIN NRF_GPIO_PIN_MAP(0, 6) // Receiving pin
#define LINK_TX_PIN NRF_GPIO_PIN_MAP(0, 8) // Transmitting pin. What is the first number passed in?
#endif

// UART 115200 8N1, no flow control. Names are prefixed so they cannot clash with the
// Kobuki library's own copies of these objects.
NRF_SERIAL_DRV_UART_CONFIG_DEF(ddr_uart_cfg,
                               LINK_RX_PIN, LINK_TX_PIN,
                               0, 0,
                               NRF_UART_HWFC_DISABLED, NRF_UART_PARITY_EXCLUDED,
                               NRF_UART_BAUDRATE_115200,
                               UART_DEFAULT_CONFIG_IRQ_PRIORITY);
NRF_SERIAL_QUEUES_DEF(ddr_queues, 256, 256);
NRF_SERIAL_BUFFERS_DEF(ddr_buffs, 1, 1);
NRF_SERIAL_CONFIG_DEF(ddr_config, NRF_SERIAL_MODE_DMA, &ddr_queues, &ddr_buffs, NULL, NULL);
NRF_SERIAL_UART_DEF(ddr_serial, 0);

#define PING_EVERY_LOOPS 100  // each idle loop waits 20 ms, so this is about 2 seconds

int main(void) {
  ret_code_t error_code = NRF_SUCCESS;

  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  // nrf_serial implements its read/write timeouts with app_timer, which needs the
  // low-frequency clock. Without these three calls every nrf_serial_read() fails at once
  // and nothing is ever read from the UART (same setup as kobukiInit()).
  error_code = nrf_drv_clock_init();
  if (error_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
    APP_ERROR_CHECK(error_code);
  }
  nrf_drv_clock_lfclk_request(NULL);
  error_code = app_timer_init();
  if (error_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
    APP_ERROR_CHECK(error_code);
  }

  // Display (same setup as the display app)
  nrf_drv_spi_t spi_instance = NRF_DRV_SPI_INSTANCE(1);
  nrf_drv_spi_config_t spi_config = {
    .sck_pin = BUCKLER_LCD_SCLK,
    .mosi_pin = BUCKLER_LCD_MOSI,
    .miso_pin = BUCKLER_LCD_MISO,
    .ss_pin = BUCKLER_LCD_CS,
    .irq_priority = NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
    .orc = 0,
    .frequency = NRF_DRV_SPI_FREQ_4M,
    .mode = NRF_DRV_SPI_MODE_2,
    .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
  };
  error_code = nrf_drv_spi_init(&spi_instance, &spi_config, NULL, NULL);
  APP_ERROR_CHECK(error_code);
  display_init(&spi_instance);

  // Serial link to the robot
  error_code = nrf_serial_init(&ddr_serial, &ddr_uart_cfg, &ddr_config);
  APP_ERROR_CHECK(error_code);

  display_write("Waiting...", DISPLAY_LINE_0);
  display_write("rx 0 tx 0", DISPLAY_LINE_1);

  char line[17];
  size_t line_len = 0;
  unsigned rx_lines = 0, tx_pings = 0, loops = 0;

  while (1) {
    // Wait up to 20 ms for one byte from the robot.
    char c;
    size_t got = 0;
    ret_code_t r = nrf_serial_read(&ddr_serial, &c, 1, &got, 20);
    if (r == NRF_SUCCESS && got == 1) {
      if (c == '\n') {
        line[line_len] = '\0';
        display_write(line, DISPLAY_LINE_0);
        line_len = 0;
        rx_lines++;
        char counts[17];
        snprintf(counts, sizeof counts, "rx %u tx %u", rx_lines, tx_pings);
        display_write(counts, DISPLAY_LINE_1);
      } else if (c != '\r' && line_len < 16) {
        line[line_len++] = c;
      }
    }

    // Every ~2 seconds send a ping back so the robot side can show it got something.
    if (++loops >= PING_EVERY_LOOPS) {
      loops = 0;
      nrf_serial_write(&ddr_serial, "PING\n", 5, NULL, 100);
      tx_pings++;
    }
  }
}
