# Troubleshooting

Each entry is something that actually happened to us. "Cause" is what we found; where we only
suspect something, it says so.

## The Buckler

| Symptom | Cause and fix |
|---|---|
| `make flash` finishes but the display still shows the old program ("Hello, Human!", or your old text). | The DK has no SoftDevice at address 0 (the course's original program is there instead), so your app at 0x26000 never starts. Restore the SoftDevice and the app together: [SETUP.md, "Restoring the SoftDevice"](SETUP.md). To check, read address 0: a SoftDevice starts with bytes `00 04 00 20`. |
| `make flash` prints `File is of unknown / unsupported format` and a `//` path. | Harmless path bug in the course makefile's SoftDevice step. Ignore it. It also means `make flash` can never install the SoftDevice. |
| `Cannot connect to the probe/programmer` | The nRF DK's own micro-USB is not plugged in. The LaunchPad's USB shows up as an "XDS110", which is a different thing. |
| Display stuck on "Waiting..." | The Buckler is running but receives nothing. Check the robot is running `demo` or `hello`, and the 4 jumper wires (P3.3 to P0.06 is the one that carries positions). Older versions of the app also got stuck because the clock and `app_timer` were never started (fixed: `simple_ble_init` starts them). |
| Display blinks or restarts when the USB cable is unplugged. | Not enough power on the chassis rail. Fresh batteries. |
| Top row shows text followed by `(not a POS line)`. | The robot is sending something other than `POS ...`, for example `HELLO` from the hello test. Flash `demo`. |
| Receiver shows "Listening..." forever. | It has heard no DDR Bluetooth packet. Is the driving robot's Buckler flashed with `coords` and receiving `POS` lines (row 0 should show `me x.. y..`)? Bluetooth has **not been tested between two real Bucklers yet**, so this may be a real bug: tell the team what each display shows. |

## The robot

| Symptom | Cause and fix |
|---|---|
| Robot goes a bit, stops, and the **chassis power light goes off**; pressing the power button again resumes. | The chassis supply cut out under motor load. For us it was **weak batteries**. The program on the LaunchPad keeps running the whole time, so it carries on from where it was when power returns. Fresh batteries. A USB port is not a power source for the motors. |
| Both encoder counts go **negative** while the wheels drive forward (`ERR stall`, `L-497 R-322`). | Encoder directions are flipped. Set `LEFT_ENC_SIGN` and `RIGHT_ENC_SIGN` to -1 in `encoder.c`. (Hand-spinning a wheel is an unreliable check: it is easy to spin it the wrong way. Use the demo's own readout.) |
| Sides are 2x too long and turns are about 190 degrees instead of 90 (or half of that). | `COUNTS_PER_REV` in `odom.c` is wrong for this robot. Calibrate it: [SETUP.md](SETUP.md). The factor you are off by is the same for distance and for turns. |
| Turn overshoots (we saw 98 degrees for 90). | The robot coasts after the motors stop. Raise `TURN_MARGIN_DEG`, or lower `TURN_SLOW_SPEED`, in `demo.c`. Too low a `TURN_SLOW_SPEED` can stall the turn: raise it a few points. |
| Final position does not return to (0, 0). | Normal sources: the turn margin, wheel slip, and the unverified wheel spacing (`TRACK_MM`). Calibrate the turn first. |
| LaunchPad LED blinks **fast** and the robot is dead. | A bump switch read as pressed, which stops everything until reset. Press RESET (or reflash). If it happens with nothing touching the bumper, a switch line may be flaky. |
| LED blinks slowly and the Buckler alternates two lines starting `ERR` and `LEG`. | A safety check failed: see the table in [HOW-IT-WORKS.md](HOW-IT-WORKS.md). |
| Nothing at all after `make flash`. | Is the chassis switched on? Check the OpenOCD output for `Verified OK`. |

## Things that wasted our time

- **Do not guess a robot's measurements. Measure them.** We were off by exactly 2x twice because a
  value taken from a datasheet or from the other robot did not apply.
- **Check power before code.** A robot that "stops for no reason" and a display that blinks were
  both low batteries, after we had changed a lot of code.
- Wires and pins are the unglamorous suspects: confirm the link with the `hello` test app before
  blaming the demo.
- zsh does not split an unquoted `$VAR` into words and errors on an unmatched glob such as `*.c`.
  If a command from these docs fails oddly in zsh, try it in `bash`.
