#ifdef NDEBUG
#undef NDEBUG // Verify every condition in the MSVC Release CTest gate.
#endif
#include "../PocketJazzRadio/RadioBufferLevelPolicy.h"
#include <cassert>

int main() {
  pocketjazz::RadioBufferLevelPolicy b;
  assert(!b.valid() && b.current() == 0 && b.minimum() == 0);
  b.observe(2048, 100);
  assert(b.valid() && b.current() == 2048 && b.minimum() == 2048 && b.minAtMs() == 100);
  b.observe(32767, 200);
  assert(b.current() == 32767 && b.minimum() == 2048 && b.minAtMs() == 100);
  b.observe(12, 310);
  assert(b.minimum() == 12 && b.minAtMs() == 310);
  b.observe(50, 320);
  assert(b.current() == 50 && b.minimum() == 12);
  b.reset(); // STOP, reconnect, station switch and decoder EOF must all clear stale readings.
  assert(!b.valid() && b.current() == 0 && b.minimum() == 0 && b.minAtMs() == 0);
  b.observe(900, 450);
  assert(b.current() == 900 && b.minimum() == 900 && b.minAtMs() == 450);
  return 0;
}
