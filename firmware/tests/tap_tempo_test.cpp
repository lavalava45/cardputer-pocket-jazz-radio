#ifdef NDEBUG
#undef NDEBUG // Keep release-build regression checks active.
#endif
#include "../PocketJazzRadio/TapTempo.h"
#include <cassert>
#include <cstdint>

int main() {
  pocketjazz::TapTempo tap;
  assert(tap.bpm(0) == 0);
  assert(tap.tap(100));
  assert(!tap.tap(150)); // debounce: does not disturb the period
  assert(tap.tap(600));
  assert(tap.tap(1100));
  assert(tap.tap(1600));
  assert(tap.bpm(1600) == 120);
  assert(tap.intervals() == 3);
  assert(tap.tap(2100));
  assert(tap.tap(2550)); // one outlier
  assert(tap.tap(3100));
  assert(tap.bpm(3100) == 120); // median resists an outlier
  assert(tap.bpm(9000) == 0);   // stale estimate
  assert(tap.tap(9600));        // long pause resets estimate
  assert(tap.intervals() == 0 && tap.bpm(9600) == 0);
  assert(tap.tap(10200));
  assert(tap.tap(10800));
  assert(tap.tap(11400));
  assert(tap.bpm(11400) == 100);
  tap.reset();
  assert(tap.intervals() == 0 && tap.bpm(12000) == 0);
  // millis() wraps on ESP32 around 49 days. Unsigned subtraction is safe.
  const uint32_t nearWrap = UINT32_MAX - 399;
  assert(tap.tap(nearWrap));
  assert(tap.tap(100)); // gap = 500
  assert(tap.tap(600));
  assert(tap.tap(1100));
  assert(tap.bpm(1100) == 120);
  // 1 tap is not enough to invent BPM
  tap.reset();
  assert(tap.tap(2000));
  assert(tap.bpm(2000) == 0);
  return 0;
}
