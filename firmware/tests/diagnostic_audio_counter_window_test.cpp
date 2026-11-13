#ifdef NDEBUG
#undef NDEBUG // Release CTest must execute assertions.
#endif
#include "../PocketJazzRadio/DiagnosticAudioCounterWindow.h"
#include <cassert>
#include <cstdint>

using pocketjazz::DiagnosticAudioCounterWindow;

int main() {
  DiagnosticAudioCounterWindow counters;
  assert(counters.under(12) == 12);
  assert(counters.opens(3) == 3);
  assert(counters.frames(5000) == 5000);

  // The UI's X reset changes only its baseline. No audio-task raw counter
  // needs to be rewritten while PCM and TLS/HTTP are running.
  counters.reset(12, 3, 5000);
  assert(counters.under(12) == 0);
  assert(counters.opens(3) == 0);
  assert(counters.frames(5000) == 0);
  assert(counters.under(14) == 2);
  assert(counters.opens(4) == 1);
  assert(counters.frames(5999) == 999);

  // New manual X reset while playback is running.
  counters.reset(14, 4, 5999);
  assert(counters.under(14) == 0 && counters.opens(4) == 0);
  assert(counters.frames(5999) == 0);
  assert(counters.frames(6100) == 101);

  // Correct unsigned modulo arithmetic if a long-running uptime wraps.
  counters.reset(UINT32_MAX - 2, UINT32_MAX - 1, UINT32_MAX);
  assert(counters.under(2) == 5);
  assert(counters.opens(1) == 3);
  assert(counters.frames(6) == 7);
}
