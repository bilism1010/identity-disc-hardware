# Checkpoint — Identity Disc prototype firmware, M1 (serial) + glow ring — 2026-11-15 15:45
**Status:** M1 COMPLETE (fw 0.1.2-m1, 16/16). Next: M2 BLE

Repo: `/Users/bill/src/maker-iot/identity-disc-hardware` (renamed from `esp32c3-round-display`; the
Claude Code session folder still points at the old path, so `/restart` fails — start Claude
Code in the new folder and use /resume, Ctrl+A for all projects). Read-only sister repos:
`/Users/bill/src/maker-iot/input-knob` and
`/Users/bill/src/research/systems-programming/indentity-disc-vault` (sic).

## Goal (user's words)
> "check this out in the dir/repo - are we ready to build the indentity disc firmware for
> this ESP32 display from aliexpress? ask my clarifying questions until you're clear and
> I'm satisfied with answers to your questions ."

Build the Identity Disc prototype firmware for the ESP32-C3 1.28" round touch display.
Spec: `docs/FIRMWARE_SPEC.md`. Later scope addition:
> "i remember talking to you about the amber and red glowing and speeds like the TV Show Silo's glowing stick the head of IT carried. Do you have that somehwere in the history or conversation? if not add it. make sure you can correctly identify this device as there are two other boards connected at least - a arduino pro micro as a volume knob, and a elecrow display + knob. we want to target the correct device"

## Decisions and corrections (verbatim)
- "1 - approve ; 2 yes ; 3 yes, test / go" — spec approved incl. my five choices (JSON lines,
  60 s pairing window, hold times 1.2/3/5 s, reset clears bonds, M1 serial → M2 BLE → M3
  settings); flashing via `tools/flash.sh` allowed; opening the disc's serial port allowed.
- Earlier answers: serial+BLE one protocol; software key in NVS; HMAC PRF; press and hold;
  show label with deny; BLE bonded 6-digit passkey on screen; generate on first boot +
  on-disc reset; Arduino core 3.0.7 + LovyanGFX + NimBLE; test script in this repo.
- Standing: "Do not commit or push; leave the files staged-ready but uncommitted so the
  owner can review." · "do not modify that repo" (vault; input-knob also read-only) ·
  never touch other devices (`/dev/cu.usbmodem68EE8F5DB6202`, `/dev/cu.usbmodemHIDJB1`,
  the Pro Micro volume knob, the Elecrow display+knob) · `CAPABILITIES.md` never edited ·
  no eFuse operations · user's email stays private.
- Silo staff: NOT in prior history (first mentioned in the owner's message above). The amber/red
  glow behaviour WAS (earlier requirements + vault `disc_display.c`); I implemented from the
  vault spec. I have not seen the show's staff described — owner may want speeds/colours tuned.

## Done (with evidence)
- Firmware in `firmware/identity-disc/`: `identity-disc.ino`, `board.h`, `keystore.h`,
  `jsonmin.h`, `glow.h` (new), `sketch.yaml`. 0.1.1-m1 = 417,040 bytes flashed.
- `sh tools/flash.sh` → hash verified, only `/dev/cu.usbmodem2141301` (MAC-matched).
- `python3 tools/disc_test.py info` → `device identity-disc`, `fw 0.1.1-m1`, `disc_id cfdc3ab0aa694eb7`.
- `python3 tools/disc_test.py demo` → ok; demo started on the disc (visuals NOT yet confirmed by owner).
- Glow ring: 24 arc segments r112–119, 25 fps, breathing pulse + chaser. Standby cyan 1.8 rad/s,
  Presence amber 3.5, Warning amber 5.0, Alert red 10.0, Ok green 1.8. Request→presence,
  approve→ok, deny→alert, timeout/cancel→warning.
- New `demo` op (Idle only, else `busy`; touch or `cancel` ends; screen says "DEMO - not a request").
- Device targeting: `find-disc.py` (MAC 48:31:B7:3D:40:10 + VID:PID 303A:1001, exactly one port);
  `info` returns `"device":"identity-disc"`; `disc_test.py` `verify()` exits unless it matches.
- Spec updated: demo op, ring visuals, measured port-open result (DTR/RTS low does not reset).
- Earlier `check` run: 11/16 pass (all non-touch checks).

## Update 2026-11-15 (later)
- `disc_test.py check` → 16/16 on hardware with the owner running it (fw 0.1.2-dbg, then debug lines
  removed and reflashed as 0.1.2-m1; `info` verified). Earlier "failures" were because the owner could
  not see prompts when an agent ran the script; run the touch checks yourself with `!`.
- Owner confirmed the demo/ring looks good. Silo staff look still not described.
- Docs written: PURPOSE, ARCHITECTURE, USER_GUIDE, DEVELOPER, BOM_PROTOTYPE; README and spec updated.
- Owner then authorised commit and push (earlier "do not commit" lifted). No git remote existed.

## Unresolved
- M2 (BLE) and M3 (settings) not started. Remote repo for push not yet set up.

## Dead ends (don't retry)
- `arduino-cli` not on PATH: use
  `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`.
- Homebrew/pip esptool is broken; use the Arduino-bundled one.
- zsh: `echo ====` fails; shell cwd resets each call, so `cd` into the repo each time.
- The C3 cannot be USB HID/FIDO2 (fixed-function USB).
- Opening the port normally never yielded a `ready` event; default no-reset open is used.

## Working-tree state
- Branch: main, no commits
- `git status --short`: all untracked: `.gitignore CAPABILITIES.md LICENSE README.md assets/ docs/ examples/ firmware/ hardware/ reference/ tools/` (`backup/` ignored)
- Stashes: none · Background jobs/agents: none · Ports: `/dev/cu.usbmodem2141301` (disc), `/dev/cu.usbmodem68EE8F5DB6202` (other board, leave alone)

## Authorizations and scope (RE-CONFIRM after reset)
- Flash: allowed, only via `tools/flash.sh`. Serial testing of the disc via `tools/disc_test.py`: allowed.
- No commit/push, no vault/input-knob changes, no eFuse, never touch other devices.

## Baseline check
- `"…/arduino-cli" compile firmware/identity-disc` → 417,040 bytes
- `python3 tools/disc_test.py check` → last full run 11/16 (before glow); touch checks need the owner

## Exact next command (superseded: M1 passed; next is M2 — install NimBLE-Arduino and bleak, measure flash)
Ask the owner how the ring looked and to be at the disc, then
`cd /Users/bill/src/maker-iot/identity-disc-hardware && python3 tools/disc_test.py check`
(first hold prompt waits 40 s). If holding does nothing, add `#` debug lines in `tickRequest()`
(raw x,y, `holding`, `needRelease`) and reflash with `sh tools/flash.sh`. After M1 passes:
update README, then M2 (install NimBLE-Arduino, `pip install bleak`).
