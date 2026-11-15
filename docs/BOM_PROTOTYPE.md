# Prototype bill of materials and hardware

The bench prototype uses one off-the-shelf board. The Rev 1.0 purpose-built disc has its
own bill of materials in [../hardware/bom/BILL_OF_MATERIALS.md](../hardware/bom/BILL_OF_MATERIALS.md);
the two are different designs, not revisions of each other.

## Bill of materials

| Item | Part | Qty | Cost | Notes |
|---|---|---|---|---|
| Board | AliExpress "ESP32 LVGL for Arduino Development Board 1.28 Inch 240x240 IPS Smart Display Screen LCD TFT Module WiFi & Bluetooth with Touch" (ESP32-2424S012 or clone) | 1 | not recorded | Contains everything below |
| USB cable | USB-C to USB-A or C, data capable | 1 | not recorded | Power and serial. A charge-only cable shows no serial port |

Nothing else is required. There are no external components, wiring or soldering.

## What is on the board

All values are from [../CAPABILITIES.md](../CAPABILITIES.md), which tags each as measured,
chip or assumed.

| Block | Part | Detail |
|---|---|---|
| SoC | ESP32-C3, rev v0.4 | Single-core RISC-V up to 160 MHz, 400 KB SRAM, no PSRAM, 40 MHz crystal (measured) |
| Flash | 4 MB SPI, JEDEC 0x20 0x4016 | About 1.25 MB app space with the default partition table (measured) |
| Display | 1.28" round IPS, 240x240, GC9A01, 4-wire SPI write-only, RGB565 | Controller and inversion assumed; confirmed by eye on the bench |
| Backlight | PWM on GPIO3 | Assumed |
| Touch | CST816 family, I2C `0x15`, single touch | Address and bus measured; reset and interrupt pins assumed |
| Radios | WiFi 802.11 b/g/n 2.4 GHz, Bluetooth 5 LE | No Bluetooth Classic. Antenna type unchecked |
| USB | Fixed-function serial and JTAG, VID:PID `303A:1001` | Cannot be HID or FIDO2 |
| Security | HMAC, AES, SHA, RSA accelerators; eFuse key blocks; Secure Boot v2; flash encryption | None enabled. No secure element |

## Pin map

| Function | GPIO | Source |
|---|---|---|
| LCD SCLK / MOSI / CS / DC | 6 / 7 / 10 / 2 | assumed (works) |
| LCD reset | none (tied to board reset) | assumed |
| LCD backlight | 3 | assumed |
| Touch SDA / SCL | 4 / 5 | measured |
| Touch reset / interrupt | 1 / 0 | assumed; interrupt unused (polling) |
| USB D- / D+ | 18 / 19 | chip |
| BOOT / strapping | 9 / 8 | chip |
| UART0 RX / TX | 20 / 21 | chip |

The display and touch use nearly every pin. Treat the board as having no spare I/O.

## Identity of the bench unit

| Item | Value |
|---|---|
| MAC (also the USB serial number) | `48:31:B7:3D:40:10` |
| Serial port on macOS | follows the hub port, currently `/dev/cu.usbmodem2141301`; found by `tools/find-disc.py`, never hard-coded |
| `disc_id` of the current key | shown by `disc_test.py info`; changes if the key is reset |

## Host tooling (not part of the unit, needed to build)

| Tool | Version | Notes |
|---|---|---|
| Arduino core `esp32:esp32` | 3.0.7 | FQBN `esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashSize=4M` |
| LovyanGFX | 1.2.26 | Display driver |
| `arduino-cli` | the one bundled with Arduino IDE | Not on PATH on the bench Mac |
| esptool | 4.6 from the Arduino core | The Homebrew/pip esptool is broken on the bench Mac |
| Python 3 with pyserial | pyserial 3.5 | For `tools/` |
| NimBLE-Arduino, `bleak` | not installed | Needed from M2 |

## Differences from the Rev 1.0 design

| | Bench prototype | Rev 1.0 |
|---|---|---|
| SoC | ESP32-C3 | ESP32-S3-MINI-1-N8 |
| USB | Serial only | Native USB HID / CTAP2.1 |
| Key store | Plain NVS | ATECC608B secure element |
| Ring | Drawn on the screen rim | 16 WS2812B LEDs |
| Presence input | Capacitive touchscreen | Capacitive touch ring |
| Display | 1.28" round IPS | None in the BOM; LED ring and touch only |
