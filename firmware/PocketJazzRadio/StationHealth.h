// Consecutive complete-stream-failure health for a bounded station catalog.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace pocketjazz {

template <size_t Capacity>
class StationHealth {
 public:
  static constexpr uint8_t kWarningStreak = 3;

  // Audio worker: count only a completed station attempt (including any
  // configured #PJ-ALT fallback). Don't count a manual STOP, Wi-Fi outage or
  // an intentionally cancelled connection as a station-specific failure.
  void failed(size_t index) {
    if (index >= Capacity) return;
    uint8_t old = consecutive_[index].load(std::memory_order_relaxed);
    while (old < kWarningStreak && !consecutive_[index].compare_exchange_weak(
        old, static_cast<uint8_t>(old + 1), std::memory_order_relaxed)) {}
  }

  // Only after sustained PLAYING and accepted PCM frames.
  void recovered(size_t index) {
    if (index < Capacity) consecutive_[index].store(0, std::memory_order_relaxed);
  }

  uint8_t streak(size_t index) const {
    return index < Capacity ? consecutive_[index].load(std::memory_order_relaxed) : 0;
  }

  bool warning(size_t index) const { return streak(index) >= kWarningStreak; }

  void reset() {
    for (auto& count : consecutive_) count.store(0, std::memory_order_relaxed);
  }

 private:
  std::atomic<uint8_t> consecutive_[Capacity]{};
};

} // namespace pocketjazz
