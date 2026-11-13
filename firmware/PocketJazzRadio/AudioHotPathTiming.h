// Lightweight, allocation-free hot-path duration maxima for hardware diagnosis.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <stdint.h>

namespace pocketjazz {
inline uint32_t elapsedMicros(uint32_t started, uint32_t finished) {
  return finished - started; // unsigned rollover; each operation is short
}
inline void observeMaxMicros(std::atomic<uint32_t>& maximum, uint32_t elapsed) {
  uint32_t previous = maximum.load(std::memory_order_relaxed);
  while (elapsed > previous &&
         !maximum.compare_exchange_weak(previous, elapsed,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {}
}
} // namespace pocketjazz
