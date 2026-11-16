// Glowing status ring drawn on the GC9A01 rim with TRON styling.
// Multi-layer concentric TRON rings, rotating rune markers, and slow breathing pulse:
//   Standby  neon cyan/blue  slow, calm breathing pulse (~0.5 Hz)
//   Presence amber           brisk, pulsing awaiting touch
//   Warning  amber           faster urgent pulse
//   Alert    pulsing red     rapid high-intensity strobe
//   Ok       neon emerald    calm verification surge
#pragma once
#include <Arduino.h>
#include <math.h>
#include "board.h"

namespace glow {

enum Mode { Standby, Presence, Warning, Alert, Ok };

struct RGB { uint8_t r, g, b; };
struct Look {
  RGB dim, peak;
  RGB innerDim, innerPeak;
  float pulseSpeed, rotSpeed;
};

static const Look LOOKS[] = {
  /* Standby  */ {
    {0x00, 0x33, 0x55}, {0x00, 0xF3, 0xFF},  // Neon Electric Cyan Core
    {0x00, 0x1A, 0x33}, {0x00, 0x88, 0xCC},  // Deep Cobalt Inner Glow
    1.6f, 0.75f
  },
  /* Presence */ {
    {0x55, 0x2A, 0x00}, {0xFF, 0x9E, 0x00},  // Glowing Amber
    {0x33, 0x18, 0x00}, {0xFF, 0xCC, 0x00},
    3.5f, 1.3f
  },
  /* Warning  */ {
    {0x66, 0x30, 0x00}, {0xFF, 0x88, 0x00},  // Warning Amber
    {0x38, 0x15, 0x00}, {0xFF, 0xAA, 0x00},
    5.2f, 2.2f
  },
  /* Alert    */ {
    {0x55, 0x00, 0x10}, {0xFF, 0x00, 0x3C},  // Pulsing Alarm Red
    {0x30, 0x00, 0x08}, {0xFF, 0x33, 0x66},
    10.0f, 4.8f
  },
  /* Ok       */ {
    {0x00, 0x44, 0x1A}, {0x00, 0xFF, 0x88},  // Verified Emerald Surge
    {0x00, 0x22, 0x0D}, {0x44, 0xFF, 0xAA},
    2.0f, 0.9f
  },
};

static const int SEGS = 24;
static const uint32_t FRAME_MS = 33;  // ~30 frames per second
static const float TWO_PI_F = 6.28318530718f;

inline Mode mode = Standby;
inline float pulse = 0, rot = 0;
inline uint32_t lastMs = 0;
inline uint16_t drawnOuter[SEGS];
inline uint16_t drawnInner[SEGS];
inline bool dirty = true;

inline void invalidate() { dirty = true; }  // call after anything clears the screen
inline void set(Mode m) { mode = m; }

inline uint8_t lerp8(uint8_t a, uint8_t b, float t) { return (uint8_t)(a + (b - a) * t); }

inline void tick(Display &d, uint32_t now) {
  if (now - lastMs < FRAME_MS) return;
  float dt = (now - lastMs) / 1000.0f;
  if (dt > 0.25f) dt = 0.25f;
  lastMs = now;

  const Look &L = LOOKS[mode];
  pulse = fmodf(pulse + L.pulseSpeed * dt, TWO_PI_F);
  rot = fmodf(rot + L.rotSpeed * dt, TWO_PI_F);

  // Breathing pulse curve: smooth sinusoidal modulation
  float amp = 0.55f + 0.45f * sinf(pulse);

  // If dirty, draw static TRON concentric conduit rings & cardinal tick accents
  if (dirty) {
    uint16_t darkCyan = (mode == Alert) ? d.color565(0x35, 0x00, 0x08)
                      : (mode == Warning || mode == Presence) ? d.color565(0x35, 0x18, 0x00)
                      : d.color565(0x00, 0x22, 0x33);
    d.drawCircle(120, 120, 119, darkCyan);
    d.drawCircle(120, 120, 109, darkCyan);
    d.drawCircle(120, 120, 99, darkCyan);

    // Cardinal tick markers
    uint16_t markColor = (mode == Alert) ? d.color565(0xAA, 0x00, 0x20)
                       : (mode == Warning || mode == Presence) ? d.color565(0xAA, 0x60, 0x00)
                       : d.color565(0x00, 0x55, 0x77);
    d.drawFastHLine(1, 120, 9, markColor);
    d.drawFastHLine(230, 120, 9, markColor);
    d.drawFastVLine(120, 1, 9, markColor);
    d.drawFastVLine(120, 230, 9, markColor);
  }

  for (int i = 0; i < SEGS; i++) {
    float ang = (float)i / SEGS * TWO_PI_F;
    float diffOuter = fmodf(ang - rot + 2.0f * TWO_PI_F, TWO_PI_F);
    float diffInner = fmodf(ang + rot + 2.0f * TWO_PI_F, TWO_PI_F);

    // Outer ring with clockwise chaser flare
    float bOuter = amp * 0.72f + expf(-diffOuter * 3.8f) * 0.28f;
    if (bOuter > 1.0f) bOuter = 1.0f;
    uint16_t cOuter = d.color565(lerp8(L.dim.r, L.peak.r, bOuter),
                                lerp8(L.dim.g, L.peak.g, bOuter),
                                lerp8(L.dim.b, L.peak.b, bOuter));

    // Inner ring with counter-clockwise chaser flare
    float bInner = amp * 0.65f + expf(-diffInner * 4.2f) * 0.35f;
    if (bInner > 1.0f) bInner = 1.0f;
    uint16_t cInner = d.color565(lerp8(L.innerDim.r, L.innerPeak.r, bInner),
                                lerp8(L.innerDim.g, L.innerPeak.g, bInner),
                                lerp8(L.innerDim.b, L.innerPeak.b, bInner));

    float a0 = i * (360.0f / SEGS);

    // Render outer neon arc segments (radius 111..118)
    if (dirty || cOuter != drawnOuter[i]) {
      d.fillArc(120, 120, 111, 118, a0, a0 + 13.0f, cOuter);
      drawnOuter[i] = cOuter;
    }

    // Render inner concentric TRON conduit ring (radius 101..107)
    if (dirty || cInner != drawnInner[i]) {
      d.fillArc(120, 120, 101, 107, a0 + 3.0f, a0 + 12.0f, cInner);
      drawnInner[i] = cInner;
    }
  }
  dirty = false;
}

}  // namespace glow
