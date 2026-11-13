// SPDX-License-Identifier: GPL-3.0-or-later
// Pure, host-tested guards for reading legacy Pocket Jazz NVS values.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace pocketjazz {
constexpr uint8_t kCurrentPreferenceSchema = 2;

inline bool canWritePreferenceSchema(uint8_t stored) {
  // Version 0 is v0.1 legacy with no explicit schema. Preserve forward data:
  // never downgrade NVS created by a newer unknown firmware.
  return stored <= kCurrentPreferenceSchema;
}

inline bool validSavedStreamUrl(const char* url, size_t maxBytes = 192) {
  if (!url || maxBytes <= 10) return false;
  size_t length = 0;
  for (; length < maxBytes && url[length]; ++length) {
    const unsigned char c = static_cast<unsigned char>(url[length]);
    if (c <= 32 || c >= 127 || c == '\\') return false;
  }
  if (length >= maxBytes || length <= 10) return false;
  const bool secure = std::strncmp(url, "https://", 8) == 0;
  const bool plain = std::strncmp(url, "http://", 7) == 0;
  if (!secure && !plain) return false;
  const size_t start = secure ? 8 : 7;
  size_t authorityEnd = start;
  for (; authorityEnd < length && url[authorityEnd] != '/' &&
         url[authorityEnd] != '?'; ++authorityEnd) {
    if (url[authorityEnd] == '@' || url[authorityEnd] == '#') return false;
  }
  if (authorityEnd == start) return false;
  for (size_t i = authorityEnd; i < length; ++i) {
    if (url[i] == '#') return false;
  }
  return true; // Stored URL may include a broadcaster query string.
}

// Preferences::getString returns the complete value. Validate *that value*
// before copying to a smaller static array. Never accept a valid-looking
// URL that resulted from silent strlcpy truncation of corrupt NVS contents.
template <size_t N>
inline bool copyValidatedSavedStreamUrl(char (&destination)[N],
                                        const char* original,
                                        size_t originalBytes) {
  destination[0] = '\0';
  if (!original || originalBytes >= N || originalBytes <= 10 ||
      std::strlen(original) != originalBytes ||
      !validSavedStreamUrl(original, N)) return false;
  std::memcpy(destination, original, originalBytes + 1);
  return true;
}
} // namespace pocketjazz
