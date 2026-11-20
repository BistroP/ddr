# ddr: two robots for Dance Dance Revolution (EECS 149/249)

Two TI-RSLK MAX robots (MSP432 LaunchPad on the chassis, with a Berkeley Buckler stacked on
top) drive around a grid of cells. Each robot works out where it is from its wheel encoders,
shows that on the Buckler's display, and broadcasts it over Bluetooth so the other robot can show
it too.

- Milestone 2 (due 2026-12-03): one robot moves to the left/right/up/down arrows and learns the
  other robot's coordinates.
- Final report due 2026-12-18.

## Where things stand (2026-11-20)

| Piece | State |
|---|---|
| Drive a square with odometry (`msp432/drive`, app `demo`) | Works on the driving robot, tested on hardware. Still being tuned (turn overshoot, closing error). |
| Robot to Buckler serial link and display | Works. |
| Bluetooth: broadcast own position and show the other robot's (`buckler/coords`, `buckler/receiver`) | **Built, not yet tested on hardware.** |
| ESP32 radio (`esp32/`, `msp432/drive/esp.c`) | Written, **never run**. Switched off (`USE_ESP 0`): the borrowed board drew too much power. Kept as a fallback. |
| Buckler sending commands to the robot | Not built. |
| mmWave radar, moving to a given arrow | Not in this repo yet. |

## The hardware stack, bottom to top

```
  Berkeley Buckler revC (16x2 display)         <- shows positions, does Bluetooth
  nRF52-DK                                     <- the Buckler's brain (nRF52832)
  "UCB Buckler / TI LaunchPad adapter"
  MSP-EXP432P401R LaunchPad (MSP432P401R)      <- drives the wheels, reads the encoders
  TI-RSLK MAX chassis (motors, encoders, bump switches, battery pack)
```

Two chips, two different programming paths (details in [docs/SETUP.md](docs/SETUP.md)):

- **MSP432** (robot brain): programmed through the LaunchPad's own micro-USB, with OpenOCD.
- **nRF52832** (Buckler): programmed through the nRF DK's own micro-USB, with SEGGER J-Link.

## What is in the repo

| Folder | What |
|---|---|
| `msp432/drive/` | Robot firmware: motors, encoders, odometry, and the apps (`demo` is the real one). |
| `msp432/blink/`, `msp432/vendor/` | LED blink test; TI/CMSIS headers, startup file and linker script. |
| `buckler/coords/` | Buckler app for the **driving** robot: shows own position, broadcasts it, shows the other robot's. |
| `buckler/receiver/` | Buckler app for a robot that only **receives and displays** (no driving). |
| `buckler/common/` | The Bluetooth packet format, shared by both Buckler apps. |
| `buckler/serial_hello/` | Link test: prints the robot's `HELLO` lines. |
| `buckler/CHEATSHEET.md` | What the nRF SDK boilerplate in these apps means. |
| `esp32/robot_radio/` | ESP32 (Arduino) radio sketch: the unused fallback. |
| `buckler-blink/` | `paths.txt` (puts the compiler and J-Link on your PATH) and `mergehex.py`. |
| `docs/` | Setup, how it works, troubleshooting. |

## Quick start

```bash
cd ~
git clone https://github.com/BistroP/ddr.git eecs149-ddr
git clone --recursive https://github.com/lab11/buckler.git     # the course repo, a SIBLING folder
```

Then install the tools and flash: [docs/SETUP.md](docs/SETUP.md). To understand the code:
[docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md). When something goes wrong:
[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

The commands you will use most:

```bash
source ~/eecs149-ddr/buckler-blink/paths.txt                            # once per terminal
cd ~/eecs149-ddr/msp432/drive && make APP=demo flash                    # robot: LaunchPad USB
cd ~/eecs149-ddr/buckler/coords && make flash                           # driving robot's Buckler: DK USB
cd ~/eecs149-ddr/buckler/receiver && make flash                         # receiving Buckler: DK USB
```

## Working together in git

- `git pull` before you start, and commit small and often.
- Do not commit build output (`build/`, `_build/`) or flash dumps; `.gitignore` covers them.
- Per-robot settings (`ROBOT_ID`, start position, counts per wheel turn) are edited in the source
  and differ from robot to robot, so say in your commit message if you change one.
