// SPDX-License-Identifier: MIT
// Offline helper for EXPLICIT, app-owned SD log writes only. No SD or Arduino I/O.
#pragma once

#include <cstddef>
#include <cstdint>

namespace pocketjazz {
namespace log {

constexpr size_t kDefaultLogLimit = 16384;
// Fixed input scan budget for untrusted ICY titles/station names.
constexpr size_t kMaxFieldInputBytes = 512;

// Overflow-safe even for SIZE_MAX. Incoming is the *entire* encoded line,
// including TAB separators and LF. Exact cap is permitted; no rotation here.
inline bool canAppend(size_t existingBytes, size_t incomingBytes,
                      size_t capBytes = kDefaultLogLimit) {
  return existingBytes <= capBytes && incomingBytes <= capBytes - existingBytes;
}

inline bool forbiddenCodepoint(uint32_t cp) {
  // No C0/C1 controls, ASCII DEL, Unicode line/paragraph separators, bidi
  // formatting, zero-width formatting or BOM. Preserve printable non-ASCII.
  return cp < 0x20u || (cp >= 0x7fu && cp <= 0x9fu) ||
         cp == 0x2028u || cp == 0x2029u || cp == 0xfeffu ||
         (cp >= 0x200bu && cp <= 0x200fu) ||
         (cp >= 0x202au && cp <= 0x202eu) ||
         (cp >= 0x2060u && cp <= 0x206fu);
}

// Returns the number of bytes written (excluding terminal NUL). Always NUL
// terminates when dest != nullptr && destBytes > 0, even on invalid input.
// The caller must provide a valid NUL-terminated source; we read at most 512
// bytes. No heap allocation, malformed UTF-8 is replaced with ASCII space,
// and a valid multibyte sequence is never partially copied to the buffer.
// Source and destination must be distinct (no in-place sanitization).
inline size_t cleanTsvField(const char* src, char* dest, size_t destBytes) {
  if (!dest || destBytes == 0) return 0;
  dest[0] = '\0';
  if (!src) return 0;
  size_t out = 0;
  size_t pos = 0;
  while (pos < kMaxFieldInputBytes && src[pos] != '\0') {
    const uint8_t first = static_cast<uint8_t>(src[pos]);
    uint32_t cp = 0;
    size_t span = 1;
    bool valid = true;
    if (first < 0x80u) {
      cp = first;
    } else {
      if (first >= 0xc2u && first <= 0xdfu) { span = 2; cp = first & 0x1fu; }
      else if (first >= 0xe0u && first <= 0xefu) { span = 3; cp = first & 0x0fu; }
      else if (first >= 0xf0u && first <= 0xf4u) { span = 4; cp = first & 0x07u; }
      else valid = false;
      if (valid) {
        if (span > kMaxFieldInputBytes - pos) valid = false;
        for (size_t i = 1; valid && i < span; ++i) {
          const uint8_t byte = static_cast<uint8_t>(src[pos + i]);
          if (byte == 0 || (byte & 0xc0u) != 0x80u) valid = false;
          else cp = (cp << 6) | (byte & 0x3fu);
        }
        if (valid && ((span == 2 && cp < 0x80u) ||
                      (span == 3 && cp < 0x800u) ||
                      (span == 4 && cp < 0x10000u) ||
                      (cp >= 0xd800u && cp <= 0xdfffu) ||
                      cp > 0x10ffffu)) valid = false;
      }
    }
    const bool replace = !valid || forbiddenCodepoint(cp);
    const size_t size = replace ? 1 : span;
    if (size > (destBytes - 1) - out) break;
    if (replace) dest[out++] = ' ';
    else {
      for (size_t i = 0; i < span; ++i) dest[out++] = src[pos + i];
    }
    pos += valid ? span : 1;
  }
  dest[out] = '\0';
  return out;
}

} // namespace log
} // namespace pocketjazz
