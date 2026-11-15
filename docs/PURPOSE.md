# Purpose

## What this repo is

`identity-disc-hardware` is the hardware side of the **Identity Disc Vault**: a small
round "disc" that holds a secret key and only releases a value derived from it when its
owner physically approves on the disc itself.

The software side (the `idvault` host CLI and the design of the full disc) lives in the
vault repo. This repo builds the physical piece and its firmware:

| Repo | Role |
|---|---|
| `indentity-disc-vault` (sic) | Host software, product design, the Rev 1.0 disc concept |
| `identity-disc-hardware` (this repo) | Prototype firmware, bring-up tools, hardware documents |
| `input-knob` | Sister project on similar hardware; source of the touch, display and BLE code in `reference/input-knob/` |

## The problem it solves

A secret that unlocks something valuable (a vault, a deploy, a password store) is safer
if a person must be physically present to release it. Software prompts on the same
computer can be clicked by malware. A separate device with its own screen and touch
surface cannot be clicked from the host: the owner must put a finger on the disc.

The disc answers one question for the host: "given this salt, what is the value only
this disc can compute?" It will do that only after a deliberate press-and-hold, and the
screen shows what is being asked.

## What the prototype proves

The prototype runs on an off-the-shelf AliExpress ESP32-C3 board with a 1.28" round
touch display. It is a prototype of the **interaction**, not of the key protection.

It proves:

- A host can request a PRF (a keyed pseudo-random value) over serial, with a label the
  owner can read before approving.
- Nothing leaves the disc without a hold on the screen; a tap on the deny zone refuses.
- The ring glows by urgency (cyan standby, amber presence and warning, red alert,
  green approved), so the disc can be read from across a desk.
- One protocol can serve both USB serial and, later, BLE.

It does not prove, and does not claim:

- Secure key storage. The key sits in plain flash (see the security position in
  [FIRMWARE_SPEC.md](FIRMWARE_SPEC.md) section 2).
- A hostile-host defence. The label on screen is whatever the host sent.
- USB FIDO2/WebAuthn. The ESP32-C3's USB is fixed-function (serial and JTAG), so it
  cannot be a security key. The Rev 1.0 design uses an ESP32-S3 and a secure element for
  that.

## Who it is for

- The owner, building and testing the disc on the bench.
- Anyone continuing the firmware: start with [DEVELOPER.md](DEVELOPER.md).
- Anyone using a disc: [USER_GUIDE.md](USER_GUIDE.md).

## Where it is going

| Milestone | Scope | State |
|---|---|---|
| M0 | Confirm display and touch on the bench unit | Done (owner confirmed the screen and ring; touch hits the right zones) |
| M1 | Serial protocol, key in NVS, hold-to-approve, glow ring, test script | Done, 16/16 hardware checks |
| M2 | BLE with bonded passkey pairing, same protocol | Not started |
| M3 | On-disc settings: pairing window, forget hosts, reset key, about, dimming | Not started |
| Rev 1.0 | Purpose-built disc: ESP32-S3, secure element, LED ring, native USB | Design only: [hardware/bom/](../hardware/bom/BILL_OF_MATERIALS.md) |
