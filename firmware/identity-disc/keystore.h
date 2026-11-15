// Device key in NVS, HMAC-SHA256, hex helpers. See docs/FIRMWARE_SPEC.md section 4.
// The key is never printed, returned or exported by any command.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <bootloader_random.h>

namespace keystore {

inline uint8_t key[32];
inline bool ready = false;
inline bool selfTestOk = false;

inline void hmac(const uint8_t *k, size_t kl, const uint8_t *d, size_t dl, uint8_t out[32]) {
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), k, kl, d, dl, out);
}

inline void toHex(const uint8_t *in, size_t n, char *out) {
  static const char *H = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    out[2 * i] = H[in[i] >> 4];
    out[2 * i + 1] = H[in[i] & 15];
  }
  out[2 * n] = 0;
}

inline int nib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Exactly 2*n hex characters, nothing else.
inline bool fromHex(const char *s, size_t n, uint8_t *out) {
  if (strlen(s) != 2 * n) return false;
  for (size_t i = 0; i < n; i++) {
    int a = nib(s[2 * i]), b = nib(s[2 * i + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = (a << 4) | b;
  }
  return true;
}

// RFC 4231 test case 2.
inline bool selfTest() {
  static const char *want = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";
  uint8_t out[32];
  char hex[65];
  hmac((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?", 28, out);
  toHex(out, 32, hex);
  return strcmp(hex, want) == 0;
}

inline void generate() {
  // Hardware RNG is only truly random with an entropy source on; there is no radio in M1.
  bootloader_random_enable();
  esp_fill_random(key, sizeof(key));
  bootloader_random_disable();
  Preferences p;
  p.begin("idv", false);
  p.putBytes("key", key, sizeof(key));
  p.end();
}

inline void begin() {
  selfTestOk = selfTest();
  Preferences p;
  p.begin("idv", false);
  size_t n = p.getBytes("key", key, sizeof(key));
  p.end();
  if (n != sizeof(key)) generate();
  ready = true;
}

// prf = HMAC-SHA256(K, "idv-prf-v1" || salt)
inline void prf(const uint8_t salt[32], uint8_t out[32]) {
  uint8_t msg[10 + 32];
  memcpy(msg, "idv-prf-v1", 10);
  memcpy(msg + 10, salt, 32);
  hmac(key, sizeof(key), msg, sizeof(msg), out);
}

// First 8 bytes of HMAC-SHA256(K, "idv-disc-id-v1"), 16 hex chars.
inline void discId(char out[17]) {
  uint8_t full[32];
  hmac(key, sizeof(key), (const uint8_t *)"idv-disc-id-v1", 14, full);
  toHex(full, 8, out);
}

}  // namespace keystore
