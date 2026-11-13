#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/PrefillBudget.h"
#include <cassert>
using pocketjazz::initialMp3PrefillGoal;
using pocketjazz::shouldKeepPrefilling;
int main() {
  assert(initialMp3PrefillGoal(16u * 1024u) == 8192u);
  assert(initialMp3PrefillGoal(24u * 1024u) == 8192u);
  assert(initialMp3PrefillGoal(32u * 1024u) == 8192u);
  assert(initialMp3PrefillGoal(8u * 1024u) == 4096u);
  assert(initialMp3PrefillGoal(0) == 0);
  // No unbounded wait when a server fails to deliver adequate bytes.
  assert(shouldKeepPrefilling(100, 0, 2048, 8192, 5000));
  assert(!shouldKeepPrefilling(5000, 0, 2048, 8192, 5000));
  assert(!shouldKeepPrefilling(100, 0, 8192, 8192, 5000));
  // Unsigned rollover remains bounded.
  assert(shouldKeepPrefilling(50, 0xfffffff0u, 0, 8192, 5000));
  return 0;
}
