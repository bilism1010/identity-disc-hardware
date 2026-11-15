// Glowing status ring drawn on the GC9A01 rim. Same behaviour as the vault's 16-LED ring
// (indentity-disc-vault/firmware/esp32/src/disc_display.c): a breathing pulse whose speed
// rises with urgency, plus a chaser that circles the ring.
//   Standby  cyan   slow, calm            Presence amber  brisk   (waiting for a hold)
//   Warning  amber  faster                Alert    red    rapid strobe
//   Ok       green  calm
#pragma once
#include <Arduino.h>
#include <math.h>
#include "board.h"

namespace glow {

enum Mode { Standby, Presence, Warning, Alert, Ok };

struct RGB { uint8_t r, g, b; };
struct Look { RGB dim, peak; float pulseSpeed, rotSpeed; };  // rad/s, as in the vault

static const Look LOOKS[] = {
  /* Standby  */ {{0x00, 0x44, 0x66}, {0x00, 0xF3, 0xFF}, 1.8f, 0.8f},
  /* Presence */ {{0x66, 0x38, 0x00}, {0xFF, 0x9E, 0x00}, 3.5f, 1.2f},
  /* Warning  */ {{0x66, 0x38, 0x00}, {0xFF, 0x9E, 0x00}, 5.0f, 2.0f},
  /* Alert    */ {{0x55, 0x00, 0x10}, {0xFF, 0x00, 0x3C}, 10.0f, 4.5f},
  /* Ok       */ {{0x00, 0x55, 0x22}, {0x00, 0xFF, 0x88}, 1.8f, 0.8f},
};

static const int SEGS = 24;
static const uint32_t FRAME_MS = 40;  // 25 frames per second
static const float TWO_PI_F = 6.28318530718f;

inline Mode mode = Standby;
inline float pulse = 0, rot = 0;
inline uint32_t lastMs = 0;
inline uint16_t drawn[SEGS];
inline bool dirty = true;

inline void invalidate() { dirty = true; }  // call after anything that clears the screen
inline void set(Mode m) { mode = m; }

inline uint8_t lerp8(uint8_t a, uint8_t b, float t) { return (uint8_t)(a + (b - a) * t); }

inline void tick(Display &d, uint32_t now) {
  if (now - lastMs < FRAME_MS) return;
  float dt = (now - lastMs) / 1000.0f;
  if (dt > 0.25f) dt = 0.25f;  // after a long stall, don't jump
  lastMs = now;
  const Look &L = LOOKS[mode];
  pulse = fmodf(pulse + L.pulseSpeed * dt, TWO_PI_F);
  rot = fmodf(rot + L.rotSpeed * dt, TWO_PI_F);
  float amp = 0.625f + 0.375f * sinf(pulse);

  for (int i = 0; i < SEGS; i++) {
    float ang = (float)i / SEGS * TWO_PI_F;
    float diff = fmodf(ang - rot + 2 * TWO_PI_F, TWO_PI_F);
    float b = amp * 0.75f + expf(-diff * 3.5f) * 0.25f;
    if (b > 1.0f) b = 1.0f;
    uint16_t c = d.color565(lerp8(L.dim.r, L.peak.r, b), lerp8(L.dim.g, L.peak.g, b),
                            lerp8(L.dim.b, L.peak.b, b));
    if (!dirty && c == drawn[i]) continue;
    float a0 = i * (360.0f / SEGS);
    d.fillArc(120, 120, 112, 119, a0, a0 + 13.0f, c);  // 2 degree gap between segments
    drawn[i] = c;
  }
  dirty = false;
}

}  // namespace glow
