# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Lego Duplo Train R/C (the repository is M5LegoDuploTrain,
https://github.com/mbogatyr/M5LegoDuploTrain): firmware that turns an
M5StickS3 into a Bluetooth LE remote for the LEGO DUPLO Cargo Train (set
10875, the set it was tested with), replacing the smartphone app. It
finds the locomotive and connects to it. Tilting the stick sets the speed and
direction, KEY1 starts and stops the train, KEY2 plays sounds and changes the
LED colour, a shake sounds the horn. The screen is made for a small child.

## Language

All documentation and the project itself are kept in English: this file, code
comments, identifiers, strings shown on the display, commit messages and any
other text files. New text is written in English too, even when the
conversation with the user happens in another language.

## Commands

PlatformIO is not installed globally but by the official installer into a
venv. The binary lives at `~/.platformio/penv/bin/pio`; it is not on `PATH`,
so it has to be called by its full path.

```bash
~/.platformio/penv/bin/pio test -e native                         # host unit tests for the logic
~/.platformio/penv/bin/pio test -e native -f test_train_controls  # a single test suite
~/.platformio/penv/bin/pio run -e sticks3                         # build the firmware
~/.platformio/penv/bin/pio run -e sticks3 -t upload               # flash the board
~/.platformio/penv/bin/pio run -e sticks3 -t merged               # single image for M5Burner
~/.platformio/penv/bin/pio device monitor -e sticks3              # serial monitor, 115200
```

The xtensa-esp32s3 toolchain is installed into `~/.platformio/packages` once
per machine (about eight minutes) and is shared by all projects. The first
build of a new project downloads the latest M5Unified into `.pio/` and takes
about 20 seconds.

`upload_port` and `monitor_port` are pinned to `/dev/cu.usbmodem*`: when the
board is not on USB, PlatformIO otherwise picks
`/dev/cu.Bluetooth-Incoming-Port` and fails with "No serial data received".

`Serial.begin(115200)` must be called in `setup()`: M5Unified's
`serial_baudrate` defaults to 0, and without it the port stays silent.

`build_unflags = -std=gnu++11` is needed for `-std=gnu++17` to take effect:
the Arduino framework appends its own `-std=gnu++11` after `build_flags`, and
the last one wins. Without it the board builds as C++11 while the host tests
build as C++17, and `static constexpr` array members fail to link
(`undefined reference to TrainControls::kLevelPower`).

## Architecture

The split into `lib/` and `src/` is not cosmetic here, it is load-bearing:

- `lib/` is the logic: plain C++ with no Arduino, no M5Unified, no NimBLE and
  no hardware access of any kind.
  - `DuploProtocol`: the part of the LEGO Wireless Protocol the train needs.
    Builds commands as byte arrays (`duplo::Message`) and recognises the
    train's advertisement.
  - `TrainControls`: running or standing, the speed level, the sound and
    light cycles. Every method returns a `Command` saying what to send.
  - `TiltThrottle`: accelerometer readings to a speed level -5..5, with a
    neutral pose, a dead zone and hysteresis.
  - `ShakeDetector`: a jolt above 2.2 g, at most one per 1.2 s.
  - `Repeats`: when to send a command again (the brake after a stop, the
    LED colour after connecting): 0.2, 0.6, 1.5 and 3 s later, then no more.
  - `DisplayTimeout`: turns the display off after 3 minutes without activity.
- `src/` is everything that knows about the board:
  - `TrainLink`: the NimBLE-Arduino client. Scans for the train, connects,
    sends messages, reconnects after losing the train.
  - `Renderer`: draws on the display.
  - `main.cpp`: wires the logic to the buttons, the accelerometer, the link
    and the display; keeps the neutral pose in NVS (`Preferences`, namespace
    `train`, key `neutral`); takes serial commands.

The `native` environment builds only `lib/` (PlatformIO's `test_build_src`
defaults to `no`), so the logic is tested on the Mac without the board.
**Do not pull hardware dependencies into `lib/`: that breaks the tests.**

### Time is passed in as a parameter

The logic in `lib/` does not call `millis()` itself; it receives the current
time as an argument. That way tests can substitute any moment without waiting,
and the logic has no `delay()`: `loop()` runs at 50 Hz and just passes
`millis()` along.

`millis()` overflows after roughly 49 days. Compute intervals with unsigned
subtraction `now - since`, so the overflow goes unnoticed.

### Rendering

`Renderer` composes the whole frame in an `M5Canvas` (a sprite in PSRAM) and
pushes it with a single `pushSprite`. Drawing directly on the screen causes
visible flicker.

`Renderer::draw()` takes a `Renderer::Screen`, compares it with the previous
frame and returns if nothing has changed. `invalidate()` clears that memory;
`main.cpp` calls it when the display wakes up, because the panel's contents
are lost during sleep.

The layout (chosen by the user from three mockups, variant C): a 4-pixel
frame in the colour of the train's LED (dark grey for Off), five chevrons up
for forward and five down for backward around the centre, a dot or a word in
the centre, the connection dot in the top left corner. Unlit chevrons are dark
grey; lit ones are outlined while the train stands (white when connected, grey
without a train) and filled while it runs (green forward, orange backward).
At power-on a splash screen takes the place of the chevrons: "Lego Duplo" /
"Train R/C", then one line per control (key in yellow on the left, action in
white on the right), "Press a key" at the bottom, the connection dot in its
corner as usual. It ends at the first key press or after 15 s
(`kSplashMs`); the search for the train runs behind it.

**Nothing may touch the frame**: the farthest chevron ends 12 pixels short of
its inner edge at the top and 11 at the bottom; the arithmetic is next to the
constants in `Renderer.cpp`. Check any layout change with a screenshot.

### Bluetooth runs in its own task

NimBLE calls the scan, disconnect and notification callbacks from its host
task. `TrainLink` callbacks only store into atomics; connecting, discovering
the service and writing happen in `TrainLink::update()` and `send()`, called
from `loop()`. Connecting blocks `loop()` for up to the 5 s connect timeout;
`update()` first switches to `Connecting` and returns, so the screen shows
it before the blocking call.

## Controls

The stick is held upright in portrait, screen towards the driver.

| Input | Action |
|---|---|
| Tilt the top edge away / towards you | Speed level forward / backward, up to 5 each way |
| KEY1 (`M5.BtnA`) | Start the train at the tilted level, or stop it (with the brake sound) |
| KEY2 once (`M5.BtnB`) | Next sound: steam, departure, water refill |
| KEY2 twice | Next LED colour: off, red, blue, green, white |
| KEY2 held for 1.5 s | The current grip becomes neutral (saved in NVS) |
| Shake | Horn |

- The tilt always shows on the screen. While the train stands it is only a
  preview of how KEY1 would start it; while it runs, the train follows it,
  through zero into reverse without stopping the run.
- Levels: a dead zone of 6° either side of neutral, then a level every 6°,
  with a quarter-level hysteresis. Motor power per level is
  `TrainControls::kLevelPower` = 0/30/45/60/80/100 %; the train does not move
  below about 25 % (from Pybricks' DUPLO project).
- The default neutral is 55° from upright, towards the screen facing up:
  while driving, the user's grip measured 50-62° (`acc`, 2026-10-01). Lying
  flat on a table is about +35° from it, so full forward.
- KEY2 clicks are decided 300 ms after the release (`setHoldThresh(300)`, the
  same value M5Unified uses as the double press window). A hold produces no
  click.
- A running train keeps the display on; otherwise the first press of KEY1
  would only wake the display instead of stopping the train. With the display
  off, the first press of either key only wakes it.
- On every connection the LED is turned off (and the frame is grey), so the
  screen and the train start out alike; the first double press of KEY2 makes
  it red. Running resets to standing on connecting and on losing the train.
- The press that ends the splash screen does nothing else, like the press
  that wakes the display.
- The name of a sound or colour shows in the centre for 1.2 s.
- The horn is only on the shake and the brake sound only on KEY1's stop, so
  neither is in KEY2's cycle.

## The DUPLO train protocol

The locomotive (DUPLO Train Base) speaks the LEGO Wireless Protocol 3.0, like
the other Powered Up hubs. The bytes come from the LWP3 documentation and two
libraries that drive this train: Legoino (`Lpf2Hub.cpp`, `Lpf2HubConst.h`)
and node-poweredup (`duplotrainbase*.ts`). `test_duplo_protocol` pins every
message.

- Service `00001623-1212-efde-1623-785feabcd123`, one characteristic
  `00001624-...`. Commands are written without response; the hub's messages
  arrive as notifications.
- The advertisement's manufacturer data starts with LEGO's company id
  `97 03`, then the button state, then the system type: `0x20` is the DUPLO
  Train Base. `TrainLink` connects to the first advertiser that matches.
- Every message starts with its total length and the hub id `0x00`.
- Port output command, WriteDirectModeData: `08 00 81 <port> 11 51 <mode> <value>`.
  Motor mode 0, power as a signed byte -100..100, or 127 (`0x7F`) to brake;
  0 only lets the motor go. Speaker mode 1, sounds:
  brake 3, station departure 5, water refill 7, horn 9, steam 10. LED mode 0,
  colours 0 (off) to 10 (white).
- After subscribing, `TrainLink` waits 200 ms (the hub announces its ports;
  Legoino waits as long) and sends once, 100 ms apart: speaker to sound mode
  `0A 00 41 01 01 01 00 00 00 01`, LED to colour mode
  `0A 00 41 11 00 01 00 00 00 00`. Then `main.cpp` turns the LED off and
  repeats that with `Repeats`.

Seen on the train with the `ble` log (2026-10-01, hub 34:68:b5:bc:89:9b):

- Right after subscribing it announces its ports (Hub Attached I/O, `0F 00 04
  <port> 01 <type> ...`): motor `0x00` (type `0x29`), speaker `0x01` (`0x2A`),
  LED `0x11` (`0x17`), colour sensor `0x12` (`0x2B`), speedometer `0x13`
  (`0x2C`), voltage `0x14` (`0x14`).
- **Commands sent back to back right after connecting get lost.** With the
  two format setups and the first LED colour sent in one burst, the hub
  acknowledged one setup (`0A 00 47 <port> ...`), answered `05 00 05 47 05`
  and never acknowledged the colour: the screen showed green while the LED
  kept its own colour. Sent 100 ms apart, both setups are acknowledged and the
  colour is too (`05 00 82 11 0A`). The sounds play even when the speaker's
  setup was the one lost: WriteDirectModeData carries the mode itself.
- Every port output command is answered with `05 00 82 <port> 0A` (idle,
  command completed).
- **The train does not report its battery.** It answers neither the battery
  hub property (`05 00 01 06 02` enable updates, `05 00 01 06 05` request)
  nor a format setup for its voltage port (`0A 00 41 14 00 0A 00 00 00 01`):
  no reply and no error. So the screen has no battery indicator.
- In one session, every sound was followed by `05 00 05 81 06` (generic
  error: port output command, invalid use) and some motor commands by
  `05 00 05 82 05`; the sounds played all the same. In the next session,
  after a fresh connection, these errors did not appear. Cause unknown.

Known from Legoino: the train stops by itself when it is lifted or held.

Stopping. The first firmware stopped the train with power 0, and sometimes
the train played the brake sound and drove on, while the screen said it was
standing. Not known whether the command was lost or the train, rolling on
with a free motor, took that for a push (push-and-go is how a DUPLO train
starts without an app). Since then (2026-10-01) every zero power, the KEY1
stop and level 0 while running alike, goes out as the brake (127, as Legoino
stops the DUPLO motor; the train acknowledges it like any power); `Repeats`
sends it again 0.2, 0.6, 1.5 and 3 s after the stop and then lets the train
be, so that it can still fall asleep when idle; and a motor write that NimBLE
refuses is retried on the next tick and logged as `Write failed`.

`TrainLink::update()` runs first in `loop()`, before the time is taken:
connecting blocks for a while, and a time taken before it is earlier than
the moment the LED repeats start from. (With it taken before, the first
repeat went out at once: `Repeats` now also counts a moment before the start
as not yet due.)

Not yet checked on the train: how long the train stays on while connected and
idle.

## Self-testing on the board

`tools/serial_cmd.py` (from M5Mars) talks to the firmware over USB Serial
without resetting it: it opens the port so that DTR/RTS never pass through
the reset state.

```bash
PY=~/.platformio/penv/bin/python
$PY tools/serial_cmd.py snap screen.png        # screenshot, 3x
$PY tools/serial_cmd.py send acc --wait 5      # accelerometer, pitch, neutral, level at 10 Hz
$PY tools/serial_cmd.py send ble --wait 60     # every message to (->) and from (<-) the train
$PY tools/serial_cmd.py monitor 10             # whatever the firmware prints
$PY tools/serial_cmd.py send "demo k 4 1 6"    # a made-up screen, until "live"
```

`demo splash`, or `demo <s|c|k> <level> <running 0|1> <colour 0-10> [word]`
(searching, connecting, connected; the word goes in the centre) shows that
screen instead of the real one until `live`. The README screenshots in
`docs/images/` were taken this way, at the default 3x scale.

`acc` and `ble` are toggles that stay on after the script exits: send them
again to turn them off. When nobody reads the port those lines are skipped,
so they cannot stall the loop. Only one program can hold the port: stop a
monitor before flashing.

## Board specifics

M5StickS3 is an ESP32-S3-PICO-1-N8R8 with 8 MB of flash, 8 MB of octal PSRAM
and an ST7789P3 135x240 display.

- PlatformIO has **no** `m5stack-sticks3` board id. The project uses
  `esp32-s3-devkitc-1` plus `board_build.arduino.memory_type = qio_opi` and
  the `default_8MB.csv` partitions. Do not "fix" this to a non-existent id.
- USB is native, with no CH9102 bridge, so on macOS the port is called
  `/dev/cu.usbmodem*`, not `/dev/cu.usbserial*`. Serial output needs the
  `-DARDUINO_USB_CDC_ON_BOOT=1` flag, which is already set.
- Buttons: KEY1 on G11 (`M5.BtnA`), the blue one on the front; KEY2 on G12
  (`M5.BtnB`), on the side. Grove (G9/G10) and HAT2 (G1–G8, G43, G44) are
  free.

### Publishing to M5Burner

M5Burner writes the uploaded file starting at address 0x0, so it needs a full
image. A bare `firmware.bin` is meant for address 0x10000: written at 0x0, it
overwrites the bootloader. `pio run -e sticks3 -t merged` (the extra script
`tools/merged_image.py`) merges the bootloader, the partition table,
`boot_app0` and the application into `.pio/build/sticks3/firmware-merged.bin`
with esptool `merge_bin`. The script takes the addresses and flash parameters
(dio, 80m, 8MB) from PlatformIO's regular upload settings, so the image matches
what `upload` writes.

How this is known. Checked in M5SpectrumAnalyzer on 2026-09-27: of the six
StickS3 firmwares on burner.m5stack.com, five, including the official
UIFlow2.0, are full images. Each has the bootloader at 0x0 (header
`e9 03 02 3f`), the partition table at 0x8000 (`aa 50`) and the application at
0x10000. One firmware was uploaded as a bare application. The merged image was
tested on the board: flashed on its own, at address 0x0, with esptool.

The upload form is at burner.m5stack.com/developer/firmware/upload. It asks for:
- a name, a category and the supported devices (StickS3);
- a firmware description and a version description, both in Markdown;
- the version number and a link to the project;
- the `.bin` file;
- visibility: Public requires moderation;
- a cover image: a screenshot of the screen works.

### The side button is handled by the PMIC, not the firmware

| Action | Result |
|---|---|
| Single press | Power on / reset |
| Double press | Power off |
| Long hold | Download mode (the internal green LED blinks) |

So the firmware does not need its own power-off button. If powering off from
software is ever needed (for example on idle), `M5.Power.powerOff()` used to
wake the StickS3 right away by timer —
[M5Unified#235](https://github.com/m5stack/M5Unified/issues/235), fixed in
0.2.23. This has not been checked on a board with the fixed version.

`M5.Power` does not set `_wakeupPin` for the StickS3, so there is no
ready-made wake-up from deep sleep by button; it would have to be configured
manually with `esp_sleep_enable_ext0_wakeup`.

### If flashing fails

`A fatal error occurred: Failed to connect to ESP32-S3: No serial data received.`

The board shows up as `USB JTAG_serial debug unit` (VID 0x303A, PID 0x1001):
that is the built-in USB-Serial-JTAG, not a CDC port (a consequence of
`ARDUINO_USB_MODE=1`). Auto-reset into download mode through it does not
always work, and neither `--before usb_reset` nor `--before no_reset` helps.
The only fix is manual: hold the side button until the green LED blinks.

### If the board is stuck in the bootloader

The firmware does not start, and the port shows `boot:0x0 (DOWNLOAD(USB/UART0))`
and `waiting for download`. This happened when a pyserial script opened and
closed the port to check on it: macOS toggles DTR/RTS when doing so, and
USB-Serial-JTAG takes that as a command to enter the bootloader. The way out is
a single short press of the side button. Open the port with
`tools/serial_cmd.py`, which avoids it; `pio device monitor` has not been
checked for this effect.

## Tests

Unit tests cover the logic in `lib/`, with one directory
`test/test_<module>/test_main.cpp` per module. Rendering is checked by eye on
the board: do not try to write tests for `Renderer`; that would require mocking
all of LovyanGFX and would prove nothing useful; use `serial_cmd.py snap`.
The same goes for `TrainLink`: it is checked with the train and the `ble` log.
Everything about the protocol that can be checked without Bluetooth lives in
`DuploProtocol` and is tested there.

`main` in the tests returns the number of failures from `UNITY_END()`, and
PlatformIO reports a non-zero exit code as a signal number. A line like
`Program received signal SIGALRM` with failing tests is a reporting artifact,
not a separate problem; it disappears once the tests pass.
