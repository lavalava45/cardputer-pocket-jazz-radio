#ifdef NDEBUG
#undef NDEBUG // MSVC Release must actually run the negative-case assertions.
#endif
#include "../PocketJazzRadio/AudioHotPathTiming.h"
#include <cassert>
#include <cstdint>

int main() {
  std::atomic<uint32_t> maxUs{0};
  pocketjazz::observeMaxMicros(maxUs, 0);
  assert(maxUs.load() == 0);
  pocketjazz::observeMaxMicros(maxUs, 1200);
  pocketjazz::observeMaxMicros(maxUs, 1199);
  assert(maxUs.load() == 1200);
  pocketjazz::observeMaxMicros(maxUs, 5000);
  assert(maxUs.load() == 5000);
  maxUs.store(0); // user pressed Diagnostics X: old maxima must not reappear
  pocketjazz::observeMaxMicros(maxUs, 44);
  assert(maxUs.load() == 44);
  assert(pocketjazz::elapsedMicros(100, 160) == 60);
  assert(pocketjazz::elapsedMicros(UINT32_MAX - 5, 8) == 14);
  return 0;
}
