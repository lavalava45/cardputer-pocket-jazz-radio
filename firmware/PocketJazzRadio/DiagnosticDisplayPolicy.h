// SPDX-License-Identifier: MIT
// Host-testable, fixed-seconds UI countdown for future retry deadlines.
#pragma once
#include <cstdint>

namespace pocketjazz {
inline uint32_t retrySecondsRemaining(uint32_t now, uint32_t deadline) {
  // ESP32 millis() wraps every ~49.7 days; the schedule always lies <8s
  // ahead, so interpreting the modular difference as signed is safe.
  const int32_t left = static_cast<int32_t>(deadline - now);
  return left > 0 ? (static_cast<uint32_t>(left) + 999u) / 1000u : 0u;
}
} // namespace pocketjazz
