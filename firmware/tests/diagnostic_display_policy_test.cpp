#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/DiagnosticDisplayPolicy.h"
#include <cassert>
using pocketjazz::retrySecondsRemaining;
int main() {
  assert(retrySecondsRemaining(1000, 9000) == 8);
  assert(retrySecondsRemaining(1001, 9000) == 8); // not jittering in milliseconds
  assert(retrySecondsRemaining(2000, 9000) == 7);
  assert(retrySecondsRemaining(8999, 9000) == 1);
  assert(retrySecondsRemaining(9000, 9000) == 0);
  assert(retrySecondsRemaining(9001, 9000) == 0);
  // Signed modular time comparison remains correct around millis() rollover.
  assert(retrySecondsRemaining(UINT32_MAX - 1999, 2000) == 4);
  assert(retrySecondsRemaining(UINT32_MAX - 1, 2000) == 3);
  assert(retrySecondsRemaining(2001, 2000) == 0);
  return 0;
}
