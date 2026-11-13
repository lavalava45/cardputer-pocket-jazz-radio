// SPDX-License-Identifier: MIT
// Offline, host-testable SD ring timeline proof of concept.
// NOT CONNECTED TO RADIO AUDIO. No automatic recording or file creation.
#pragma once
#include <cstddef>
#include <cstdint>

namespace pocketjazz {
class TimeShiftStore {
 public:
  virtual ~TimeShiftStore() {}
  // The caller must preallocate exactly capacity bytes on a suitable SD file.
  // readAt/writeAt must return false on any short/failed I/O.
  virtual bool writeAt(uint32_t offset, const uint8_t* bytes, size_t count) = 0;
  virtual bool readAt(uint32_t offset, uint8_t* bytes, size_t count) = 0;
};

class TimeShiftRing {
 public:
  TimeShiftRing() : storage_(nullptr), capacity_(0), head_(0), reader_(0), lost_(0), healthy_(false) {}
  bool begin(TimeShiftStore& store, uint32_t capacity) {
    if (capacity < 4096 || capacity > 32u * 1024u * 1024u) return false;
    storage_ = &store;
    capacity_ = capacity;
    head_ = reader_ = lost_ = 0;
    healthy_ = true;
    return true;
  }
  bool append(const uint8_t* bytes, size_t length) {
    if (!storage_ || !healthy_ || (!bytes && length) || length > capacity_) return false;
    if (!length) return true;
    const uint32_t at = (uint32_t)(head_ % capacity_);
    const size_t first = length < (capacity_ - at) ? length : capacity_ - at;
    // Logical cursor only advances after both writes have succeeded. If
    // power fails mid-write, this is NOT a durable transactional archive.
    if (!storage_->writeAt(at, bytes, first)) {
      healthy_ = false;
      return false;
    }
    if (first < length && !storage_->writeAt(0, bytes + first, length - first)) {
      healthy_ = false; // may have overwritten unread history: never replay it
      return false;
    }
    head_ += length;
    return true;
  }
  size_t read(uint8_t* destination, size_t maxBytes) {
    if (!storage_ || !healthy_ || (!destination && maxBytes)) return 0;
    const uint64_t oldest = floor();
    if (reader_ < oldest) {
      lost_ += oldest - reader_; // replay overrun, not lost live-network data
      reader_ = oldest;
    }
    const uint64_t readable = head_ - reader_;
    if (maxBytes > readable) maxBytes = (size_t)readable;
    if (!maxBytes) return 0;
    const uint32_t at = (uint32_t)(reader_ % capacity_);
    const size_t first = maxBytes < (capacity_ - at) ? maxBytes : capacity_ - at;
    if (!storage_->readAt(at, destination, first)) {
      healthy_ = false;
      return 0;
    }
    if (first < maxBytes && !storage_->readAt(0, destination + first, maxBytes - first)) {
      healthy_ = false;
      return 0;
    }
    reader_ += maxBytes;
    return maxBytes;
  }
  // Seek by *bytes*, not by seconds: MP3 streams can be variable-bitrate.
  bool seek(uint64_t absoluteByte) {
    if (!storage_ || !healthy_ || absoluteByte < floor() || absoluteByte > head_) return false;
    reader_ = absoluteByte;
    return true;
  }
  void live() { reader_ = head_; }
  uint64_t floor() const { return head_ > capacity_ ? head_ - capacity_ : 0; }
  uint64_t head() const { return head_; }
  uint64_t cursor() const { return reader_; }
  uint64_t replayOverrunBytes() const { return lost_; }
  uint32_t capacity() const { return capacity_; }
  bool healthy() const { return healthy_; }

 private:
  TimeShiftStore* storage_; // must outlive this instance
  uint32_t capacity_;
  uint64_t head_;
  uint64_t reader_;
  uint64_t lost_;
  bool healthy_;
};
} // namespace pocketjazz
