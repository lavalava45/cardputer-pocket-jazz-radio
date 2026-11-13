// SPDX-License-Identifier: MIT
// Manual tap tempo, with bounded history, debounce and no heap allocation.
#pragma once
#include <cstddef>
#include <cstdint>

namespace pocketjazz {
class TapTempo {
 public:
  void reset() {
    count_ = 0;
    lastTap_ = 0;
    seenTap_ = false;
  }
  bool tap(uint32_t ms) {
    if (!seenTap_) {
      lastTap_ = ms;
      seenTap_ = true;
      return true;
    }
    const uint32_t gap = (uint32_t)(ms - lastTap_);
    if (gap < 180) return false;            // bounce / inadvertent double tap
    lastTap_ = ms;
    if (gap > 2000) { count_ = 0; return true; } // stop / new tempo
    if (count_ == kHistory) {
      for (size_t i = 1; i < kHistory; ++i) gaps_[i - 1] = gaps_[i];
      --count_;
    }
    gaps_[count_++] = (uint16_t)gap;
    return true;
  }
  int bpm(uint32_t now) const {
    if (!seenTap_ || count_ < 3 || (uint32_t)(now - lastTap_) > 5000)
      return 0;
    uint16_t copy[kHistory] = {};
    for (size_t i = 0; i < count_; ++i) copy[i] = gaps_[i];
    // Insertion sort five elements; constant bounded CPU.
    for (size_t i = 1; i < count_; ++i) {
      const uint16_t v = copy[i];
      size_t j = i;
      while (j > 0 && copy[j - 1] > v) { copy[j] = copy[j - 1]; --j; }
      copy[j] = v;
    }
    const uint32_t median = count_ % 2
        ? copy[count_ / 2]
        : (uint32_t(copy[count_ / 2 - 1]) + copy[count_ / 2]) / 2;
    if (!median) return 0;
    const int result = (int)((60000u + median / 2) / median);
    return result >= 30 && result <= 300 ? result : 0;
  }
  size_t intervals() const { return count_; }
  uint32_t lastTap() const { return lastTap_; }

 private:
  static constexpr size_t kHistory = 5;
  uint16_t gaps_[kHistory] = {};
  size_t count_ = 0;
  uint32_t lastTap_ = 0;
  bool seenTap_ = false;
};
} // namespace pocketjazz
