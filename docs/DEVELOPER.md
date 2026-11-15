# Developer guide

For anyone building, flashing, testing or extending the firmware. Design background is in
[ARCHITECTURE.md](ARCHITECTURE.md), the protocol in [FIRMWARE_SPEC.md](FIRMWARE_SPEC.md).

## Safety rules for this bench

Several boards are plugged into the same Mac. The rules below exist so tools never touch
the wrong one.

- Flash only with `sh tools/flash.sh`. It finds the port from the disc's MAC and takes no
  port argument. Do not call `arduino-cli upload` or `esptool` by hand.
- Talk to the disc only through `tools/disc_test.py`, which verifies `device == identity-disc`.
- Never open `/dev/cu.usbmodem68EE8F5DB6202`, `/dev/cu.usbmodemHIDJB1`, the Pro Micro
  volume knob or the Elecrow display and knob.
- No eFuse operations (Secure Boot, flash encryption, key burning). They are permanent.
- `CAPABILITIES.md` is the hardware source of truth and is not edited casually. Facts
  measured later (for example "opening the port resets the board" is superseded; DTR/RTS
  held low does not reset it) are recorded in the spec.

## Setup

1. Install the Arduino IDE (provides `arduino-cli`, found at
   `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`;
   set `ARDUINO_CLI` to override).
2. Install the ESP32 core 3.0.7 and LovyanGFX 1.2.26:
   ```sh
   arduino-cli core install esp32:esp32@3.0.7
   arduino-cli lib install LovyanGFX@1.2.26
   ```
3. `pip3 install pyserial`. For M2 also `pip3 install bleak` and install NimBLE-Arduino.

## Build and flash

```sh
sh tools/flash.sh                 # compile + upload to the disc only
```

Required settings (already in `firmware/identity-disc/sketch.yaml`):
`esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashSize=4M`. Without `CDCOnBoot=cdc`, `Serial` output
does not appear on the USB port.

Compile only:

```sh
arduino-cli compile firmware/identity-disc
```

The sketch is about 417 KB of 1.25 MB.

## Repository layout

```
firmware/identity-disc/   sketch: identity-disc.ino, board.h, keystore.h, jsonmin.h, glow.h
tools/                    flash.sh, find-disc.py, disc_test.py
docs/                     PURPOSE, ARCHITECTURE, USER_GUIDE, DEVELOPER, BOM_PROTOTYPE, FIRMWARE_SPEC, CHECKPOINT
examples/c3-round-test/   display and touch check sketch
hardware/                 Rev 1.0 BOM and CAD (copied from the vault repo at 9be0cea)
reference/input-knob/     code from the sister project, read only, not compiled
CAPABILITIES.md           surveyed facts for the bench board
backup/                   factory firmware (gitignored)
```

## Testing

`python3 tools/disc_test.py check` runs 16 checks:

- 8 automatic: `info`, `ping`, garbage line, bad version, bad salt, empty label, bad
  timeout, unknown op.
- 8 that need a person at the disc: approve with a hold (salt A), same salt gives the same
  value, different salt gives a different value, no touch gives `timeout`, then
  `cooldown`, tap on the deny strip gives `denied`, a second request gives `busy`,
  `cancel` gives `cancelled`.

Run the touch checks in a terminal you can read. An agent running the script cannot show
you the prompts, and an unexpected touch during a "do not touch" step looks like a
firmware fault. This caused two false failures during development.

Options: `--salt <64 hex>`, `--wait-ready` (open the port normally and wait for `ready`;
the default holds DTR/RTS low so the board is not reset).

### Debugging touch

If holding does nothing, add a debug line in `tickRequest()`:

```cpp
Serial.printf("# touch down=%d x=%d y=%d needRel=%d hold=%d\n", down, x, y, needRelease, holding);
```

Lines starting with `#` are ignored by the protocol. Print only on change, not every
loop. Remove it before committing.

## Conventions

- One JSON line per message, at most 512 bytes. Add a new `op` to the table in the spec
  and to `handleLine()`; unknown ops must keep returning `bad_request`.
- Never add a command that returns, sets or logs the key.
- The PRF must be computed only after approval, not before.
- Redraw the screen only on change. After anything that clears the screen call
  `glow::invalidate()` so the ring repaints.
- Touch zones: hold circle radius 60 at (120,148); deny zone `y >= 195`; hold time 1200 ms.
- Debug output goes to `Serial` with a leading `#`.
- Bump `FW_VERSION` in `identity-disc.ino` for any flashed change; `info` reports it.

## Adding a ring mode

Add an enum value and a row to `LOOKS[]` in `glow.h` (dim colour, peak colour, pulse
speed, rotation speed), call `glow::set()` from the screen code, then `glow::invalidate()`.

## Next work

M2 (BLE): add NimBLE-Arduino, measure flash size first (partition table may need
changing), implement the GATT service from the spec, the pairing window and passkey
screen, and `disc_test.py --ble`. Open risks: macOS passkey pairing through `bleak`, and
the assumed touch interrupt and reset pins. M3: settings screen. See the spec sections 6
and 7.

## Licence

Apache License 2.0; see `LICENSE`.
