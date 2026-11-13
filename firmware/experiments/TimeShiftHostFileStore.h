// SPDX-License-Identifier: MIT
// Host-only seekable FILE* adapter for the TimeShiftRing proof of concept.
// NEVER activated by firmware; caller owns FILE*, no SD auto-recording.
#pragma once
#include "TimeShiftRing.h"
#include <cstdio>
#include <cstdint>

namespace pocketjazz {
class TimeShiftHostFileStore final : public TimeShiftStore {
 public:
  TimeShiftHostFileStore() : handle_(nullptr), bytes_(0), healthy_(false) {}

  // Accept only an explicitly opened, already-sized, seekable file; do not
  // truncate, preallocate, create directories or own the user's file handle.
  // The firmware SD adapter must provide its own independent lifecycle.
  bool attach(std::FILE* handle, uint32_t capacity) {
    detach();
    if (!handle || capacity < 4096 || capacity > 32u * 1024u * 1024u)
      return false;
    if (std::fseek(handle, 0, SEEK_END) != 0) return false;
    const long size = std::ftell(handle);
    if (size < 0 || static_cast<uint64_t>(size) != capacity) return false;
    if (std::fseek(handle, 0, SEEK_SET) != 0) return false;
    handle_ = handle;
    bytes_ = capacity;
    healthy_ = true;
    return true;
  }

  bool writeAt(uint32_t offset, const uint8_t* input, size_t count) override {
    if (!rangeOk(offset, count) || (!input && count)) return false;
    if (!seek(offset) ||
        std::fwrite(input, 1, count, handle_) != count ||
        std::fflush(handle_) != 0) {
      healthy_ = false;
      return false;
    }
    return true;
  }

  bool readAt(uint32_t offset, uint8_t* output, size_t count) override {
    if (!rangeOk(offset, count) || (!output && count)) return false;
    if (!seek(offset) || std::fread(output, 1, count, handle_) != count) {
      healthy_ = false;
      return false;
    }
    return true;
  }

  void detach() { handle_ = nullptr; bytes_ = 0; healthy_ = false; }
  bool healthy() const { return healthy_; }

 private:
  bool rangeOk(uint32_t offset, size_t count) const {
    return healthy_ && handle_ && count > 0 &&
        offset < bytes_ && count <= static_cast<size_t>(bytes_ - offset);
  }
  bool seek(uint32_t offset) {
    return std::fseek(handle_, static_cast<long>(offset), SEEK_SET) == 0;
  }
  std::FILE* handle_;
  uint32_t bytes_;
  bool healthy_;
};
} // namespace pocketjazz
