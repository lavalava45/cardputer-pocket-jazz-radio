// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketjazz {
// Count complete UTF-8 codepoints without crossing a source or destination
// byte bound. The LCD may lack a glyph, but it must never receive half one.
// Malformed byte sequences terminate the prefix rather than split blindly.
inline size_t utf8SafePrefix(const char* src, size_t sourceBytes, size_t maxBytes) {
  if (!src) return 0;
  size_t n = 0;
  while (n < sourceBytes && n < maxBytes && src[n]) {
    const unsigned char lead = static_cast<unsigned char>(src[n]);
    const size_t width = lead < 0x80 ? 1 :
                         (lead >= 0xC2 && lead <= 0xDF) ? 2 :
                         (lead >= 0xE0 && lead <= 0xEF) ? 3 :
                         (lead >= 0xF0 && lead <= 0xF4) ? 4 : 0;
    if (!width || width > sourceBytes - n || width > maxBytes - n) break;
    if (width > 1) {
      const auto second = static_cast<unsigned char>(src[n + 1]);
      // Reject overlong encodings, UTF-16 surrogates and out-of-range scalars.
      if ((lead == 0xE0 && second < 0xA0) ||
          (lead == 0xED && second >= 0xA0) ||
          (lead == 0xF0 && second < 0x90) ||
          (lead == 0xF4 && second >= 0x90)) break;
    }
    bool complete = true;
    for (size_t i = 1; i < width; ++i) {
      const auto c = static_cast<unsigned char>(src[n + i]);
      if (c == 0 || (c & 0xC0) != 0x80) { complete = false; break; }
    }
    if (!complete) break;
    n += width;
  }
  return n;
}

inline size_t copyUtf8Bounded(char* dst, size_t dstBytes, const char* src,
                              size_t sourceBytes) {
  if (!dst || dstBytes == 0) return 0;
  if (!src || sourceBytes == 0) { dst[0] = '\0'; return 0; }
  const size_t n = utf8SafePrefix(src, sourceBytes, dstBytes - 1);
  if (n) memcpy(dst, src, n);
  dst[n] = '\0';
  return n;
}
}  // namespace pocketjazz
