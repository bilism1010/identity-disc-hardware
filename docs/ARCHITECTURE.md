# Architecture

## System

```
 host (Mac)                                        identity disc (ESP32-C3)
 ┌──────────────────────┐   USB CDC serial         ┌─────────────────────────────┐
 │ tools/disc_test.py   │  JSON lines, 115200      │ identity-disc.ino           │
 │ (later: idvault CLI) │ <──────────────────────> │  protocol + state machine   │
 │                      │                          │   │         │         │     │
 │                      │   BLE GATT (M2)          │ keystore.h  board.h   glow.h│
 │                      │ <- - - - - - - - - - - > │  NVS key    LCD+touch  ring │
 └──────────────────────┘   bonded, passkey        └─────────────────────────────┘
```

Both carriers carry the same newline-delimited JSON protocol, defined in
[FIRMWARE_SPEC.md](FIRMWARE_SPEC.md) section 3. The disc handles one pending request at
a time, whichever carrier it came from.

## Firmware modules (`firmware/identity-disc/`)

| File | Responsibility |
|---|---|
| `identity-disc.ino` | Setup and loop, protocol handler, state machine (`Idle`, `Request`, `Result`, `Demo`), screens, touch hold logic |
| `board.h` | Pin map, `Display` (LovyanGFX GC9A01 over SPI2 at 40 MHz), `touch` (CST816 polled over I2C, address `0x15`) |
| `keystore.h` | Key in NVS (namespace `idv`, key `key`), HMAC-SHA256 PRF, `disc_id`, RFC 4231 boot self-test |
| `jsonmin.h` | Small parser for flat one-line JSON requests (no dynamic allocation, bounded fields) |
| `glow.h` | Status ring: 24 arc segments on the screen rim, pulse plus rotating chaser, speed and colour by mode |
| `sketch.yaml` | Default FQBN for `arduino-cli` |

## Request flow

```
host: {"v":1,"id":7,"op":"prf","salt":"<64 hex>","label":"deploy prod"}
  │
  ▼ validate (version, salt, label, timeout) ── bad ──> {"ok":false,"err":"bad_request"}
  ▼ state must be Idle and past cooldown   ── no  ──> busy / cooldown
  ▼ State::Request: screen shows label + countdown, ring amber; any finger already down must lift first
  ▼ loop polls touch every ~10 ms
        deny zone (y >= 195)             -> "denied", ring red, 3 s cooldown
        hold in centre circle for 1.2 s  -> PRF computed, reply sent, ring green
        no approval before timeout_s     -> "timeout", ring amber, 3 s cooldown
        host sends cancel                -> "cancelled"
  ▼ State::Result for 1 s, then Idle
```

The PRF is computed only at the moment of approval, so no value exists in memory
beforehand.

## Cryptography

```
K        32 random bytes, generated on first boot, stored in NVS, never sent
prf      = HMAC-SHA256(K, "idv-prf-v1" || salt)
disc_id  = first 8 bytes of HMAC-SHA256(K, "idv-disc-id-v1"), 16 hex characters
```

`disc_id` changes when the key is reset, so a host can detect a re-keyed disc. At boot
the HMAC implementation is checked against an RFC 4231 vector, and `prf` is refused if
that fails.

The key is stored in plain flash. This is a prototype limit; see the security position
in the spec.

## Device identification

Several ESP32 boards are attached to the same Mac (an Arduino Pro Micro volume knob, an
Elecrow display with a knob, and this disc). Two independent checks make sure tools only
ever touch the disc:

1. **Port lookup** (`tools/find-disc.py`): reads the macOS I/O registry (no port is
   opened) and requires USB VID:PID `303A:1001` and a USB serial number equal to the
   disc's MAC `48:31:B7:3D:40:10`. Exactly one match is required, or the tool fails.
2. **Firmware identity** (`disc_test.py`): after opening the port, sends `info` and exits
   unless the reply has `"device":"identity-disc"`.

`tools/flash.sh` uses only check 1 and never accepts a port argument.

## Status ring

The vault's Rev 1.0 design drives 16 WS2812B LEDs. The bench board has none, so `glow.h`
draws the same behaviour as 24 arc segments on the screen rim (radius 112 to 119 px,
25 fps, redrawing only segments whose colour changed).

| Mode | Used when | Colour | Pulse (rad/s) | Chaser (rad/s) |
|---|---|---|---|---|
| Standby | Idle | cyan | 1.8 | 0.8 |
| Presence | Request pending | amber | 3.5 | 1.2 |
| Warning | Timeout, cancel | amber | 5.0 | 2.0 |
| Alert | Denied | red | 10.0 | 4.5 |
| Ok | Approved | green | 1.8 | 0.8 |

Brightness per segment is `amp*0.75 + flare*0.25`, where `amp = 0.625 + 0.375*sin(pulse)`
and `flare = exp(-3.5 * angular_distance_from_chaser)`. Speeds and colours are ported
from the vault's `disc_display.c`.

## Hardware

See [BOM_PROTOTYPE.md](BOM_PROTOTYPE.md) for the bench board and
[../hardware/bom/BILL_OF_MATERIALS.md](../hardware/bom/BILL_OF_MATERIALS.md) for the Rev 1.0
target. `CAPABILITIES.md` is the source of truth for the bench board's facts, with each
fact tagged measured, chip or assumed.

Key constraint: the ESP32-C3's USB is a fixed-function serial/JTAG unit, so the disc is
a serial device, not a HID or FIDO2 key. That is why the host talks to it with a custom
protocol.

## Memory and flash

| Resource | Figure |
|---|---|
| Firmware 0.1.2-m1 | about 417 KB of the 1.25 MB app partition |
| Frame buffer | None; drawing is direct to the panel. A full RGB565 buffer would be 115 KB |
| NimBLE (M2) | Not yet measured |

## Planned (M2 and M3)

- BLE GATT service `be52314d-497c-439b-9e51-cab455d9cf4d` with RX and TX characteristics,
  LE Secure Connections, bonding, display-only 6-digit passkey, 60 s pairing window.
- Settings screen: pair a host, forget hosts, reset key (clears bonds), about, backlight
  dimming.
