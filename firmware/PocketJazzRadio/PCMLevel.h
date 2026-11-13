// Low-cost stereo source peak meter. It does not represent measured DAC output.
// Dependency-free so host tests can exercise the same routine as ESP32 firmware.
#pragma once
#include <cstddef>
#include <cstdint>

namespace pocketjazz {
inline uint8_t stereoSourcePeakPercent(const int16_t* interleaved, size_t frames) {
  if (!interleaved || frames == 0) return 0;
  int32_t peak = 0;
  for (size_t i = 0; i < frames; i += 4) {
    const int32_t left = interleaved[2 * i];
    const int32_t right = interleaved[2 * i + 1];
    const int32_t absLeft = left < 0 ? -left : left;
    const int32_t absRight = right < 0 ? -right : right;
    if (absLeft > peak) peak = absLeft;
    if (absRight > peak) peak = absRight;
  }
  const int32_t percent = peak * 100 / 32767;
  return (uint8_t)(percent > 100 ? 100 : percent);
}
} // namespace pocketjazz
