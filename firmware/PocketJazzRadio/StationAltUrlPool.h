// SPDX-License-Identifier: GPL-3.0-or-later
// Sparse, offset-indexed alternate URLs for the radio catalog.
// Allocations happen only while loading/reloading playlists in the UI task,
// not during radio playback or from the audio worker.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace pocketjazz {

class StationAltUrlPool {
 public:
  static constexpr uint16_t kNone = UINT16_MAX;
  static constexpr size_t kMaxUrlBytes = 192;
  // A maximum of 64 stations, each with at most 191 visible URL bytes.
  static constexpr size_t kMaxPoolBytes = 64 * kMaxUrlBytes;

  StationAltUrlPool() = default;
  ~StationAltUrlPool() { std::free(bytes_); }
  StationAltUrlPool(const StationAltUrlPool&) = delete;
  StationAltUrlPool& operator=(const StationAltUrlPool&) = delete;

  // Keep the already allocated block across R reloads, avoiding repeated
  // malloc/free fragmentation. Existing offsets are invalid after reset.
  void reset() { used_ = 0; }

  uint16_t add(const char* url) {
    if (!url) return kNone;
    const size_t len = std::strlen(url);
    if (!len || len >= kMaxUrlBytes || used_ + len + 1 > kMaxPoolBytes) return kNone;
    const size_t required = used_ + len + 1;
    if (required > capacity_) {
      size_t nextCapacity = capacity_ ? capacity_ : 1024;
      while (nextCapacity < required && nextCapacity < kMaxPoolBytes)
        nextCapacity *= 2;
      if (nextCapacity > kMaxPoolBytes) nextCapacity = kMaxPoolBytes;
      if (nextCapacity < required) return kNone;
      // realloc failure leaves old offsets and data valid; the station will
      // simply lack an optional fallback, never lose its primary address.
      char* extended = static_cast<char*>(std::realloc(bytes_, nextCapacity));
      if (!extended) return kNone;
      bytes_ = extended;
      capacity_ = nextCapacity;
    }
    const uint16_t offset = static_cast<uint16_t>(used_);
    std::memcpy(bytes_ + used_, url, len + 1);
    used_ += len + 1;
    return offset;
  }

  const char* get(uint16_t offset) const {
    return bytes_ && offset != kNone && offset < used_ ? bytes_ + offset : "";
  }
  size_t used() const { return used_; }
  size_t capacity() const { return capacity_; }

 private:
  char* bytes_ = nullptr;
  size_t used_ = 0;
  size_t capacity_ = 0;
};

} // namespace pocketjazz
