# Device capabilities: ESP32-C3 1.28" round touch display

Capability sheet for the AliExpress board sold as *"ESP32 LVGL for Arduino Development
Board 1.28 Inch 240\*240 IPS Smart Display Screen LCD TFT Module WiFi & Bluetooth with
Touch"*. Surveyed 2026-11-10 from the unit on the bench.

Each fact is tagged with how it is known:

- **[measured]** read from this unit (esptool, USB descriptors, I2C probe, serial log)
- **[chip]** a property of the ESP32-C3 itself, per Espressif documentation
- **[assumed]** taken from the common ESP32-2424S012 design this board appears to be; not yet checked on this unit

## Identity

| Item | Value | Source |
|---|---|---|
| SoC | ESP32-C3, revision v0.4 | measured |
| Crystal | 40 MHz | measured |
| Flash | 4 MB SPI, JEDEC manufacturer 0x20, device 0x4016 | measured |
| MAC | `48:31:b7:3d:40:10` | measured |
| USB | Espressif "USB JTAG/serial debug unit", VID:PID `303A:1001`, full speed | measured |
| Serial port (macOS) | `/dev/cu.usbmodem2141301` (name follows the hub port, so it changes if replugged elsewhere) | measured |
| Board model | ESP32-2424S012 or a clone of it | assumed |

## Processor and memory

| Item | Value | Source |
|---|---|---|
| CPU | Single-core 32-bit RISC-V (RV32IMC), up to 160 MHz | chip |
| RAM | 400 KB SRAM on chip, about 320 KB usable by an Arduino sketch | chip / measured at link time |
| PSRAM | None; the C3 has no PSRAM support | chip |
| App space | 1.25 MB with the default 4 MB partition table | measured |

A full-screen RGB565 framebuffer is 115 KB, so one fits in RAM but two do not. LVGL
should use partial draw buffers.

## Display

| Item | Value | Source |
|---|---|---|
| Panel | 1.28" round IPS, 240x240 | assumed (seller listing) |
| Controller | GC9A01, 4-wire SPI, write-only | assumed; driver initialises without error |
| Colour | RGB565, inversion on | assumed |
| Backlight | PWM on GPIO3 | assumed |

**Not yet confirmed by eye.** The test sketch runs and logs `display up`, but that log
line does not depend on the panel answering (the bus is write-only). Someone needs to
look at the screen and confirm the colour bars and counter.

## Touch

| Item | Value | Source |
|---|---|---|
| Controller | CST816 family, I2C address `0x15` | measured (ACKs on the bus) |
| Bus | I2C at 400 kHz, SDA GPIO4, SCL GPIO5 | measured |
| Reset / interrupt | RST GPIO1, INT GPIO0 | assumed |
| Points | Single touch, plus built-in gesture codes | chip family |

Coordinates have not been checked against the screen yet.

## Pin map

| Function | GPIO | Source |
|---|---|---|
| LCD SCLK | 6 | assumed |
| LCD MOSI | 7 | assumed |
| LCD CS | 10 | assumed |
| LCD DC | 2 | assumed |
| LCD RST | none (tied to board reset) | assumed |
| LCD backlight | 3 | assumed |
| Touch SDA | 4 | measured |
| Touch SCL | 5 | measured |
| Touch INT | 0 | assumed |
| Touch RST | 1 | assumed |
| USB D- / D+ | 18 / 19 | chip |
| BOOT button / strapping | 9 | chip |
| Strapping | 8 | chip |
| UART0 RX / TX | 20 / 21 | chip |

The display and touch use nearly every pin. Whether GPIO 8, 9, 20 or 21 reach a
connector on this board is unknown. Treat the board as having no spare I/O until the
connectors are traced.

## Radios

| Item | Value | Source |
|---|---|---|
| WiFi | 802.11 b/g/n, 2.4 GHz only | chip |
| Bluetooth | Bluetooth 5 LE only; no Bluetooth Classic | chip |
| Antenna | Unknown (PCB or ceramic) | not checked |

## USB

The C3's USB is a fixed-function serial + JTAG bridge. **It cannot be a USB HID
device, a USB FIDO2/CTAP key, mass storage or any other custom USB class.** Over the
cable the board is only a serial port (CDC) and a JTAG debugger.

Ways to talk to a host:

- Serial over USB (works today, 115200 or faster, no driver needed)
- BLE GATT, including BLE HID and FIDO-over-BLE
- WiFi (TCP, HTTP, WebSocket, ESP-NOW)

## Security hardware

All of these are chip features and none has been enabled or tested on this unit.

| Feature | Notes |
|---|---|
| Hardware RNG | True random when a radio is on |
| Accelerators | AES, SHA, RSA / big-number |
| eFuse key slots | 6 key blocks, can be read-protected from software |
| HMAC peripheral | HMAC-SHA256 with an eFuse key that software cannot read |
| Digital Signature peripheral | RSA signing with a key encrypted to an eFuse key |
| Secure Boot v2 | RSA-3072 signed bootloader and app |
| Flash encryption | AES-XTS; protects data at rest in flash |
| Secure element | **None.** There is no discrete secure element and no ECC key storage in hardware |

eFuse operations are permanent. Secure Boot and flash encryption cannot be turned off
once burned, and a mistake can brick the board.

## Not present or unknown

- No rotary encoder, no buttons other than BOOT/RESET (assumed), no speaker, no IMU
- No SD card slot (assumed)
- Battery connector and charger: common on this design, not checked here
- Power draw: not measured

## Toolchain that works

| Item | Value |
|---|---|
| Core | Arduino `esp32:esp32` 3.0.7 |
| FQBN | `esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashSize=4M` |
| Graphics | LovyanGFX 1.2.26 (`Panel_GC9A01`, `Bus_SPI` on `SPI2_HOST`, 40 MHz) |
| Flasher | esptool 4.6 from the Arduino core, 921600 baud |
| Test sketch | `examples/c3-round-test/` (358 KB, 27% of app space) |

Notes:

- `CDCOnBoot=cdc` is required, or `Serial` output does not appear on the USB port.
- Opening the serial port resets the board.
- Flashing needs no button presses; esptool enters and leaves the bootloader itself.
- The Homebrew/pip `esptool` on this Mac is broken (click version clash). Use the one
  under `~/Library/Arduino15/packages/esp32/tools/esptool_py/4.6/`.
- Other ESP32 boards are plugged into the same Mac. Always pass `--port` explicitly and
  check the MAC before writing.

## Original firmware

The factory firmware was read out in full (4 MB) before the test sketch was flashed.
The image is kept in `backup/` (not committed). Restore with:

```sh
esptool --chip esp32c3 --port <port> write_flash 0x0 backup/c3-original-flash-4MB.bin
```

## Fit for Identity Disc Vault

The first example application is a prototype of the Identity Disc Vault hardware disc
(`/Users/bill/src/research/systems-programming/indentity-disc-vault`).

What the board can do for it:

- Show a round "disc" UI and take a tap as proof of presence
- Hold a device key and answer challenges over USB serial or BLE
- Use the HMAC or Digital Signature peripheral so the key is not readable by firmware

What it cannot do:

- Act as a USB FIDO2/U2F security key (no programmable USB)
- Offer secure-element grade key protection or native ECC key storage
- Do WebAuthn PRF as a platform or USB authenticator; the host CLI would need a custom
  serial or BLE transport instead
