// SPDX-License-Identifier: MIT
// Pure, fixed-memory animation policy: never accesses Wi-Fi, SD or I2S.
#pragma once
#include <cstdint>

namespace pocketjazz {
constexpr uint32_t kMarqueeStartPauseMs = 1200;
constexpr uint32_t kMarqueeEndPauseMs = 1000;
constexpr uint32_t kMarqueeStepMs = 125; // maximum eight tiny text-line repaints / second
constexpr uint32_t kMarqueeStepPixels = 3;

inline uint16_t marqueeOffset(uint32_t elapsedMs, uint16_t textPixels,
                              uint16_t viewportPixels) {
  if (!viewportPixels || textPixels <= viewportPixels) return 0;
  const uint32_t overflow = textPixels - viewportPixels;
  const uint32_t stepCount = (overflow + kMarqueeStepPixels - 1) / kMarqueeStepPixels;
  const uint32_t motionMs = stepCount * kMarqueeStepMs;
  const uint32_t cycle = kMarqueeStartPauseMs + motionMs + kMarqueeEndPauseMs;
  const uint32_t position = elapsedMs % cycle;
  if (position <= kMarqueeStartPauseMs) return 0;
  const uint32_t inMotion = position - kMarqueeStartPauseMs;
  if (inMotion >= motionMs) return static_cast<uint16_t>(overflow);
  const uint32_t px = (inMotion / kMarqueeStepMs) * kMarqueeStepPixels;
  return static_cast<uint16_t>(px < overflow ? px : overflow);
}
} // namespace pocketjazz
