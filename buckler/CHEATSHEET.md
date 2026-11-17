# Buckler / nRF52 cheat sheet

What the boilerplate in `serial_hello/main.c` (and the course's display app) means.
Everything about the SDK macros below was read from the SDK headers on disk, so it matches the
version we build against (nRF5 SDK 15.3.0).

## Where the documentation is
The SDK on this machine has no docs folder and no examples folder. The best local docs are the
comments in the header files (the macro definitions are readable right there):

| What | File (under `~/buckler/software/nrf52x-base/sdk/nrf5_sdk_15.3.0/`) |
|---|---|
| Serial port library (`nrf_serial_*`) | `components/libraries/serial/nrf_serial.h` |
| SPI driver config struct | `modules/nrfx/drivers/include/nrfx_spi.h` |
| SPI modes and speeds | `modules/nrfx/hal/nrf_spi.h` |
| Buckler pin names | `~/buckler/software/boards/buckler_revC/buckler.h` |
| Display functions | `~/buckler/software/libraries/nhd_display/display.h` |
| Buckler schematic | `~/buckler/hardware/rev_c/buckler.pdf` |

Online: search for "nRF5 SDK v15.3.0" on Nordic's documentation site (sections "Serial interface"
and "SPI master driver"), and the "nRF52832 Product Specification" for register-level detail.
For the robot chip: TI's "MSP432P4xx Technical Reference Manual" (SLAU356) has the eUSCI UART chapter.

## Pins
`NRF_GPIO_PIN_MAP(port, pin)` turns a name like P0.08 into the single pin number the SDK wants.
The first number is the port. The nRF52832 only has port 0, so it is always `0`. The second is
the pin number: P0.08 is `NRF_GPIO_PIN_MAP(0, 8)`.

`LINK_SWAPPED` (in `serial_hello/main.c`) is a debugging switch I added. 0 = receive on P0.06 and
transmit on P0.08 (what the adapter wiring needs). 1 = the reverse. It would only matter if the
adapter crossed the two wires.

## The serial (UART) boilerplate
Five macros, each creating one object. Together they make a serial port you read and write with
`nrf_serial_read()` / `nrf_serial_write()`.

| Macro | Arguments | What it is |
|---|---|---|
| `NRF_SERIAL_DRV_UART_CONFIG_DEF(name, rx, tx, rts, cts, flow, parity, baud, irq)` | `rx`, `tx`: the pins. `rts`, `cts`: flow-control pins, unused so we pass 0. `flow`: `NRF_UART_HWFC_DISABLED` = no RTS/CTS wires. `parity`: `NRF_UART_PARITY_EXCLUDED` = none. `baud`: `NRF_UART_BAUDRATE_115200`. `irq`: interrupt priority, use `UART_DEFAULT_CONFIG_IRQ_PRIORITY`. | The settings of the wire: which pins, how fast. Must match the robot: 115200 baud, 8 data bits, no parity, 1 stop bit. |
| `NRF_SERIAL_QUEUES_DEF(name, tx_size, rx_size)` | Sizes of the send and receive FIFOs. We use 256 each. | Waiting rooms: incoming bytes pile up here until `nrf_serial_read()` takes them. If the program stops reading, the receive queue fills up. |
| `NRF_SERIAL_BUFFERS_DEF(name, tx_size, rx_size)` | Size of the DMA chunk per transfer. We use 1. | How many bytes the hardware moves per DMA transfer (one at a time here). |
| `NRF_SERIAL_CONFIG_DEF(name, mode, &queues, &buffers, ev_handler, sleep_handler)` | `mode`: `NRF_SERIAL_MODE_POLLING`, `_IRQ` or `_DMA`. The last two are optional callbacks, `NULL` for none. | Picks how the port works. We use DMA (the hardware copies bytes into memory by itself), same as the Kobuki code. |
| `NRF_SERIAL_UART_DEF(name, instance)` | `instance`: which UART peripheral, 0. | The serial port object itself, built from the pieces above. |

Then `nrf_serial_init(&serial, &uart_cfg, &config)` starts it.

Reading and writing:
- `nrf_serial_read(&serial, buf, size, &got, timeout_ms)`: waits up to `timeout_ms` for bytes.
  `got` is how many it actually read (can be NULL). `timeout_ms = 0` means do not wait at all.
  Returns `NRF_SUCCESS`, or `NRF_ERROR_TIMEOUT` if the time ran out first.
- `nrf_serial_write(&serial, data, size, &written, timeout_ms)`: same idea for sending.

### The three clock/timer lines (needed once, before `nrf_serial_init`)
```c
nrf_drv_clock_init();               // start the clock driver
nrf_drv_clock_lfclk_request(NULL);  // ask for the slow 32 kHz clock
app_timer_init();                   // start the software timer library
```
`nrf_serial` uses an app timer to implement its timeouts. Without these lines every
`nrf_serial_read()` returned immediately and read nothing. This is the bug that made the
display stick on "Waiting...". The `if (... != NRF_ERROR_MODULE_ALREADY_INITIALIZED)` wrappers
just ignore "already started".

## The SPI (display) boilerplate
SPI is the wire the display is on. Fields of `nrf_drv_spi_config_t`:

| Field | Meaning |
|---|---|
| `.sck_pin` | clock pin (`BUCKLER_LCD_SCLK`) |
| `.mosi_pin` | data going from the nRF to the display (`BUCKLER_LCD_MOSI`) |
| `.miso_pin` | data coming back from the display (`BUCKLER_LCD_MISO`); not really used here |
| `.ss_pin` | chip select (`BUCKLER_LCD_CS`): the driver pulls it low during each transfer |
| `.irq_priority` | interrupt priority; the default constant is fine |
| `.orc` | "over-run character": the byte sent if the transfer reads more than it writes. Unused, 0 |
| `.frequency` | clock speed: `NRF_DRV_SPI_FREQ_4M` = 4 MHz |
| `.mode` | clock polarity and phase. Mode 2 = clock idles low-active, data sampled on the leading edge. Must match what the display chip expects. It comes from the course's display example, so leave it |
| `.bit_order` | `MSB_FIRST`: the high bit of each byte goes first |

`NRF_DRV_SPI_INSTANCE(1)` means "use SPI hardware block number 1"; the chip has a few, and 1 is
the one the course's display app uses. Then:
```c
nrf_drv_spi_init(&spi_instance, &spi_config, NULL, NULL);  // NULL, NULL = no callback, blocking
display_init(&spi_instance);
display_write("text up to 16 chars", DISPLAY_LINE_0);      // DISPLAY_LINE_1 for the bottom row
```
`display_write` pads the row with spaces and refuses text longer than 16 characters.

## Flashing
- Robot (MSP432, LaunchPad USB): `cd ~/eecs149-ddr/msp432/drive && make APP=hello flash`
  (other apps: `wheeltest`, `square`).
- Buckler app (DK USB): `cd ~/eecs149-ddr/buckler/serial_hello && make flash`.
  Every terminal first needs `source ~/eecs149-ddr/buckler-blink/paths.txt`.
- `make flash` prints an error about loading a "SoftDevice" file. That is a known, harmless
  path bug; the app still flashes after it.

## Gotchas we already hit
- **Display shows an old program:** the DK's boot code at address 0 had been replaced by the
  course's original program, so the new app never started. A backup of the DK is in
  `~/eecs149-ddr/buckler-blink/dk_flash_backup_2026-11-15.bin`.
- **Wire directions:** the Buckler's `BUCKLER_UART_RX/TX` names are for the Kobuki. Our link is
  the reverse: nRF receives on P0.06, transmits on P0.08.
- **Do not open the DK's USB serial port** in a terminal program while the link is running; P0.06
  and P0.08 are shared with it.
- **The robot chip stays powered by USB or the Buckler**, so cycling only the chassis switch may
  not restart its program.
