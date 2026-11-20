// Receiver, Buckler side. For the robot that does NOT drive.
//
// It listens for the driving robot's Bluetooth broadcasts (see ../common/ddr_payload.h) and
// shows the driving robot's position on the display. It does not need the robot underneath it
// to do anything: no wheels, no serial link, no LaunchPad program. It never transmits.
//
//     row 0:  R1 x300 y0           (the driving robot's position in mm, on the shared map)
//     row 1:  h90 cell 1,0         (its heading and grid cell; cell = position / CELL_MM)
//
// Before any packet has arrived the display says "Listening...". If nothing has been heard for
// about 1.5 seconds it says "No signal" and shows the last position it had.
//
// The position is on the SHARED MAP: the driving robot adds its own start position (set as
// MY_START_X_MM and MY_START_Y_MM in the coords app) before broadcasting.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_error.h"
#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_drv_spi.h"

#include "buckler.h"
#include "ddr_payload.h"
#include "display.h"
#include "simple_ble.h"

// One grid cell in millimetres; must match UNIT_MM in the robot's demo.c and CELL_MM in coords.
#define CELL_MM 300

#define LOOP_MS 50                  // how often the main loop checks for news
#define STALE_AFTER_MS 1500         // no packet for this long: show "No signal"

static simple_ble_config_t ble_config = {
  .platform_id       = 0x49,         // 4th byte of this Buckler's Bluetooth address
  .device_id         = 0x0DD0 + 9,   // last two bytes: different from the driving robots' (1, 2)
  .adv_name          = "DDR-RX",     // never sent: this app does not advertise
  .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),   // unused
  .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),     // unused: we never connect
  .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};
simple_ble_app_t* simple_ble_app;

// The newest packet. The Bluetooth callback fills it in, in interrupt context; the main loop
// picks it up in poll_heard(), so the display is only touched from main.
static ddr_payload_t heard;
static volatile bool heard_new = false;

// Called by the library for every advertisement any device sends (runs in interrupt context,
// so it only does the minimum): keep it if it is a DDR packet.
void ble_evt_adv_report(ble_evt_t const* p_ble_evt) {
  ble_gap_evt_adv_report_t const* report = &p_ble_evt->evt.gap_evt.params.adv_report;
  ddr_payload_t p;
  if (ddr_parse_adv(report->data.p_data, report->data.len, &p)) {
    heard = p;
    heard_new = true;
  }
}

// Position divided by the cell size, rounded to the nearest cell (also for negatives).
static int to_cell(int mm) {
  return (mm >= 0 ? mm + CELL_MM / 2 : mm - CELL_MM / 2) / CELL_MM;
}

static void show(const ddr_payload_t* p, bool stale) {
  char top[17], bottom[17];
  snprintf(top, sizeof top, "R%d x%d y%d", p->id, p->x, p->y);
  if (stale) {
    snprintf(bottom, sizeof bottom, "No signal");
  } else {
    snprintf(bottom, sizeof bottom, "h%d cell %d,%d", p->h, to_cell(p->x), to_cell(p->y));
  }
  display_write(top, DISPLAY_LINE_0);
  display_write(bottom, DISPLAY_LINE_1);
}

int main(void) {
  ret_code_t error_code = NRF_SUCCESS;

  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  // Bluetooth (this also starts the low-frequency clock and app_timer), then listen.
  simple_ble_app = simple_ble_init(&ble_config);
  advertising_stop();  // this app only listens
  scanning_start();

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

  display_write("Listening...", DISPLAY_LINE_0);
  display_write("for robot", DISPLAY_LINE_1);

  ddr_payload_t last = {0};
  bool have_last = false;
  bool showing_stale = false;
  int quiet_ms = 0;  // time since the last packet

  while (1) {
    ddr_payload_t p;
    bool fresh = false;
    CRITICAL_REGION_ENTER();
    if (heard_new) {
      p = heard;
      heard_new = false;
      fresh = true;
    }
    CRITICAL_REGION_EXIT();

    if (fresh) {
      last = p;
      have_last = true;
      quiet_ms = 0;
      showing_stale = false;
      show(&last, false);
    } else if (have_last) {
      quiet_ms += LOOP_MS;
      if (quiet_ms >= STALE_AFTER_MS && !showing_stale) {
        showing_stale = true;
        show(&last, true);
      }
    }

    nrf_delay_ms(LOOP_MS);
  }
}
