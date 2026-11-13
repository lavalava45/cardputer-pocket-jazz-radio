#ifdef NDEBUG
#undef NDEBUG // Assert negative and cancellation cases in MSVC Release.
#endif
#include "../PocketJazzRadio/AudioIntentPolicy.h"
#include <cassert>
#include <cstdint>

int main() {
  using pocketjazz::audioIntentSuperseded;
  const uint32_t connecting = 19;
  assert(!audioIntentSuperseded(connecting, connecting)); // volume-only command
  assert(audioIntentSuperseded(connecting, connecting + 1)); // STOP during GET
  assert(audioIntentSuperseded(connecting, connecting + 2)); // switch during prefill
  assert(audioIntentSuperseded(UINT32_MAX, 1)); // sequence wrap; 0 is skipped
  assert(!audioIntentSuperseded(1, 1));
  return 0;
}
