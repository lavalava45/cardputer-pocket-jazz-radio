// Source-independent MP3 prefill deadline.
// The caller starts this clock after HTTP(S) open and initial buffer read.
#pragma once
#include <cstdint>

namespace pocketjazz {
inline bool shouldKeepPrefilling(uint32_t now, uint32_t prefillStarted,
                                 uint32_t availableBytes,
                                 uint32_t targetBytes, uint32_t timeoutMs) {
  return availableBytes < targetBytes && (uint32_t)(now - prefillStarted) < timeoutMs;
}
inline bool hasPlayablePrefill(uint32_t availableBytes, uint32_t minBytes) {
  return availableBytes >= minBytes;
}
} // namespace pocketjazz
