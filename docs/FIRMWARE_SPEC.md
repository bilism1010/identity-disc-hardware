# Identity Disc prototype firmware: specification

Status: **draft for review, nothing built yet.** Written 2026-11-10.

This specifies the first firmware for the ESP32-C3 round display acting as a bench
prototype of the Identity Disc. Hardware facts come from
[`CAPABILITIES.md`](../CAPABILITIES.md); where that sheet marks something as
*assumed* or unchecked, this spec says so rather than relying on it.

## 1. What the prototype is

A touch-to-approve token. The host sends a salt and a short label. The disc shows the
label, waits for the owner to press and hold the screen, then returns
`HMAC-SHA256(device key, salt)`. The host uses that value the way `idvault` would use
a FIDO2 `hmac-secret` / PRF output: as input to derive the vault key.

It is not a FIDO2 authenticator. The C3 cannot be a USB HID device, so the link is a
custom protocol over USB serial and BLE.

### Decisions (owner, 2026-11-10)

| Topic | Decision |
|---|---|
| Transport | USB serial and BLE, one protocol over both |
| Key storage | Software key in NVS. No eFuse burning |
| Disc computation | HMAC PRF |
| Approval gesture | Press and hold |
| Request screen | Shows the label the host sent, with a way to deny |
| BLE security | Bonded and encrypted, 6-digit passkey shown on the disc |
| Key lifecycle | Generated on first boot; on-disc reset gesture; no export, no host provisioning |
| Stack | Arduino core 3.0.7, LovyanGFX, NimBLE; Python host test script |
| Host side | Test script in this repo. The `idvault` repo is not touched |
| Flashing | The assistant may flash, but only through `tools/flash.sh`, which finds the board by MAC and refuses otherwise |

### Out of scope

eFuse keys, Secure Boot, flash encryption, FIDO2/CTAP, WiFi, the LED ring, battery
management, and any change to the `idvault` CLI.

## 2. Security position

This is a prototype of the interaction, not of the key protection. Stated plainly:

- The device key sits in NVS in plain flash. Anyone with the USB cable can read it
  with esptool, or flash firmware that prints it.
- The label on screen is whatever the host sent. A compromised host can show a
  harmless label for a harmful request. The label helps against mistakes, not against
  a hostile host.
- Over USB serial the PRF output crosses the cable in the clear. Over BLE it is
  protected by link encryption.
- What the prototype does provide: no PRF output leaves the disc without a deliberate
  hold on the screen, and only one request can be pending at a time.

## 3. Protocol

Carrier-neutral. Each message is one line of UTF-8 JSON ending in `\n`, at most 512
bytes. JSON is used because `idvault` already depends on `serde_json`.

Every request carries `v` (protocol version, `1`) and `id` (host-chosen unsigned
integer). Every reply echoes `id` and has `ok` true or false. Unsolicited lines from
the disc carry `evt` and no `id`.

### Requests

| `op` | Extra fields | Needs touch | Success reply |
|---|---|---|---|
| `info` | none | no | `proto`, `fw`, `disc_id`, `transport` |
| `ping` | none | no | none |
| `prf` | `salt`, `label`, optional `timeout_s` | yes | `prf` |
| `cancel` | none | no | none; the pending `prf` replies `cancelled` |
| `demo` | none | no | none; cycles the ring visuals (standby, presence, warning, alert, verified) with "DEMO" on screen; touch or `cancel` ends it; `busy` if a request is pending |

`info` also returns `"device":"identity-disc"`. Host tools must check it and refuse any port that does not report it.

**Ring visuals** (`firmware/identity-disc/glow.h`, from the vault `disc_display.c`): a breathing pulse plus a rotating chaser, faster with urgency. Standby cyan 1.8 rad/s; presence amber 3.5; warning amber 5.0; alert red 10.0; ok green 1.8. A pending request shows presence; approve shows ok, deny shows alert, timeout and cancel show warning.

Field rules:

- `salt`: exactly 64 hex characters (32 bytes).
- `label`: 1 to 48 printable ASCII characters. Shown on screen over at most two lines.
- `timeout_s`: 5 to 120, default 30.
- `prf` in the reply: 64 hex characters.
- `disc_id`: 16 hex characters, the first 8 bytes of
  `HMAC-SHA256(K, "idv-disc-id-v1")`. It changes when the key is reset, so the host
  can tell that a disc has been re-keyed.

Example:

```
> {"v":1,"id":7,"op":"prf","salt":"<64 hex>","label":"deploy prod"}
< {"id":7,"ok":true,"prf":"<64 hex>"}
```

### PRF definition

```
prf = HMAC-SHA256(K, "idv-prf-v1" || salt)
```

`K` is the 32-byte device key. The prefix separates PRF outputs from `disc_id`. The
same salt always gives the same output until the key is reset.

### Errors

`{"id":7,"ok":false,"err":"<code>"}` with one of:

| Code | Meaning |
|---|---|
| `bad_request` | Malformed JSON, unknown `op`, bad field, line too long |
| `bad_version` | `v` is not supported |
| `busy` | A `prf` is already pending, on either transport |
| `denied` | Owner denied on the disc |
| `timeout` | No approval within `timeout_s` |
| `cancelled` | Host sent `cancel` |
| `cooldown` | Too soon after a deny or timeout (3 s) |

### Events

| `evt` | When |
|---|---|
| `ready` | Once after boot, with `proto` and `fw`. The host must wait for it |
| `touch_wait` | A `prf` is on screen and waiting |

### Serial carrier

USB CDC. Baud rate is irrelevant to CDC; the host uses 115200.

**Opening the port resets the board** (noted in `CAPABILITIES.md`). So every host
session starts with a reboot, and the host must wait for `ready` before sending.
Measured on the bench unit: holding DTR/RTS low while opening does NOT reset the board (see tools/disc_test.py). Originally unknown, and was the
first thing to measure in milestone 1. The key survives because it is in NVS.

Debug logging shares this port. Log lines start with `#` and the host ignores them.

### BLE carrier

A custom GATT service, modelled on the Nordic UART layout:

| Item | UUID | Properties |
|---|---|---|
| Service | `be52314d-497c-439b-9e51-cab455d9cf4d` | |
| RX (host to disc) | `8e7ad6fd-4382-4338-aeba-d354fef1c756` | write; encrypted and authenticated |
| TX (disc to host) | `b3ea993c-ed3a-4940-b10f-6249d6dbddd9` | notify; encrypted and authenticated |

Lines longer than one ATT packet are split across writes or notifications and
rejoined on `\n`.

Pairing: LE Secure Connections with bonding and MITM protection; the disc is
display-only and shows a 6-digit passkey to type on the host. The disc accepts a new
bond only while the owner has opened a 60-second pairing window from the settings
screen. Outside that window only bonded hosts can use the service.

## 4. Device behaviour

### Key

- First boot: 32 random bytes from the hardware RNG, stored in NVS namespace `idv`.
  `CAPABILITIES.md` says the RNG is truly random only when a radio is on, so the radio
  is started before the key is generated.
- Reset: settings screen, hold for 5 seconds. Generates a new key and removes all BLE
  bonds.
- No command reads or writes the key.

### States

```
BOOT -> IDLE -> REQUEST -> RESULT (1 s) -> IDLE
          |-> SETTINGS -> PAIRING (60 s) | RESET KEY | ABOUT
```

| Screen | Shows | Touch |
|---|---|---|
| Idle | Disc ring, link state (USB, BLE bonded, BLE connected) | Hold 3 s for settings |
| Request | Label, which transport asked, seconds left | Hold centre 1.2 s to approve while a ring fills; tap the deny zone at the bottom to deny. Lifting early cancels the fill |
| Result | Approved, denied or timed out | none |
| Pairing | 6-digit passkey, seconds left | Tap to close |
| Settings | Pair new host, Forget hosts, Reset key, About | Tap to pick; destructive items need a 5 s hold |
| About | Firmware version, `disc_id`, MAC | Tap to close |

The backlight dims after 30 seconds idle and returns on touch or on a request.

Approve and deny use touch coordinates only, not the CST816's built-in gesture codes,
which are untested on this unit.

### Self-test

At boot the firmware checks its HMAC against an RFC 4231 test vector and refuses
`prf` requests if it fails.

## 5. Repository layout

```
firmware/identity-disc/     Arduino sketch: identity-disc.ino (protocol, states, screens), board.h
                            (display, touch), keystore.h (NVS key, HMAC), jsonmin.h, glow.h (ring);
                            M2 adds a BLE link
tools/                      flash.sh, find-disc.py, disc_test.py (pyserial; bleak from M2)
docs/                       PURPOSE, ARCHITECTURE, USER_GUIDE, DEVELOPER, BOM_PROTOTYPE, this spec
```

Display and touch setup start from `examples/c3-round-test/`. The files in
`reference/input-knob/` are consulted for touch handling, not compiled.

To install before building: the NimBLE-Arduino library and the Python package `bleak`
(neither is on this Mac today), plus a JSON library for the sketch or a small
hand-written parser.

## 6. Milestones

**M0. Confirm the board by eye (owner). Done.** `CAPABILITIES.md` lists the panel as not
confirmed by eye and touch coordinates as unchecked. With the test sketch already on
the board: colour bars and counter visible, and touch dots land under the finger. If
the image is mirrored or touch is rotated, that is fixed before anything else.

**M1. Serial. Done (fw 0.1.2-m1, 16/16 checks on the bench unit).** Key store, protocol, idle/request/result screens, press and hold,
`disc_test.py` over serial. Done when: the same salt returns the same value twice, a
different salt returns a different value, deny and timeout return the right errors,
and nothing is returned without a hold. Also records whether the port-open reset can
be avoided.

**M2. BLE.** GATT service, pairing window, passkey screen, bonded reconnect,
`disc_test.py --ble`. Done when M1's checks pass over BLE, an unbonded host is
refused, and serial and BLE give the same output for the same salt.

**M3. Settings.** Reset key, forget hosts, about, backlight dimming. Done when a reset
changes `disc_id` and PRF outputs, and old bonds no longer connect.

## 7. Risks and unknowns

| Item | Why it matters | When resolved |
|---|---|---|
| Display and touch unconfirmed | Resolved: owner confirmed screen and ring by eye; touch zones respond correctly (16/16 checks) | resolved (M0, M1) |
| Port-open reset | Measured: opening with DTR/RTS low does not reset the board; the scripts use that open | resolved (M1) |
| Flash size with BLE | App space is 1.25 MB and the test sketch uses 27%. NimBLE's size is not yet measured; a different partition table may be needed | Start of M2 |
| macOS passkey pairing through `bleak` | The system pairing prompt is outside the script's control | M2 |
| Touch INT and RST pins | Polling over I2C works, so INT is unused; RST (GPIO1) is assumed but touch initialises | resolved (M1) |
| Power without USB | Battery connector unchecked, so BLE use is still tethered to a USB power source | Not in scope |
