// Bounded, deterministic, non-secret Pocket Jazz settings backup format.
// SPDX-License-Identifier: GPL-3.0-or-later
// CRC32 detects accidental corruption, NOT malicious tampering/authentication.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace pocketjazz {

struct SettingsBackupData {
  uint8_t volume = 57;
  uint8_t list = 0;
  uint8_t station = 0;
  bool autoplay = true;
  bool tzOverride = false;
  int32_t tzMinutes = 0;
  bool sparkAuto = true;
  char lastUrl[192]{};
  char presetUrl[9][192]{};
  char presetName[9][56]{};
};

class SettingsBackupPolicy {
 public:
  static constexpr size_t kMaxFileBytes = 8192;
  static bool valid(const SettingsBackupData& d) {
    if (d.volume > 185 || d.list > 11 || d.station > 63 ||
        d.tzMinutes < -720 || d.tzMinutes > 840 || d.tzMinutes % 30) return false;
    if (!safeUrl(d.lastUrl, sizeof(d.lastUrl))) return false;
    for (int i = 0; i < 9; ++i)
      if (!safeUrl(d.presetUrl[i], sizeof(d.presetUrl[i])) ||
          !safeText(d.presetName[i], sizeof(d.presetName[i]))) return false;
    return true;
  }

  static bool encode(const SettingsBackupData& d, char* out, size_t cap,
                     size_t& length) {
    length = 0;
    if (!out || cap < 128 || cap > kMaxFileBytes || !valid(d)) return false;
    Writer w{out, cap, 0};
    if (!w.raw("PJSET1\n") ||
        !w.number("vol", d.volume) || !w.number("list", d.list) ||
        !w.number("station", d.station) || !w.number("autoplay", d.autoplay) ||
        !w.number("tzSet", d.tzOverride) || !w.number("tzMin", d.tzMinutes) ||
        !w.number("sparkAuto", d.sparkAuto) || !w.hex("lastUrl", d.lastUrl, sizeof(d.lastUrl)))
      return false;
    for (int i = 0; i < 9; ++i) {
      char key[9];
      std::snprintf(key, sizeof(key), "pr%du", i);
      if (!w.hex(key, d.presetUrl[i], sizeof(d.presetUrl[i]))) return false;
      std::snprintf(key, sizeof(key), "pr%dn", i);
      if (!w.hex(key, d.presetName[i], sizeof(d.presetName[i]))) return false;
    }
    const uint32_t checksum = crc32(out, w.n);
    char footer[24];
    std::snprintf(footer, sizeof(footer), "crc32=%08lX\n", (unsigned long)checksum);
    if (!w.raw(footer)) return false;
    length = w.n;
    return true;
  }

  static bool decode(const char* input, size_t length, SettingsBackupData& destination) {
    if (!input || length < 32 || length > kMaxFileBytes) return false;
    Reader r{input, length, 0};
    SettingsBackupData temp{};
    if (!r.literal("PJSET1") || !r.number("vol", temp.volume) ||
        !r.number("list", temp.list) || !r.number("station", temp.station) ||
        !r.number("autoplay", temp.autoplay) || !r.number("tzSet", temp.tzOverride) ||
        !r.number("tzMin", temp.tzMinutes) || !r.number("sparkAuto", temp.sparkAuto) ||
        !r.hex("lastUrl", temp.lastUrl, sizeof(temp.lastUrl))) return false;
    for (int i = 0; i < 9; ++i) {
      char key[9];
      std::snprintf(key, sizeof(key), "pr%du", i);
      if (!r.hex(key, temp.presetUrl[i], sizeof(temp.presetUrl[i]))) return false;
      std::snprintf(key, sizeof(key), "pr%dn", i);
      if (!r.hex(key, temp.presetName[i], sizeof(temp.presetName[i]))) return false;
    }
    const size_t checksumOffset = r.pos;
    const char* checksumText;
    size_t checksumLength;
    if (!r.line(checksumText, checksumLength) || r.pos != length ||
        checksumLength != 14 || std::memcmp(checksumText, "crc32=", 6)) return false;
    uint32_t recorded = 0;
    for (size_t i = 6; i < 14; ++i) {
      const int h = hexDigit(checksumText[i]);
      if (h < 0) return false;
      recorded = (recorded << 4) | static_cast<uint32_t>(h);
    }
    if (recorded != crc32(input, checksumOffset) || !valid(temp)) return false;
    destination = temp; // no partial mutation on any parsing failure
    return true;
  }

 private:
  static bool safeText(const char* s, size_t cap) {
    size_t i = 0;
    for (; i < cap && s[i]; ++i) if ((unsigned char)s[i] < 0x20) return false;
    return i < cap;
  }
  static bool safeUrl(const char* s, size_t cap) {
    if (!safeText(s, cap)) return false;
    if (!s[0]) return true;
    return std::strncmp(s, "https://", 8) == 0 ||
           std::strncmp(s, "http://", 7) == 0;
  }
  static uint32_t crc32(const char* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
      crc ^= static_cast<uint8_t>(data[i]);
      for (unsigned b = 0; b < 8; ++b)
        crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return ~crc;
  }
  static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1; // exact canonical uppercase hex only
  }
  struct Writer {
    char* buffer;
    size_t cap, n;
    bool raw(const char* s) {
      const size_t len = std::strlen(s);
      if (len > cap - n) return false;
      std::memcpy(buffer + n, s, len); n += len;
      return true;
    }
    bool number(const char* key, int32_t value) {
      char line[48];
      const int len = std::snprintf(line, sizeof(line), "%s=%ld\n", key, (long)value);
      return len > 0 && len < (int)sizeof(line) && raw(line);
    }
    bool hex(const char* key, const char* value, size_t capacity) {
      static const char* digits = "0123456789ABCDEF";
      if (!raw(key) || !raw("=")) return false;
      size_t len = 0;
      while (len < capacity && value[len]) ++len;
      if (len >= capacity || len > (cap - n) / 2) return false;
      for (size_t i = 0; i < len; ++i) {
        const uint8_t c = static_cast<uint8_t>(value[i]);
        buffer[n++] = digits[c >> 4]; buffer[n++] = digits[c & 15];
      }
      return raw("\n");
    }
  };
  struct Reader {
    const char* data;
    size_t length, pos;
    bool line(const char*& begin, size_t& size) {
      if (pos >= length) return false;
      const size_t start = pos;
      while (pos < length && data[pos] != '\n' && pos - start < 512) ++pos;
      if (pos >= length || data[pos] != '\n') return false;
      begin = data + start; size = pos++ - start;
      return true;
    }
    bool literal(const char* text) {
      const char* lineText; size_t count;
      return line(lineText, count) && count == std::strlen(text) &&
             !std::memcmp(lineText, text, count);
    }
    template <typename T> bool number(const char* key, T& out) {
      const char* str; size_t size;
      if (!line(str, size)) return false;
      const size_t keyLength = std::strlen(key);
      if (size < keyLength + 2 || std::memcmp(str, key, keyLength) ||
          str[keyLength] != '=') return false;
      size_t idx = keyLength + 1;
      bool minus = idx < size && str[idx] == '-';
      if (minus) ++idx;
      if (idx == size || size - idx > 7) return false;
      int64_t v = 0;
      for (; idx < size; ++idx) {
        if (str[idx] < '0' || str[idx] > '9') return false;
        v = v * 10 + str[idx] - '0';
      }
      if (minus) v = -v;
      if (v < -720 || v > 840) {
        // Narrow numeric fields also undergo their per-field validation below.
        // Do not reject larger *positive* enum values here before validation.
        if (v < -1000000 || v > 1000000) return false;
      }
      out = static_cast<T>(v);
      if (static_cast<int64_t>(out) != v) return false;
      if ((std::strcmp(key, "autoplay") == 0 || std::strcmp(key, "tzSet") == 0 ||
           std::strcmp(key, "sparkAuto") == 0) && v != 0 && v != 1) return false;
      return true;
    }
    bool hex(const char* key, char* destination, size_t capacity) {
      const char* str; size_t size;
      if (!line(str, size)) return false;
      const size_t prefix = std::strlen(key);
      if (size < prefix + 1 || std::memcmp(str, key, prefix) ||
          str[prefix] != '=' || ((size - prefix - 1) & 1)) return false;
      const size_t bytes = (size - prefix - 1) / 2;
      if (bytes >= capacity) return false;
      for (size_t i = 0; i < bytes; ++i) {
        const int high = hexDigit(str[prefix + 1 + i * 2]);
        const int low = hexDigit(str[prefix + 2 + i * 2]);
        if (high < 0 || low < 0 || !(high || low)) return false;
        destination[i] = static_cast<char>((high << 4) | low);
      }
      destination[bytes] = 0;
      return true;
    }
  };
};

} // namespace pocketjazz
