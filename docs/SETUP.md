# Setup and flashing

Written for macOS (Apple silicon) because that is what we developed on. The Linux lab desktop can
build, but it could not flash the nRF: that needs a udev rule for the J-Link probe, which needs sudo.

## 1. Get the code

Clone both repos into the **same parent folder**. The Buckler apps' Makefiles find the course
library with the relative path `../../../buckler/software`, so the course repo must be a sibling
of this one's parent. With the default names that is:

```
~/eecs149-ddr    (this repo)
~/buckler        (the course repo)
```

```bash
cd ~
git clone https://github.com/BistroP/ddr.git eecs149-ddr
git clone --recursive https://github.com/lab11/buckler.git
git -C buckler submodule update --init      # in case the submodule (nrf52x-base) did not come
```

Update later with `git pull` in `~/eecs149-ddr`.

## 2. Install the tools

| Tool | Used for | Version we used |
|---|---|---|
| Arm GNU toolchain (`arm-none-eabi-gcc`) | building both chips | 15.3.rel2 |
| OpenOCD | flashing the MSP432 | 0.12.0 (`brew install openocd`) |
| SEGGER J-Link software | flashing the nRF DK | V9.x, installed in `/Applications/SEGGER/JLink` |
| Python 3 | `buckler-blink/mergehex.py` | any |

We unpacked the Arm toolchain into `~/tools/`. If yours is somewhere else, edit
`buckler-blink/paths.txt`. Then, **in every new terminal**:

```bash
source ~/eecs149-ddr/buckler-blink/paths.txt
```

(It must be `source`d. Running the file does not change your terminal.)

## 3. Flash the MSP432 (the robot's brain)

Plug into the **LaunchPad's** micro-USB, with the chassis power switched on.

```bash
cd ~/eecs149-ddr/msp432/drive
make APP=demo flash           # build and flash one app
make APP=demo                 # build only
```

The program starts running as soon as it is flashed. To run it again without flashing, press the
LaunchPad's RESET button (not tried on every board).

| `APP=` | What it does |
|---|---|
| `demo` | The real program: 5 s countdown, then a 4-sided square (30 cm sides, right turns), reporting coordinates the whole time. |
| `hello` | Sends `HELLO n` lines to the Buckler. A link test. |
| `enctest` | Wheels not driven. Shows raw encoder counts: spin a wheel by hand. |
| `encdrive` | Drives straight, then turns, streaming raw counts. Isolates motor/encoder problems. |
| `wheeltest` | Each wheel, both wheels, backwards, spin, 2 s each. |
| `square` | An old timed (open-loop) square. Uncalibrated. |

Settings you may need to change are `#define`s at the top of the files:

| Setting | File | Meaning |
|---|---|---|
| `COUNTS_PER_REV` | `msp432/drive/odom.c` | Encoder counts per wheel turn. **Differs per robot** (see below). |
| `DRIVE_SPEED`, `TURN_SPEED` | `msp432/drive/demo.c` | Motor power in percent. Lower is slower and easier to film. |
| `USE_ESP` | `msp432/drive/demo.c` | 0 = no ESP32 attached (leave 0). |
| `LEFT_ENC_SIGN`, `RIGHT_ENC_SIGN` | `msp432/drive/encoder.c` | Flip if a count falls while the wheel drives forward. |

### Calibrating a new robot (do this once per robot)

Distances and angles come from counting encoder edges, so `COUNTS_PER_REV` must match the robot:
robot 1 measured **360**, robot 2 measured **720**. With the wrong value the robot drives the
wrong distance and turns the wrong angle by the same factor (ours drove 60 cm instead of 30 and
turned about 190 degrees instead of 90 with 720 set on the 360 robot).

1. Run `demo` and measure how far the first side really goes (it should be 30 cm).
2. Real distance divided by 30 cm is the factor you are off by. **Divide** `COUNTS_PER_REV` by
   it (a side that is 60 cm long gives a factor of 2, so halve the number; a side that is
   15 cm gives 0.5, so double it).
3. Re-run and check the turns are about 90 degrees. If turns are still off but distances are
   right, the wheel spacing `TRACK_MM` (141 mm, taken from the Pololu Romi and **not verified**
   on this chassis) is the next suspect.

## 4. Flash the Buckler (the nRF DK)

Plug into the **nRF DK's own** micro-USB (not the LaunchPad's). Set the per-robot settings at the
top of the app's `main.c` first.

```bash
cd ~/eecs149-ddr/buckler/coords      # the driving robot's Buckler
make flash
cd ~/eecs149-ddr/buckler/receiver    # a robot that only receives and displays
make flash
```

Settings in `buckler/coords/main.c`:

| Setting | Meaning |
|---|---|
| `ROBOT_ID` | 1 on one driving robot, 2 on the other. |
| `MY_START_X_MM`, `MY_START_Y_MM` | Where this robot starts on the shared map, in mm. Both robots must start facing the same way. |
| `BENCH_TEST` | 1 = invent a position so two Bucklers can test Bluetooth with no robots. 0 for normal use. |

`make flash` prints a scary error ending in `File is of unknown / unsupported format` near the
SoftDevice step. That is a harmless path bug in the course makefile; ignore it.

### Restoring the SoftDevice (the trap that cost us hours)

Our apps are linked to run **on top of Nordic's SoftDevice** (s132) at address 0x26000. If a DK
still has the course's original program at address 0, then `make flash` succeeds, the app sits at
0x26000, and it **never starts**: the display keeps showing the old program. This happened on
both of our DKs. `make flash` cannot fix it. Restore the SoftDevice and the app together:

```bash
source ~/eecs149-ddr/buckler-blink/paths.txt
cd ~/eecs149-ddr/buckler/coords && make                     # build the app first
SD=$(find ~/buckler -name "s132_nrf52_6.1.1_softdevice.hex" | head -1)
python3 ~/eecs149-ddr/buckler-blink/mergehex.py "$SD" _build/coords_sdk15_s132.hex /tmp/merged.hex

# Optional but wise: back up the whole chip first (this restore ERASES it).
printf 'connect\nsavebin /tmp/dk_backup.bin 0x0 0x80000\nexit\n' > /tmp/backup.jlink
JLinkExe -device nRF52832_xxAA -if SWD -speed 4000 -autoconnect 1 -NoGui 1 -CommanderScript /tmp/backup.jlink

printf 'connect\nerase\nloadfile /tmp/merged.hex\nr\ng\nexit\n' > /tmp/restore.jlink
JLinkExe -device nRF52832_xxAA -if SWD -speed 4000 -autoconnect 1 -NoGui 1 -CommanderScript /tmp/restore.jlink
```

For `receiver`, use `_build/receiver_sdk15_s132.hex` in the merge step. After this, plain
`make flash` keeps working on that DK until someone flashes a course app that overwrites address 0.

## 5. Wiring between the LaunchPad and the Buckler

Four jumper wires through the adapter. Both sides are 3.3 V, 115200 baud, 8N1.

| LaunchPad | Buckler (nRF) | Direction |
|---|---|---|
| P3.3 (UCA2TXD) | P0.06 | robot to Buckler (positions) |
| P3.2 (UCA2RXD) | P0.08 | Buckler to robot (unused so far) |
| 3.3 V | J8 VCC | |
| GND | GND | |

Note the nRF receives on P0.06, the reverse of the Kobuki library's RX/TX names. Do **not** open
the DK's USB serial port in a terminal: it shares those two pins.

## 6. Safety and power

- **Remove the +5 V jumper on the LaunchPad's J101** when it sits on the chassis. TI warns it can
  damage the board when the chassis battery is powering it. Ours is removed.
- Run the chassis from **good batteries**. Weak batteries were the cause of most "the robot just
  stops" problems we chased (see [TROUBLESHOOTING.md](TROUBLESHOOTING.md)).
- Put the robot on blocks the first time you run any new motor code.
- Any bump switch stops the robot and blinks the LaunchPad LED fast until you reset it.

## 7. Running the Bluetooth demo

You need the **driving robot** and, optionally, a second Buckler that only receives.

1. Driving robot: flash `msp432/drive` with `APP=demo`, and `buckler/coords` with `ROBOT_ID 1`.
2. Receiving Buckler: flash `buckler/receiver`. Its LaunchPad needs no program.
3. Power both and keep them within a few metres. The driving robot's display shows
   `me x.. y..` on row 0. The receiver shows `R1 x.. y..` on row 0 and heading/cell on row 1.

No robots? Set `BENCH_TEST 1` in `buckler/coords/main.c`, set a different `ROBOT_ID` on each of
two Bucklers, and flash both. Each invents a position and shows the other's on row 1.
