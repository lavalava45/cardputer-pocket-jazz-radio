// SPDX-License-Identifier: MIT
// Passive source byte-position progress monitor for the audio task only.
// No clocks, locks, network reads, SD writes, allocations, or recovery policy.
#pragma once
#include <stdint.h>

namespace pocketjazz {
class AudioSourceProgress {
 public:
  void reset(uint32_t now, uint32_t sourcePosition = 0) {
    lastPosition_ = sourcePosition;
    lastAt_ = now;
    maximumGapMs_ = 0;
  }
  void observe(uint32_t now, uint32_t sourcePosition) {
    if (sourcePosition != lastPosition_) {
      const uint32_t gap = now - lastAt_;
      if (gap > maximumGapMs_) maximumGapMs_ = gap;
      lastPosition_ = sourcePosition;
      lastAt_ = now;
    }
  }
  uint32_t bytes() const { return lastPosition_; }
  uint32_t idleMs(uint32_t now) const { return now - lastAt_; }
  uint32_t maximumGapMs() const { return maximumGapMs_; }

 private:
  uint32_t lastPosition_ = 0;
  uint32_t lastAt_ = 0;
  uint32_t maximumGapMs_ = 0;
};
static_assert(sizeof(AudioSourceProgress) == 12,
              "Source progress monitor must be three 32-bit counters");
} // namespace pocketjazz
