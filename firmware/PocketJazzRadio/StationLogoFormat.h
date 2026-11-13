// SPDX-License-Identifier: MIT
// Pocket Jazz logo v1. Header-only, Arduino-independent, fixed memory/I/O.
#pragma once
#include <cstddef>
#include <cstdint>

namespace pocketjazz {
namespace logo {
constexpr size_t kWidth = 64;
constexpr size_t kHeight = 64;
constexpr size_t kPixelsBytes = kWidth * kHeight * 2;
constexpr size_t kHeaderBytes = 16;
constexpr size_t kFileBytes = kHeaderBytes + kPixelsBytes;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kRgb565Le = 1;

// Stable station identity derived from complete URL bytes, not M3U position.
inline uint64_t stationUrlHash(const char* url) {
  uint64_t hash = UINT64_C(14695981039346656037);
  if (!url) return hash;
  for (const unsigned char* p =
           reinterpret_cast<const unsigned char*>(url); *p; ++p) {
    hash ^= *p;
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

inline uint32_t get32le(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline uint32_t crc32Update(uint32_t crc, const uint8_t* bytes, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
  }
  return crc;
}
inline uint32_t crc32(const uint8_t* bytes, size_t len) {
  return bytes || len == 0 ? ~crc32Update(0xffffffffu, bytes, len) : 0;
}
inline bool validHeader(const uint8_t* header, size_t headerSize) {
  return header && headerSize == kHeaderBytes &&
         header[0] == 'P' && header[1] == 'J' &&
         header[2] == 'L' && header[3] == 'G' &&
         header[4] == kVersion && header[5] == kWidth &&
         header[6] == kHeight && header[7] == kRgb565Le &&
         get32le(header + 8) == kPixelsBytes;
}

// SD-friendly streaming API: reader.size() and reader.read(ptr, count).
// Must call from the UI task only; never use on decoder/audio task.
// out is caller-owned 8192-byte cache, never allocated internally.
// Return false => discard out and draw the original radio icon.
// Reading the fixed-size file is bounded to <= 8208 bytes and 33 reads.
template <typename Reader>
bool load(Reader& reader, uint8_t* out, size_t outBytes) {
  if (!out || outBytes < kPixelsBytes || reader.size() != kFileBytes) return false;
  uint8_t header[kHeaderBytes];
  if (reader.read(header, sizeof(header)) != sizeof(header) ||
      !validHeader(header, sizeof(header))) return false;
  uint32_t crc = 0xffffffffu;
  for (size_t offset = 0; offset < kPixelsBytes; offset += 256) {
    if (reader.read(out + offset, 256) != 256) return false;
    crc = crc32Update(crc, out + offset, 256);
  }
  return ~crc == get32le(header + 12);
}

// For callers with a complete buffer; no allocation and no I/O.
inline bool validate(const uint8_t* file, size_t fileBytes) {
  return file && fileBytes == kFileBytes &&
         validHeader(file, kHeaderBytes) &&
         crc32(file + kHeaderBytes, kPixelsBytes) == get32le(file + 12);
}

// RGB565 LE pixel i is encoded as 2 bytes, independent of host endianness.
inline uint16_t pixel(const uint8_t* rgb565le, size_t i) {
  return uint16_t(rgb565le[i * 2]) | (uint16_t(rgb565le[i * 2 + 1]) << 8);
}
}  // namespace logo
}  // namespace pocketjazz
