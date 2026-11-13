#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/MarqueeScroll.h"
#include <cassert>
#include <cstdint>

int main() {
  using pocketjazz::marqueeOffset;
  // Short or exactly fitting text never animates (including timers wrapping).
  assert(marqueeOffset(0, 137, 137) == 0);
  assert(marqueeOffset(UINT32_MAX, 80, 137) == 0);
  assert(marqueeOffset(50000, 140, 0) == 0);
  // 24px overflow: initial pause; 3px per 125ms; end pause; restart.
  constexpr uint16_t text = 161, view = 137;
  assert(marqueeOffset(0, text, view) == 0);
  assert(marqueeOffset(1200, text, view) == 0);
  assert(marqueeOffset(1324, text, view) == 0);
  assert(marqueeOffset(1325, text, view) == 3);
  assert(marqueeOffset(1575, text, view) == 9);
  assert(marqueeOffset(2200, text, view) == 24);
  assert(marqueeOffset(3199, text, view) == 24);
  assert(marqueeOffset(3200, text, view) == 0);
  // Bound every position to the actual clipping width, no negative X cursor.
  for (uint32_t ms = 0; ms < 25000; ms += 61)
    assert(marqueeOffset(ms, 162, 137) <= 25);
  // millis() elapsed is unsigned: time arithmetic across wrap is predictable.
  const uint32_t start = UINT32_MAX - 100;
  assert(marqueeOffset(static_cast<uint32_t>(1500u - start), text, view) == 9);
  return 0;
}
