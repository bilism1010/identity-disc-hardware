// Identity Disc prototype firmware, milestone M1 (USB serial).
// Spec: docs/FIRMWARE_SPEC.md. Protocol: one JSON line per message over USB CDC.
// Host lines in, replies out on the same port; debug lines start with '#'.
#include <Arduino.h>
#include "board.h"
#include "keystore.h"
#include "jsonmin.h"
#include "glow.h"

#define FW_VERSION "0.1.2-m1"
#define DEVICE_NAME "identity-disc"
static const int PROTO = 1;

static const uint32_t HOLD_APPROVE_MS = 1200;
static const uint32_t COOLDOWN_MS = 3000;
static const uint32_t RESULT_MS = 1000;
static const uint32_t DEFAULT_TIMEOUT_S = 30;
static const size_t MAX_LINE = 512;

static Display tft;

enum class State { Idle, Request, Result, Demo };
static State state = State::Idle;

// Demo: cycles the ring through every look so the visuals can be seen without a host.
static const uint32_t DEMO_STEP_MS = 4500;
static int demoStep = 0;
static uint32_t demoUntil = 0;

// Pending prf request
static uint32_t pendId = 0;
static uint8_t pendSalt[32];
static char pendLabel[56];
static uint32_t pendDeadline = 0, pendStarted = 0;
static uint32_t cooldownUntil = 0;
static bool needRelease = false;
static uint32_t holdStart = 0;
static bool holding = false;
static int lastFill = -1, lastSecs = -1;

static uint32_t resultUntil = 0;
static char discId[17];

// ---------- replies ----------

static void sendLine(const char *s) { Serial.println(s); }

static void replyErr(bool hasId, uint32_t id, const char *err) {
  char b[96];
  if (hasId) snprintf(b, sizeof(b), "{\"id\":%lu,\"ok\":false,\"err\":\"%s\"}", (unsigned long)id, err);
  else snprintf(b, sizeof(b), "{\"id\":null,\"ok\":false,\"err\":\"%s\"}", err);
  sendLine(b);
}

static void replyOk(uint32_t id) {
  char b[48];
  snprintf(b, sizeof(b), "{\"id\":%lu,\"ok\":true}", (unsigned long)id);
  sendLine(b);
}

// ---------- screens ----------

static void ring() { tft.drawCircle(120, 120, 119, TFT_DARKGREY); }

static void drawIdle() {
  tft.fillScreen(TFT_BLACK);
  ring();
  tft.setTextDatum(middle_center);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Identity Disc", 120, 85);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("USB ready", 120, 125);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  char b[24];
  snprintf(b, sizeof(b), "id %s", discId);
  tft.drawString(b, 120, 150);
  tft.drawString("fw " FW_VERSION, 120, 170);
  glow::set(glow::Standby);
  glow::invalidate();
}

// Wrap label into at most two lines of up to 17 characters.
static void drawRequestStatic() {
  tft.fillScreen(TFT_BLACK);
  ring();
  tft.setTextDatum(middle_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("APPROVE?", 120, 22);

  char l1[24] = "", l2[24] = "";
  size_t n = strlen(pendLabel);
  const size_t W = 17;
  if (n <= W) {
    strcpy(l1, pendLabel);
  } else {
    size_t cut = W;
    for (size_t i = W; i > 0; i--)
      if (pendLabel[i] == ' ') { cut = i; break; }
    memcpy(l1, pendLabel, cut);
    l1[cut] = 0;
    const char *rest = pendLabel + cut + (pendLabel[cut] == ' ' ? 1 : 0);
    strncpy(l2, rest, W);
    l2[W] = 0;
  }
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(l1, 120, l2[0] ? 55 : 70);
  if (l2[0]) tft.drawString(l2, 120, 82);

  tft.setFont(&fonts::Font2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("HOLD", 120, 148);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawLine(70, 192, 170, 192, TFT_DARKGREY);
  tft.drawString("tap here to deny", 120, 212);
  lastFill = -1;
  lastSecs = -1;
  glow::set(glow::Presence);
  glow::invalidate();
}

static void drawHoldFill(int pct) {
  if (pct == lastFill) return;
  lastFill = pct;
  tft.fillArc(120, 148, 32, 40, 0, 360, TFT_BLACK);  // clear track
  tft.drawCircle(120, 148, 36, TFT_DARKGREY);
  if (pct > 0) tft.fillArc(120, 148, 32, 40, 270, 270 + 360 * pct / 100, TFT_GREEN);
}

static void drawSecs(int s) {
  if (s == lastSecs) return;
  lastSecs = s;
  char b[12];
  snprintf(b, sizeof(b), "%d s", s);
  tft.setFont(&fonts::Font2);
  tft.setTextDatum(middle_center);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.fillRect(90, 32, 60, 14, TFT_BLACK);
  tft.drawString(b, 120, 40);
}

static void drawResult(const char *text, uint16_t color, glow::Mode m) {
  tft.fillScreen(TFT_BLACK);
  ring();
  tft.setTextDatum(middle_center);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, 120, 120);
  glow::set(m);
  glow::invalidate();
}

// Demo screen: always says DEMO so it can never be mistaken for a real request.
static const char *DEMO_NAMES[] = {"Standby", "Presence", "Warning", "Alert", "Verified"};
static const glow::Mode DEMO_MODES[] = {glow::Standby, glow::Presence, glow::Warning, glow::Alert, glow::Ok};
static const uint16_t DEMO_COLORS[] = {TFT_CYAN, TFT_ORANGE, TFT_ORANGE, TFT_RED, TFT_GREEN};

static void drawDemo(int step) {
  tft.fillScreen(TFT_BLACK);
  ring();
  tft.setTextDatum(middle_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("DEMO - not a request", 120, 70);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(DEMO_COLORS[step], TFT_BLACK);
  tft.drawString(DEMO_NAMES[step], 120, 120);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("touch to exit", 120, 170);
  glow::set(DEMO_MODES[step]);
  glow::invalidate();
}

static void finish(const char *text, uint16_t color, glow::Mode m) {
  drawResult(text, color, m);
  state = State::Result;
  resultUntil = millis() + RESULT_MS;
  holding = false;
}

// ---------- protocol ----------

static bool labelOk(const char *s) {
  size_t n = strlen(s);
  if (n < 1 || n > 48) return false;
  for (size_t i = 0; i < n; i++)
    if ((uint8_t)s[i] < 0x20 || (uint8_t)s[i] > 0x7e) return false;
  return true;
}

static void handleLine(const char *line) {
  Request r;
  if (!jsonmin::parse(line, r)) { replyErr(false, 0, "bad_request"); return; }
  bool hid = r.hasId;
  if (!r.hasV || !hid) { replyErr(hid, r.id, "bad_request"); return; }
  if (r.v != PROTO) { replyErr(true, r.id, "bad_version"); return; }
  if (r.opOverflow) { replyErr(true, r.id, "bad_request"); return; }

  if (!strcmp(r.op, "ping")) { replyOk(r.id); return; }

  if (!strcmp(r.op, "info")) {
    char b[256];
    snprintf(b, sizeof(b),
             "{\"id\":%lu,\"ok\":true,\"device\":\"" DEVICE_NAME "\",\"proto\":%d,\"fw\":\"%s\",\"disc_id\":\"%s\",\"transport\":\"serial\"}",
             (unsigned long)r.id, PROTO, FW_VERSION, discId);
    sendLine(b);
    return;
  }

  if (!strcmp(r.op, "cancel")) {
    if (state == State::Request) {
      replyErr(true, pendId, "cancelled");
      finish("Cancelled", TFT_ORANGE, glow::Warning);
    } else if (state == State::Demo) {
      state = State::Idle;
      drawIdle();
    }
    replyOk(r.id);
    return;
  }

  if (!strcmp(r.op, "demo")) {  // visuals only: touches no key and answers no request
    if (state != State::Idle) { replyErr(true, r.id, "busy"); return; }
    demoStep = 0;
    demoUntil = millis() + DEMO_STEP_MS;
    state = State::Demo;
    drawDemo(0);
    replyOk(r.id);
    return;
  }

  if (!strcmp(r.op, "prf")) {
    uint8_t salt[32];
    if (r.saltOverflow || r.labelOverflow || !keystore::fromHex(r.salt, 32, salt) || !labelOk(r.label) ||
        (r.hasTimeout && (r.timeout_s < 5 || r.timeout_s > 120))) {
      replyErr(true, r.id, "bad_request");
      return;
    }
    if (!keystore::selfTestOk) { replyErr(true, r.id, "bad_request"); return; }
    if (state == State::Request || state == State::Demo) { replyErr(true, r.id, "busy"); return; }
    if (state == State::Result || (int32_t)(millis() - cooldownUntil) < 0) { replyErr(true, r.id, "cooldown"); return; }
    pendId = r.id;
    memcpy(pendSalt, salt, 32);
    strcpy(pendLabel, r.label);
    pendStarted = millis();
    pendDeadline = pendStarted + 1000UL * (r.hasTimeout ? r.timeout_s : DEFAULT_TIMEOUT_S);
    holding = false;
    int tx, ty;
    needRelease = touch::read(tx, ty);  // a finger already down must lift first
    state = State::Request;
    drawRequestStatic();
    sendLine("{\"evt\":\"touch_wait\"}");
    return;
  }

  replyErr(true, r.id, "bad_request");
}

static char lineBuf[MAX_LINE + 1];
static size_t lineLen = 0;
static bool lineOverflow = false;

static void pollSerial() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (lineOverflow) replyErr(false, 0, "bad_request");
      else if (lineLen > 0) { lineBuf[lineLen] = 0; handleLine(lineBuf); }
      lineLen = 0;
      lineOverflow = false;
      continue;
    }
    if (lineLen < MAX_LINE) lineBuf[lineLen++] = (char)c;
    else lineOverflow = true;
  }
}

// ---------- request handling (touch) ----------

static void tickRequest() {
  uint32_t now = millis();
  if ((int32_t)(now - pendDeadline) >= 0) {
    replyErr(true, pendId, "timeout");
    cooldownUntil = now + COOLDOWN_MS;
    finish("Timed out", TFT_ORANGE, glow::Warning);
    return;
  }
  drawSecs((int)((pendDeadline - now + 999) / 1000));

  int x, y;
  bool down = touch::read(x, y);
  if (!down) { needRelease = false; holding = false; drawHoldFill(0); return; }
  if (needRelease) return;

  if (y >= 195) {  // deny zone
    replyErr(true, pendId, "denied");
    cooldownUntil = now + COOLDOWN_MS;
    finish("Denied", TFT_RED, glow::Alert);
    return;
  }
  int dx = x - 120, dy = y - 148;
  if (dx * dx + dy * dy > 60 * 60) { holding = false; drawHoldFill(0); return; }

  if (!holding) { holding = true; holdStart = now; }
  uint32_t held = now - holdStart;
  drawHoldFill((int)min<uint32_t>(100, held * 100 / HOLD_APPROVE_MS));
  if (held >= HOLD_APPROVE_MS) {
    uint8_t out[32];
    char hex[65], b[140];
    keystore::prf(pendSalt, out);
    keystore::toHex(out, 32, hex);
    snprintf(b, sizeof(b), "{\"id\":%lu,\"ok\":true,\"prf\":\"%s\"}", (unsigned long)pendId, hex);
    sendLine(b);
    finish("Approved", TFT_GREEN, glow::Ok);
  }
}

// ---------- main ----------

void setup() {
  Serial.begin(115200);
  touch::begin();
  tft.init();
  tft.setBrightness(200);
  keystore::begin();
  keystore::discId(discId);
  drawIdle();

  Serial.printf("# identity-disc %s touch=%s selftest=%s\n", FW_VERSION, touch::ok ? "ok" : "none",
                keystore::selfTestOk ? "ok" : "FAIL");
  char b[96];
  snprintf(b, sizeof(b), "{\"evt\":\"ready\",\"proto\":%d,\"fw\":\"%s\"}", PROTO, FW_VERSION);
  sendLine(b);
}

void loop() {
  pollSerial();
  switch (state) {
    case State::Idle: break;
    case State::Request: tickRequest(); break;
    case State::Result:
      if ((int32_t)(millis() - resultUntil) >= 0) { state = State::Idle; drawIdle(); }
      break;
    case State::Demo: {
      int tx, ty;
      if (touch::read(tx, ty)) { state = State::Idle; drawIdle(); break; }
      if ((int32_t)(millis() - demoUntil) >= 0) {
        if (++demoStep >= 5) { state = State::Idle; drawIdle(); }
        else { drawDemo(demoStep); demoUntil = millis() + DEMO_STEP_MS; }
      }
      break;
    }
  }
  glow::tick(tft, millis());
  delay(10);
}
