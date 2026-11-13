#ifdef NDEBUG
#undef NDEBUG // Release asserts are actual tests.
#endif
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioShortageEpisode.h"
#include <cassert>

int main() {
  audio_buffer::ShortageEpisode episode;
  assert(!episode.onRead(0, 0));
  assert(!episode.onRead(1536, 1536)); // full direct read is not shortage
  assert(episode.onRead(1536, 200));   // start the actual shortage
  assert(episode.starving());
  for (int i = 0; i < 2000; ++i)
    assert(!episode.onRead(1536, 0));  // one continuous 2-s polling gap
  assert(!episode.onRead(1536, 1000)); // still short
  assert(!episode.onRead(1536, 1536)); // full resume rearms
  assert(!episode.starving());
  assert(episode.onRead(1536, 0));     // second distinct shortage
  episode.reset();                     // seek/close/next session
  assert(episode.onRead(1, 0));
  assert(!episode.onRead(0, 0));        // explicit zero-byte prefill is not shortage
  assert(episode.onRead(1, 0));
  return 0;
}
