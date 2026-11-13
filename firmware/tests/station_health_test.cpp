#ifdef NDEBUG
#undef NDEBUG // Release CTest must execute every assertion.
#endif
#include "../PocketJazzRadio/StationHealth.h"
#include <cassert>

int main() {
  pocketjazz::StationHealth<64> health;
  assert(health.streak(0) == 0 && !health.warning(0));
  health.failed(0);
  health.failed(0);
  assert(health.streak(0) == 2 && !health.warning(0));
  health.failed(0);
  assert(health.streak(0) == 3 && health.warning(0));
  for (int i = 0; i < 10000; ++i) health.failed(0);
  assert(health.streak(0) == 3); // no wrap or flash churn
  health.failed(63);
  assert(health.streak(63) == 1);
  health.failed(64);            // no OOB write
  health.failed(static_cast<size_t>(-1));
  assert(health.streak(64) == 0);
  assert(health.streak(0) == 3);
  health.recovered(0);
  assert(!health.warning(0) && health.streak(63) == 1);
  health.reset();
  assert(health.streak(63) == 0);
  return 0;
}
