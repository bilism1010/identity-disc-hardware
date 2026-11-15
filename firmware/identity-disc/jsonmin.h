// Minimal parser for one flat JSON object whose values are strings or integers.
// Anything else (nesting, arrays, floats, true/false/null, bad escapes) is rejected.
#pragma once
#include <Arduino.h>

struct Request {
  bool hasV = false, hasId = false, hasTimeout = false;
  long v = 0;
  uint32_t id = 0;
  long timeout_s = 0;
  char op[12] = "";
  char salt[72] = "";
  char label[56] = "";
  bool saltOverflow = false, labelOverflow = false, opOverflow = false;
};

namespace jsonmin {

inline void skipWs(const char *&p) {
  while (*p == ' ' || *p == '\t' || *p == '\r') p++;
}

// Parses a quoted string into out (size cap); sets overflow instead of truncating silently.
inline bool str(const char *&p, char *out, size_t cap, bool &overflow) {
  if (*p != '"') return false;
  p++;
  size_t n = 0;
  while (*p && *p != '"') {
    char c = *p++;
    if (c == '\\') {
      char e = *p++;
      if (e == '"' || e == '\\' || e == '/') c = e;
      else return false;
    } else if ((uint8_t)c < 0x20) {
      return false;
    }
    if (n + 1 < cap) out[n++] = c;
    else overflow = true;
  }
  if (*p != '"') return false;
  p++;
  out[n] = 0;
  return true;
}

inline bool integer(const char *&p, long &out) {
  bool neg = false;
  if (*p == '-') { neg = true; p++; }
  if (*p < '0' || *p > '9') return false;
  long v = 0;
  int digits = 0;
  while (*p >= '0' && *p <= '9') {
    v = v * 10 + (*p++ - '0');
    if (++digits > 9) return false;
  }
  if (*p == '.' || *p == 'e' || *p == 'E') return false;
  out = neg ? -v : v;
  return true;
}

inline bool parse(const char *line, Request &r) {
  const char *p = line;
  skipWs(p);
  if (*p++ != '{') return false;
  skipWs(p);
  if (*p == '}') { p++; skipWs(p); return *p == 0; }
  for (;;) {
    char key[16];
    bool ovf = false;
    skipWs(p);
    if (!str(p, key, sizeof(key), ovf) || ovf) return false;
    skipWs(p);
    if (*p++ != ':') return false;
    skipWs(p);
    if (!strcmp(key, "op")) {
      if (!str(p, r.op, sizeof(r.op), r.opOverflow)) return false;
    } else if (!strcmp(key, "salt")) {
      if (!str(p, r.salt, sizeof(r.salt), r.saltOverflow)) return false;
    } else if (!strcmp(key, "label")) {
      if (!str(p, r.label, sizeof(r.label), r.labelOverflow)) return false;
    } else if (!strcmp(key, "v")) {
      if (!integer(p, r.v)) return false;
      r.hasV = true;
    } else if (!strcmp(key, "id")) {
      long t;
      if (!integer(p, t) || t < 0) return false;
      r.id = (uint32_t)t;
      r.hasId = true;
    } else if (!strcmp(key, "timeout_s")) {
      if (!integer(p, r.timeout_s)) return false;
      r.hasTimeout = true;
    } else {
      // Unknown key: accept a string or integer value and ignore it.
      char junk[4];
      bool o2 = false;
      long t;
      if (*p == '"') { if (!str(p, junk, sizeof(junk), o2)) return false; }
      else if (!integer(p, t)) return false;
    }
    skipWs(p);
    if (*p == ',') { p++; continue; }
    if (*p == '}') { p++; break; }
    return false;
  }
  skipWs(p);
  return *p == 0;
}

}  // namespace jsonmin
