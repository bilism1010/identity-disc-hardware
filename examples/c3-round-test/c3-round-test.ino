// Board check for the ESP32-C3 1.28" round display (ESP32-2424S012 pinout):
// GC9A01 240x240 over SPI, CST816 touch over I2C.
// Shows colour bars, then a ticking counter; touching the screen draws a dot.
#include <Arduino.h>
#include <Wire.h>
#include <LovyanGFX.hpp>

static const int PIN_SCLK = 6, PIN_MOSI = 7, PIN_CS = 10, PIN_DC = 2, PIN_BL = 3;
static const int PIN_SDA = 4, PIN_SCL = 5, PIN_TP_INT = 0, PIN_TP_RST = 1;
static const uint8_t CST816_ADDR = 0x15;

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
    b.pin_sclk = PIN_SCLK;
    b.pin_mosi = PIN_MOSI;
    b.pin_miso = -1;
    b.pin_dc = PIN_DC;
    bus.config(b);
    panel.setBus(&bus);

    auto p = panel.config();
    p.pin_cs = PIN_CS;
    p.pin_rst = -1;
    p.panel_width = 240;
    p.panel_height = 240;
    p.invert = true;
    p.readable = false;
    panel.config(p);

    auto l = light.config();
    l.pin_bl = PIN_BL;
    l.freq = 5000;
    l.pwm_channel = 0;
    light.config(l);
    panel.setLight(&light);

    setPanel(&panel);
  }
};

static Display tft;
static bool touchOk = false;

static bool touchRead(int &x, int &y) {
  Wire.beginTransmission(CST816_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(CST816_ADDR, (uint8_t)5) != 5) return false;
  uint8_t d[5];
  for (auto &v : d) v = Wire.read();
  if (d[0] == 0) return false;
  x = ((d[1] & 0x0F) << 8) | d[2];
  y = ((d[3] & 0x0F) << 8) | d[4];
  return true;
}

static void drawHome() {
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 119, TFT_WHITE);
  tft.setTextDatum(middle_center);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.drawString("Board OK", 120, 70);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("ESP32-C3  GC9A01 240x240", 120, 100);
  tft.setTextColor(touchOk ? TFT_CYAN : TFT_ORANGE, TFT_BLACK);
  tft.drawString(touchOk ? "touch: ok - draw on me" : "touch: not found", 120, 180);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(10);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(60);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  Wire.beginTransmission(CST816_ADDR);
  touchOk = Wire.endTransmission() == 0;

  tft.init();
  tft.setBrightness(200);

  const uint16_t bars[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE};
  for (auto c : bars) {
    tft.fillScreen(c);
    delay(350);
  }
  drawHome();
  Serial.printf("[c3-round-test] display up, touch=%s\n", touchOk ? "ok" : "none");
}

void loop() {
  static uint32_t lastTick = 0, seconds = 0;
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    seconds++;
    tft.setFont(&fonts::Font7);
    tft.setTextDatum(middle_center);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char buf[8];
    snprintf(buf, sizeof(buf), "%04lu", (unsigned long)(seconds % 10000));
    tft.drawString(buf, 120, 140);
    Serial.printf("[c3-round-test] t=%lus\n", (unsigned long)seconds);
  }

  int x, y;
  if (touchOk && touchRead(x, y)) {
    tft.fillCircle(x, y, 4, TFT_MAGENTA);
    Serial.printf("[c3-round-test] touch %d,%d\n", x, y);
  }
  delay(10);
}
