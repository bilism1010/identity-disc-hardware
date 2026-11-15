// Board support: GC9A01 display (LovyanGFX) and CST816 touch (polled over I2C).
// Pins from CAPABILITIES.md. Touch INT/RST are assumed pins; INT is not used (polling).
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <LovyanGFX.hpp>

namespace pins {
constexpr int SCLK = 6, MOSI = 7, CS = 10, DC = 2, BL = 3;
constexpr int SDA = 4, SCL = 5, TP_RST = 1;
}  // namespace pins

class Display : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM light;

 public:
  Display() {
    auto b = bus.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 40000000;
    b.pin_sclk = pins::SCLK;
    b.pin_mosi = pins::MOSI;
    b.pin_miso = -1;
    b.pin_dc = pins::DC;
    bus.config(b);
    panel.setBus(&bus);

    auto p = panel.config();
    p.pin_cs = pins::CS;
    p.pin_rst = -1;
    p.panel_width = 240;
    p.panel_height = 240;
    p.invert = true;
    p.readable = false;
    panel.config(p);

    auto l = light.config();
    l.pin_bl = pins::BL;
    l.freq = 5000;
    l.pwm_channel = 0;
    light.config(l);
    panel.setLight(&light);

    setPanel(&panel);
  }
};

namespace touch {
constexpr uint8_t ADDR = 0x15;
inline bool ok = false;

inline void begin() {
  pinMode(pins::TP_RST, OUTPUT);
  digitalWrite(pins::TP_RST, LOW);
  delay(10);
  digitalWrite(pins::TP_RST, HIGH);
  delay(60);
  Wire.begin(pins::SDA, pins::SCL, 400000);
  Wire.beginTransmission(ADDR);
  ok = Wire.endTransmission() == 0;
}

// True while a finger is down; x, y valid only then.
inline bool read(int &x, int &y) {
  if (!ok) return false;
  Wire.beginTransmission(ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDR, (uint8_t)5) != 5) return false;
  uint8_t d[5];
  for (auto &v : d) v = Wire.read();
  if (d[0] == 0) return false;
  x = ((d[1] & 0x0F) << 8) | d[2];
  y = ((d[3] & 0x0F) << 8) | d[4];
  return true;
}
}  // namespace touch
