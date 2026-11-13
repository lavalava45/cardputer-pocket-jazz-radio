// Bounded SD Library discovery and safe ID3v2 seek planning.
// SPDX-License-Identifier: GPL-3.0-or-later
// Intentionally portable, no SD calls or dynamic memory: production integration
// requires real FAT/SD read latency and audio-worker ownership tests.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketjazz {

struct OfflineTrack {
  char filename[80]{};
  uint32_t sizeBytes = 0;
};

template <size_t Capacity>
class OfflineLibraryPolicy {
 public:
  static constexpr size_t kMaxEntries = Capacity;
  static constexpr uint32_t kMinMp3Bytes = 1024;
  static constexpr uint32_t kMaxMp3Bytes = 512u * 1024u * 1024u;
  static constexpr uint32_t kMaxId3Bytes = 2u * 1024u * 1024u;

  size_t count() const { return count_; }
  const OfflineTrack* at(size_t index) const {
    return index < count_ ? &entries_[index] : nullptr;
  }
  void clear() {
    count_ = 0;
    for (auto& e : entries_) e = OfflineTrack{};
  }

  // Each leaf-name comes from SD.openNextFile(), not untrusted URLs/M3U paths.
  // Reject traversal, separators and invalid UTF-8; support non-Latin names.
  static bool validLeafName(const char* name) {
    if (!name || !name[0]) return false;
    size_t n = 0;
    while (n < sizeof(OfflineTrack::filename) && name[n]) ++n;
    if (n < 5 || n >= sizeof(OfflineTrack::filename) || name[0] == '.') return false;
    if (lower(name[n - 4]) != '.' || lower(name[n - 3]) != 'm' ||
        lower(name[n - 2]) != 'p' || lower(name[n - 1]) != '3') return false;
    for (size_t i = 0; i < n; ++i) {
      const uint8_t c = static_cast<uint8_t>(name[i]);
      if (c < 0x20 || c == 0x7f || c == '/' || c == '\\' || c == ':' ||
          (c == '.' && i && name[i - 1] == '.')) return false;
    }
    return validUtf8(reinterpret_cast<const uint8_t*>(name), n);
  }

  bool add(const char* leafName, uint64_t bytes, bool directory) {
    if (directory || count_ >= Capacity || bytes < kMinMp3Bytes ||
        bytes > kMaxMp3Bytes || !validLeafName(leafName)) return false;
    for (size_t i = 0; i < count_; ++i)
      if (sameAsciiCaseInsensitive(entries_[i].filename, leafName)) return false;
    OfflineTrack& e = entries_[count_++];
    std::memcpy(e.filename, leafName, std::strlen(leafName) + 1);
    e.sizeBytes = static_cast<uint32_t>(bytes);
    return true;
  }

  static bool makePath(const char* leafName, char* out, size_t capacity) {
    static const char prefix[] = "/PocketJazz/offline/";
    if (!out || !validLeafName(leafName)) return false;
    const size_t nameBytes = std::strlen(leafName);
    if (sizeof(prefix) + nameBytes > capacity) return false;
    std::memcpy(out, prefix, sizeof(prefix) - 1);
    std::memcpy(out + sizeof(prefix) - 1, leafName, nameBytes + 1);
    return true;
  }

  // Read exactly the first 10 bytes of a local file before opening libmad.
  // If no ID3v2 signature, play from offset 0. If ID3 is malformed, reject
  // rather than let an upstream unbounded tag parser spin in the audio task.
  // We skip metadata, NOT attempt to decode/display unsanitized ID3 strings.
  static bool id3Start(const uint8_t* header, size_t read, uint32_t fileBytes,
                       uint32_t& contentOffset) {
    contentOffset = 0;
    if (!header || read != 10 || fileBytes < kMinMp3Bytes) return false;
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') return true;
    if (header[3] < 2 || header[3] > 4 || header[4] != 0) return false;
    const uint8_t flags = header[5];
    const uint8_t allowedFlags = header[3] == 4 ? 0xF0 :
                                 header[3] == 3 ? 0xE0 : 0xC0;
    if (flags & ~allowedFlags) return false;
    uint32_t size = 0;
    for (int i = 6; i < 10; ++i) {
      if (header[i] & 0x80) return false; // syncsafe size only
      size = (size << 7) | header[i];
    }
    const uint32_t footer = header[3] == 4 && (flags & 0x10) ? 10 : 0;
    if (size > kMaxId3Bytes || size + 10u + footer >= fileBytes) return false;
    contentOffset = size + 10u + footer;
    return true;
  }

 private:
  OfflineTrack entries_[Capacity]{};
  size_t count_ = 0;
  static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
  }
  static bool sameAsciiCaseInsensitive(const char* a, const char* b) {
    while (*a && *b) if (lower(*a++) != lower(*b++)) return false;
    return *a == 0 && *b == 0;
  }
  static bool validUtf8(const uint8_t* text, size_t n) {
    for (size_t i = 0; i < n;) {
      const uint32_t c = text[i];
      if (c < 0x80) { ++i; continue; }
      size_t needed = 0;
      uint32_t cp = 0, minValue = 0;
      if (c >= 0xC2 && c <= 0xDF) { needed = 1; cp = c & 0x1f; minValue = 0x80; }
      else if (c >= 0xE0 && c <= 0xEF) { needed = 2; cp = c & 0x0f; minValue = 0x800; }
      else if (c >= 0xF0 && c <= 0xF4) { needed = 3; cp = c & 0x07; minValue = 0x10000; }
      else return false;
      if (i + needed >= n) return false;
      for (size_t k = 1; k <= needed; ++k) {
        if ((text[i + k] & 0xC0) != 0x80) return false;
        cp = (cp << 6) | (text[i + k] & 0x3f);
      }
      if (cp < minValue || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
      i += needed + 1;
    }
    return true;
  }
};

} // namespace pocketjazz
