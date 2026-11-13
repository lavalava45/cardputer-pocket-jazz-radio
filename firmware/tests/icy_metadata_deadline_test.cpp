#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioICYDeadline.h"
#include <cassert>
#include <cstdint>

int main() {
  assert(!audio_icy::metadataTimedOut(1000, 1000));
  assert(!audio_icy::metadataTimedOut(1000, 3499));
  assert(audio_icy::metadataTimedOut(1000, 3500));
  assert(audio_icy::metadataTimedOut(1000, 10000));
  // Rollover-safe 32-bit millis().
  assert(!audio_icy::metadataTimedOut(UINT32_MAX - 1500, 998));
  assert(audio_icy::metadataTimedOut(UINT32_MAX - 1500, 1000));
  // Edge and explicit caller limit.
  assert(!audio_icy::metadataTimedOut(200, 499, 300));
  assert(audio_icy::metadataTimedOut(200, 500, 300));
  return 0;
}
