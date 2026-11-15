# Reference code from input-knob

These files are copied unchanged from the sister repo
[`input-knob`](https://github.com/bsommers/input-knob) at commit `0a0b0b2` (v0.2.1).
They target the Elecrow CrowPanel ESP32-S3 knobs, not this ESP32-C3 board, and are
**not compiled here**. They are starting points for the disc firmware.

| File | What it offers | What must change for the C3 board |
|---|---|---|
| `touch.cpp`, `touch.h` | CST816 polling at I2C `0x15` and tap, long-press and swipe recognition from raw coordinates | Pins (SDA 4, SCL 5, RST 1, INT 0), `Wire` instead of `Wire1`, drop `touchSuppress()` (there is no knob) |
| `display.cpp`, `display.h` | LovyanGFX `Panel_GC9A01` setup and LVGL 9 glue with partial draw buffers | Pins, no LCD reset pin, no power-rail pins, 40 MHz SPI, drop the 1.46" JD9855 panel and board auto-detect, no PSRAM so buffers come from the heap |
| `ble_hid.cpp`, `ble_hid.h` | BLE server, bonding and advertising lifecycle (HID over GATT) | The disc needs its own GATT service for challenge and response; only the connection handling carries over |
| `board.h`, `config.h` | Pin map layout and the touch timing constants the files above use | Replace the pin map with the one in [CAPABILITIES.md](../../CAPABILITIES.md) |

Not copied: the USB HID transport (the C3 cannot be a USB HID device), the rotary
encoder and LED ring drivers (this board has neither), and `upload.sh` (it exists to
clear an ESP32-S3 download-mode flag that does not apply here).
