// Coordinate display, Buckler side.
//
// The robot (MSP432) sends its estimated position as text lines over the UART, e.g.
//     POS 300 0 90      (x in mm, y in mm, heading in degrees clockwise from "up")
// This app shows it on the display:
//     row 0:  me x300 y0           (our position in mm)
//     row 1:  R2 x600 y0           (the OTHER robot's position, once its radio line arrives)
//             or, when we are not hearing the other robot:
//             h90 cell 1,0         (cell = position divided by CELL_MM, rounded)
// The other robot's position arrives as a line like  OTHER 2 600 0 90  (id, x, y, heading).
// Any other line the robot sends (for example "HELLO 5 rx0" or "ENC 120 -3") is shown as it
// is on row 0, which is handy for testing. If such a line contains a "|", the text before it
// goes on row 0 and the text after it on row 1, e.g. the robot's failure report
//     ERR drift h25|L412 R300      ->   row 0: "ERR drift h25"   row 1: "L412 R300"
// (each row shows at most 16 characters).
//
// Wiring (through the adapter):  MSP432 P3.3 (TX) -> nRF P0.06
//                                nRF P0.08 -> MSP432 P3.2 (RX)
// The nRF receives on P0.06. This app only listens; it does not send anything yet.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#define LINK_RX_PIN NRF_GPIO_PIN_MAP(0, 6)  // receives what the robot transmits
#define LINK_TX_PIN NRF_GPIO_PIN_MAP(0, 8)  // unused for now, but part of the UART setup

// One grid cell in millimetres; must match UNIT_MM in the robot's demo.c.
#define CELL_MM 300

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

// Position divided by the cell size, rounded to the nearest cell (also for negatives).
static int to_cell(int mm) {
  return (mm >= 0 ? mm + CELL_MM / 2 : mm - CELL_MM / 2) / CELL_MM;
}

// Our own latest position, and the latest position of the other robot (it arrives over the
// radio as "OTHER id x y heading"). Positions are in mm.
static int own_x, own_y, own_h;
static int other_id, other_x, other_y;
// How many of our own POS lines have arrived since the last OTHER line. The robot sends about
// 10 POS lines a second, so more than OTHER_FRESH_POS_LINES means we have not heard from the
// other robot for about 1.5 s and its position is out of date.
#define OTHER_FRESH_POS_LINES 15
#define NEVER_HEARD 1000
static int other_age = NEVER_HEARD;

// Row 0: our position.  Row 1: the other robot's position while we are hearing it, otherwise
// our heading and cell.
static void redraw(void) {
  char top[17], bottom[17];
  snprintf(top, sizeof top, "me x%d y%d", own_x, own_y);
  if (other_age <= OTHER_FRESH_POS_LINES) {
    snprintf(bottom, sizeof bottom, "R%d x%d y%d", other_id, other_x, other_y);
  } else {
    snprintf(bottom, sizeof bottom, "h%d cell %d,%d", own_h, to_cell(own_x), to_cell(own_y));
  }
  display_write(top, DISPLAY_LINE_0);
  display_write(bottom, DISPLAY_LINE_1);
}

static void show_line(const char *line) {
  int x, y, h, id;
  char top[17], bottom[17];
  if (sscanf(line, "POS %d %d %d", &x, &y, &h) == 3) {
    own_x = x;
    own_y = y;
    own_h = h;
    if (other_age < NEVER_HEARD) {
      other_age++;
    }
    redraw();
  } else if (sscanf(line, "OTHER %d %d %d %d", &id, &x, &y, &h) == 4) {
    other_id = id;
    other_x = x;
    other_y = y;
    other_age = 0;
    redraw();
  } else {
    const char *bar = strchr(line, '|');
    if (bar != NULL) {
      // "%.*s" copies only the text before the bar; both rows are cut to 16 characters.
      snprintf(top, sizeof top, "%.*s", (int)(bar - line), line);
      snprintf(bottom, sizeof bottom, "%s", bar + 1);
      display_write(top, DISPLAY_LINE_0);
      display_write(bottom, DISPLAY_LINE_1);
    } else {
      snprintf(top, sizeof top, "%s", line);  // truncated to 16 characters
      display_write(top, DISPLAY_LINE_0);
      display_write("(not a POS line)", DISPLAY_LINE_1);
    }
  }
}

int main(void) {
  ret_code_t error_code = NRF_SUCCESS;

  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  // nrf_serial implements its read/write timeouts with app_timer, which needs the
  // low-frequency clock. Without these calls every nrf_serial_read() fails at once.
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
  display_write("for the robot", DISPLAY_LINE_1);

  char line[48];
  size_t line_len = 0;

  while (1) {
    // Wait up to 20 ms for one byte from the robot.
    char c;
    size_t got = 0;
    ret_code_t r = nrf_serial_read(&ddr_serial, &c, 1, &got, 20);
    if (r == NRF_SUCCESS && got == 1) {
      if (c == '\n') {
        line[line_len] = '\0';
        if (line_len > 0) {
          show_line(line);
        }
        line_len = 0;
      } else if (c != '\r' && line_len < sizeof line - 1) {
        line[line_len++] = c;
      }
    }
  }
}
