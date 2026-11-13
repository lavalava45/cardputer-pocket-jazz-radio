// Source-independent MP3 prefill deadline.
// The caller starts this clock after HTTP(S) open and initial buffer read.
#pragma once
#include <cstdint>

namespace pocketjazz {
// Startup buffering, independent from the ring's memory allocation policy:
// preserve at most 8 KiB of compressed MP3 before decoding, including on
// 32-KiB rings. The old 32-KiB mode started with only 2 KiB (~125 ms at
// 128 kbit/s), creating a predictable initial jitter vulnerability.
// Larger prefills increase time-to-first-sound if a server is slow; the
// existing bounded timeout below still prevents indefinite waiting.
inline uint32_t initialMp3PrefillGoal(uint32_t ringCapacity) {
  const uint32_t halfRing = ringCapacity / 2;
  return halfRing < 8192u ? halfRing : 8192u;
}
inline bool shouldKeepPrefilling(uint32_t now, uint32_t prefillStarted,
                                 uint32_t availableBytes,
                                 uint32_t targetBytes, uint32_t timeoutMs) {
  return availableBytes < targetBytes && (uint32_t)(now - prefillStarted) < timeoutMs;
}
inline bool hasPlayablePrefill(uint32_t availableBytes, uint32_t minBytes) {
  return availableBytes >= minBytes;
}
} // namespace pocketjazz
