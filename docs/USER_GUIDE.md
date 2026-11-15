# User guide

How to set up the prototype disc, use it, and read what it shows. Current firmware:
0.1.2-m1 (serial only; BLE and settings come in M2 and M3).

## 1. What you need

- The prototype board (see [BOM_PROTOTYPE.md](BOM_PROTOTYPE.md)) and a data-capable USB cable.
- A Mac with Python 3 and `pyserial` (`pip3 install pyserial`).
- To build or flash: the Arduino IDE (it ships `arduino-cli`), the `esp32` core 3.0.7 and
  the LovyanGFX library. Details in [DEVELOPER.md](DEVELOPER.md).

## 2. Plug in and check

Connect the disc by USB. It boots to the idle screen: a cyan ring that breathes slowly,
and the text "Identity Disc", "USB ready", the disc id and the firmware version.

```sh
python3 tools/disc_test.py info
```

Expected:

```
# verified identity-disc fw 0.1.2-m1 disc_id cfdc3ab0aa694eb7
{'id': 3, 'ok': True, 'device': 'identity-disc', 'proto': 1, 'fw': '0.1.2-m1', 'disc_id': '...', 'transport': 'serial'}
```

The tool finds the disc by its MAC address and then asks it to confirm its name. Other
boards on the same Mac are never opened. If it prints `REFUSING`, the port is not the
disc.

## 3. Ask the disc for a value

```sh
python3 tools/disc_test.py prf "deploy prod"            # the label is shown on the disc
python3 tools/disc_test.py prf "unlock vault" --salt <64 hex characters>
```

On the disc the screen changes to **APPROVE?** with your label, a countdown and an amber
ring. Then:

| You do | Result |
|---|---|
| **Press and hold the centre circle** until it fills (about 1.2 s) | Approved. The tool prints the 64-character value. Ring turns green |
| Lift early | Nothing happens; the fill resets |
| **Tap the bottom strip** ("tap here to deny") | Denied. Ring turns red. Wait 3 s before asking again |
| Do nothing | After 30 s (default) it times out; ring is amber; wait 3 s |

Rules worth knowing:

- Read the label before you approve. The host chooses the label, so on a hostile host it
  could be misleading.
- The same salt always gives the same value. A different salt gives a different value.
  The value changes only if the key is reset.
- Only one request can be pending at a time (`busy`).
- If your finger is already on the screen when a request arrives, lift it first. Touches
  that began before the request are ignored.

## 4. What the ring means

| Ring | Meaning | Speed |
|---|---|---|
| Cyan, slow breathing | Idle, ready | calm |
| Amber, brisk | A request is waiting for you | brisk |
| Amber, faster | Timed out or cancelled | faster |
| Red, rapid | You denied the request | fastest |
| Green, calm | Approved | calm |

To see all of them without making a request:

```sh
python3 tools/disc_test.py demo
```

The screen says "DEMO - not a request" and steps through every mode, about 4.5 s each.
Touch the screen to end it. The demo never touches the key and returns nothing.

## 5. Run the self-check

```sh
python3 tools/disc_test.py check
```

Run it yourself in a terminal where you can read the prompts. It runs 8 automatic checks,
then asks you to hold three times, leave the disc alone for 5 s, tap the deny strip once,
and leave it alone again. A full pass prints `16/16 checks passed`.

## 6. Errors

The tool prints the disc's reply. Error codes:

| Code | Meaning | What to do |
|---|---|---|
| `bad_request` | A field was malformed | Check salt (64 hex), label (1 to 48 printable ASCII), timeout (5 to 120) |
| `bad_version` | Protocol version not supported | Update the tool or firmware |
| `busy` | A request is already on the disc, or a demo is running | Finish or touch the screen to end the demo, or send `cancel` |
| `denied` | You denied it | None |
| `timeout` | Not approved in time | Ask again |
| `cancelled` | The host cancelled | None |
| `cooldown` | Too soon after a deny, timeout or result | Wait 3 s |

Other problems:

| Symptom | Likely cause |
|---|---|
| `disc not found` | Cable is charge-only, disc unplugged, or a different board is attached; run `python3 tools/find-disc.py` |
| `REFUSING: ... did not identify as identity-disc` | The port belongs to another device; nothing was sent beyond `info` |
| Touch seems to approve by itself | A hand or object is on the glass. Hands off, then retry |
| Screen blank | Reflash with `sh tools/flash.sh` |

## 7. Key and privacy

- The key is created on the first boot and stays on the disc. No command reads it.
- Reflashing with `tools/flash.sh` keeps the key (it lives in NVS). Erasing the flash
  or restoring the factory image destroys it, and `disc_id` changes.
- Treat the prototype as a demonstration. The key is in plain flash, and anyone with
  the cable can read it. See the security position in
  [FIRMWARE_SPEC.md](FIRMWARE_SPEC.md) section 2.

## 8. Restore the factory firmware

The factory image is kept locally in `backup/` (not in git). See `CAPABILITIES.md` for the
`esptool` command. This erases the disc key.
