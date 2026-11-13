#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/HeapLifecycleProfile.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <atomic>

using pocketjazz::HeapMoment;
using pocketjazz::HeapCheckpoint;
using pocketjazz::HeapLifecycleProfile;

int main() {
  HeapLifecycleProfile profile;
  HeapCheckpoint read{};
  assert(!profile.read(HeapMoment::PRE_RING, read));
  assert(!profile.firstStallEver(read));
  assert(!profile.read(HeapMoment::COUNT, read));
  auto point = [](uint32_t attempt, uint32_t free, uint32_t largest) {
    HeapCheckpoint c{};
    c.attempt = attempt;
    c.atMs = attempt * 1000;
    c.free8 = free; c.largest8 = largest;
    c.freeDma = free / 2; c.largestDma = largest / 2;
    c.ringBytes = 16384;
    return c;
  };
  profile.begin(21);
  profile.capture(HeapMoment::PRE_RING, point(20, 180000, 50000));
  assert(!profile.read(HeapMoment::PRE_RING, read)); // stale sequence rejected
  profile.capture(HeapMoment::PRE_RING, point(21, 164000, 45000));
  assert(profile.read(HeapMoment::PRE_RING, read) && read.free8 == 164000);
  profile.capture(HeapMoment::LOW_FREE, point(21, 13000, 4300));
  profile.capture(HeapMoment::LOW_BLOCK, point(21, 13000, 4300));
  profile.capture(HeapMoment::LOW_FREE, point(21, 15000, 9000));
  profile.capture(HeapMoment::LOW_BLOCK, point(21, 15000, 9000));
  assert(profile.read(HeapMoment::LOW_FREE, read) && read.free8 == 13000);
  assert(profile.read(HeapMoment::LOW_BLOCK, read) && read.largest8 == 4300);
  profile.capture(HeapMoment::LOW_FREE, point(21, 12500, 4900));
  profile.capture(HeapMoment::LOW_BLOCK, point(21, 12500, 4900));
  assert(profile.read(HeapMoment::LOW_FREE, read) && read.free8 == 12500);
  assert(profile.read(HeapMoment::LOW_BLOCK, read) && read.largest8 == 4300);
  profile.capture(HeapMoment::FIRST_STALL, point(21, 8500, 1320));
  profile.capture(HeapMoment::FIRST_STALL, point(21, 7000, 800));
  assert(profile.read(HeapMoment::FIRST_STALL, read) && read.free8 == 8500);
  assert(profile.firstStallEver(read) && read.attempt == 21);
  profile.begin(22);
  assert(!profile.read(HeapMoment::PRE_RING, read));
  assert(!profile.read(HeapMoment::FIRST_STALL, read));
  assert(profile.firstStallEver(read) && read.attempt == 21);
  profile.capture(HeapMoment::FIRST_STALL, point(22, 6000, 700));
  assert(profile.firstStallEver(read) && read.attempt == 21);
  assert(profile.read(HeapMoment::FIRST_STALL, read) && read.attempt == 22);
  assert(!std::strcmp(pocketjazz::heapMomentName(HeapMoment::RING_HELD), "RING_HELD"));

  // Short critical sections protect a UI reader from a different core.
  std::atomic<bool> done{false};
  std::thread producer([&]() {
    for (uint32_t n = 23; n < 123; ++n) {
      profile.begin(n);
      profile.capture(HeapMoment::PRE_RING, point(n, n * 100, n * 50));
    }
    done = true;
  });
  while (!done.load()) {
    if (profile.read(HeapMoment::PRE_RING, read))
      assert(read.free8 == read.attempt * 100 &&
             read.largest8 == read.attempt * 50);
  }
  producer.join();
  return 0;
}
