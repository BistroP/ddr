# How it works

## Coordinates

- Units are **millimetres**. One grid cell is 300 mm (`UNIT_MM` in `demo.c`, `CELL_MM` on the
  Buckler; keep them equal).
- **x to the right, y up.** Heading is in degrees, **clockwise from +y** (0 = the way the robot
  started, 90 = a right angle clockwise from that).
- Every robot starts at **(0, 0)** facing 0. The square's corners are therefore cells (0,1), (1,1),
  (1,0), (0,0).
- Two robots only agree on a map if each is told where it started: see "The shared map" below.

## Where the robot thinks it is (odometry)

The robot has no position sensor. It counts wheel-encoder edges and does dead reckoning.

- **`encoder.c`**: each wheel has two Hall-effect signals, A and B, a quarter turn out of phase.
  An interrupt fires on **every edge of B** (port 5). In the interrupt we read A: whether A equals
  B tells us which way the wheel turned, so the count goes up or down. `LEFT_ENC_SIGN` /
  `RIGHT_ENC_SIGN` flip it if "forward" counts the wrong way.
- **`odom.c`**: every 20 ms it turns the change in counts into millimetres
  (`mm per count = pi x wheel diameter / COUNTS_PER_REV`), then:
  - distance moved = average of the two wheels,
  - turn = (left distance - right distance) / wheel spacing, clockwise positive,
  - position moves along the average heading of that small step.
- Three numbers decide how true this is: wheel diameter 70 mm, wheel spacing `TRACK_MM` 141 mm
  (from the Pololu Romi, **not verified** on this chassis), and `COUNTS_PER_REV` (**measured per
  robot**). Wheel slip also adds error, so what the display shows is what the robot *believes*.

## Driving the square (`msp432/drive/demo.c`)

For each of 4 sides: drive one cell, then turn right 90 degrees. Everything runs from one loop
that ticks once a millisecond and also updates odometry (every 20 ms) and reports position
(every 100 ms).

- **A straight side** ends when the robot has covered 300 mm *along the side's heading*. It
  **steers**: every 20 ms the heading error becomes a small power difference between the wheels
  (`STEER_GAIN`, `STEER_MAX_PCT`), so the two motors do not need to match exactly. Each side
  aims at an **absolute** heading (0, 90, 180, 270), so a turn that lands a few degrees off is
  pulled back during the next side instead of carrying on.
- **A turn** also aims at an absolute heading (90, 180, 270, 360). It slows down for the last
  24 degrees (`TURN_SLOW_ZONE_DEG`, `TURN_SLOW_SPEED`) and stops `TURN_MARGIN_DEG` early, because
  the robot still coasts a little after the motors stop.
- **Pauses** of 1.5 s (`SETTLE_MS`) after every move let you read the display on video.
- **Safety checks** stop the robot and report a reason if the encoders disagree with the motors
  (see below). Each step also has a long timeout as a safety net, and a **bump switch** stops
  everything (the LaunchPad LED then blinks fast until you reset).

### Failure messages

On a failure the robot stops and alternates two lines on the Buckler, 1.5 s each:

```
ERR <reason> h<heading>|L<left count> R<right count>      e.g.  ERR stall h0 / L0 R0
LEG <ms>ms|L<left> R<right> p<mm covered>                 how the last side ended
```

| Reason | Meaning |
|---|---|
| `stall` | A side made under 10 mm of progress in its first 0.8 s. Motors not driving, wheels blocked, or encoders dead. Counts both negative = encoder signs wrong. |
| `drift` | A "straight" side turned more than 25 degrees. One encoder is wrong or one side is not driving. |
| `noturn` | A turn had turned under 5 degrees after 0.8 s. |
| `wrong` | A turn went 15 degrees the wrong way. |

## Messages between the parts

**Robot to Buckler (serial, text lines, 115200 8N1):**

```
POS <x mm> <y mm> <heading deg>     10 times a second, from demo.c
ERR ... / LEG ...                   after a failure, as above
HELLO n ...                         from the hello test app
ENC <left> <right>                  from enctest
```

The Buckler shows anything it does not recognise as plain text, and splits a line at `|` into
two rows.

**Buckler to Buckler (Bluetooth Low Energy advertisement, no pairing):** the Buckler puts an
11-byte packet into the "manufacturer specific data" of its advertisement and sends it about
3 times a second (every 3rd position line):

| Bytes | Field |
|---|---|
| 0-2 | tag `D` `D` `R` (so other people's devices are ignored) |
| 3 | robot id |
| 4-5 / 6-7 / 8-9 | x / y / heading, little-endian signed 16-bit |
| 10 | counter that goes up by one per packet |

The format and parser live in `buckler/common/ddr_payload.h` and are used by both Buckler apps.
The parser was unit-tested on a Mac with valid, truncated and corrupted advertisements. The
Bluetooth *link* itself has **not** been tested between two real Bucklers yet.

**Display (`buckler/coords`):**

```
row 0:  me x300 y0           our position
row 1:  R2 x600 y0           the other robot's, while we keep hearing it
        h90 cell 1,0         otherwise: our heading and grid cell
```

`buckler/receiver` shows `R1 x.. y..` / `h.. cell ..,..`, and "No signal" after 1.5 s of silence.

## The shared map

Each robot counts its position from where it **started**. To compare two robots, put them on one
map: set `MY_START_X_MM` / `MY_START_Y_MM` in `buckler/coords/main.c` to where that robot starts
on the shared map (for example robot 2 starts 600 mm to the right of robot 1: 600, 0). A robot
broadcasts *start + own position*, and the receiver subtracts *its own* start before showing it,
so each display reads in that robot's own frame. Both robots must start facing the same way.

## Code map

| File | Job |
|---|---|
| `msp432/drive/board.c` | Clock (3 MHz), LED, bump switches, millisecond delay. |
| `msp432/drive/motor.c` | PWM motor control, power cap (60%), emergency stop. |
| `msp432/drive/encoder.c` | Interrupt-driven wheel counting. |
| `msp432/drive/odom.c` | Counts to x, y, heading. |
| `msp432/drive/uart.c` | Serial port to the Buckler (P3.2/P3.3). |
| `msp432/drive/esp.c` | Serial port to an ESP32 (P2.2/P2.3); off unless `USE_ESP 1`. |
| `msp432/drive/demo.c` | The square, the safety checks, position reports. |
| `buckler/coords/main.c` | Display, Bluetooth broadcast and listen, serial link to the robot. |
| `buckler/receiver/main.c` | Listen-only display. |
| `esp32/robot_radio/robot_radio.ino` | Fallback radio over Wi-Fi (ESP-NOW). Never run. |

More on the nRF SDK boilerplate in `buckler/CHEATSHEET.md`.
