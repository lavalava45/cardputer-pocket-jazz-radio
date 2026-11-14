// SPDX-License-Identifier: MIT
// Bounded, allocation-free memory checkpoints for ONE audio-worker writer.
// Readback uses a tiny critical section; SD formatting is UI-only and manual.
#pragma once
#include <cstdint>
#include <cstddef>
#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
#include <mutex>
#endif

namespace pocketjazz {

enum class HeapMoment : uint8_t {
  PRE_RING,
  RING_HELD,
  PRE_OPEN,
  OPEN_RESULT,
  PREFILL_DONE,
  CODEC_RESULT,
  FIRST_PCM,
  LOW_FREE,
  LOW_BLOCK,
  FIRST_STALL,
  BEFORE_FREE,
  AFTER_FREE,
  COUNT
};

inline const char* heapMomentName(HeapMoment moment) {
  switch (moment) {
    case HeapMoment::PRE_RING: return "PRE_RING";
    case HeapMoment::RING_HELD: return "RING_HELD";
    case HeapMoment::PRE_OPEN: return "PRE_OPEN";
    case HeapMoment::OPEN_RESULT: return "OPEN_RESULT";
    case HeapMoment::PREFILL_DONE: return "PREFILL_DONE";
    case HeapMoment::CODEC_RESULT: return "CODEC_RESULT";
    case HeapMoment::FIRST_PCM: return "FIRST_PCM";
    case HeapMoment::LOW_FREE: return "LOW_FREE";
    case HeapMoment::LOW_BLOCK: return "LOW_BLOCK";
    case HeapMoment::FIRST_STALL: return "FIRST_STALL";
    case HeapMoment::BEFORE_FREE: return "BEFORE_FREE";
    case HeapMoment::AFTER_FREE: return "AFTER_FREE";
    default: return "INVALID";
  }
}

struct HeapCheckpoint {
  uint32_t atMs = 0, attempt = 0;
  uint32_t free8 = 0, largest8 = 0;
  uint32_t freeDma = 0, largestDma = 0;
  uint32_t ringBytes = 0, bufferedBytes = 0;
  // Passive input-source position, not socket bytes or Wi-Fi packet count.
  // Useful for correlating a drained MP3 ring with upstream progress.
  uint32_t sourceBytes = 0, sourceIdleMs = 0, sourceMaxGapMs = 0;
  bool valid = false;
};

class HeapLifecycleProfile {
 public:
  static constexpr size_t kCount = static_cast<size_t>(HeapMoment::COUNT);

  // Start a NEW attempt without losing the first live starvation since boot.
  void begin(uint32_t attempt) {
    Guard lock(*this);
    attempt_ = attempt;
    for (auto& slot : points_) slot = HeapCheckpoint{};
  }

  void capture(HeapMoment phase, const HeapCheckpoint& point) {
    const auto index = static_cast<size_t>(phase);
    if (index >= kCount) return;
    Guard lock(*this);
    if (!attempt_ || point.attempt != attempt_) return;
    if (phase == HeapMoment::FIRST_STALL && points_[index].valid) return;
    if (phase == HeapMoment::LOW_FREE && points_[index].valid &&
        point.free8 >= points_[index].free8) return;
    if (phase == HeapMoment::LOW_BLOCK && points_[index].valid &&
        point.largest8 >= points_[index].largest8) return;
    points_[index] = point;
    points_[index].valid = true;
    if (phase == HeapMoment::FIRST_STALL && !firstStallEver_.valid)
      firstStallEver_ = points_[index];
  }

  bool read(HeapMoment phase, HeapCheckpoint& out) const {
    const auto index = static_cast<size_t>(phase);
    if (index >= kCount) { out = HeapCheckpoint{}; return false; }
    Guard lock(*this);
    out = points_[index];
    return out.valid;
  }

  bool firstStallEver(HeapCheckpoint& out) const {
    Guard lock(*this);
    out = firstStallEver_;
    return out.valid;
  }

  uint32_t attempt() const {
    Guard lock(*this);
    return attempt_;
  }

 private:
  struct Guard {
    explicit Guard(const HeapLifecycleProfile& p) : profile_(p) { profile_.lock(); }
    ~Guard() { profile_.unlock(); }
    const HeapLifecycleProfile& profile_;
  };
  void lock() const {
#ifdef ARDUINO
    portENTER_CRITICAL(&mutex_);
#else
    mutex_.lock();
#endif
  }
  void unlock() const {
#ifdef ARDUINO
    portEXIT_CRITICAL(&mutex_);
#else
    mutex_.unlock();
#endif
  }
#ifdef ARDUINO
  mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
#else
  mutable std::mutex mutex_;
#endif
  uint32_t attempt_ = 0;
  HeapCheckpoint points_[kCount]{};
  HeapCheckpoint firstStallEver_{}; // survives automatic retries
};

static_assert(sizeof(HeapCheckpoint) <= 48, "heap checkpoint must remain compact");
static_assert(sizeof(HeapLifecycleProfile) <= 752, "no large telemetry cache in DRAM");
} // namespace pocketjazz
