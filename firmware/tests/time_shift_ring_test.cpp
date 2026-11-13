#ifdef NDEBUG
#undef NDEBUG // Keep release-build regression checks active.
#endif
#include "../experiments/TimeShiftRing.h"
#include <cassert>
#include <vector>
#include <cstdint>
#include <cstring>

struct MemoryDisk : pocketjazz::TimeShiftStore {
  explicit MemoryDisk(size_t length) : bytes(length, 0), badWrite(false), badRead(false) {}
  bool writeAt(uint32_t position, const uint8_t* input, size_t length) override {
    if (badWrite || (uint64_t)position + length > bytes.size()) return false;
    std::memcpy(bytes.data() + position, input, length);
    return true;
  }
  bool readAt(uint32_t position, uint8_t* output, size_t length) override {
    if (badRead || (uint64_t)position + length > bytes.size()) return false;
    std::memcpy(output, bytes.data() + position, length);
    return true;
  }
  std::vector<uint8_t> bytes;
  bool badWrite, badRead;
};

int main() {
  MemoryDisk disk(4096);
  pocketjazz::TimeShiftRing ring;
  assert(!ring.begin(disk, 4095)); // capacity floor
  assert(ring.begin(disk, 4096));
  std::vector<uint8_t> a(3000), b(1500);
  for (size_t i = 0; i < a.size(); ++i) a[i] = uint8_t(i % 251);
  for (size_t i = 0; i < b.size(); ++i) b[i] = uint8_t((i + 19) % 253);
  assert(ring.append(a.data(), a.size()));
  assert(ring.head() == 3000 && ring.floor() == 0);
  uint8_t result[2000] = {};
  assert(ring.read(result, 1000) == 1000);
  assert(std::memcmp(result, a.data(), 1000) == 0);
  assert(ring.append(b.data(), b.size())); // physical wrap at 4096
  assert(ring.head() == 4500 && ring.floor() == 404);
  assert(!ring.seek(0)); // already overwritten, do not falsely replay
  assert(ring.seek(3000));
  assert(ring.read(result, 1500) == 1500);
  assert(std::memcmp(result, b.data(), 1500) == 0);
  assert(ring.cursor() == ring.head());
  assert(ring.seek(404));
  assert(ring.read(result, 1000) == 1000);
  assert(std::memcmp(result, a.data() + 404, 1000) == 0);
  disk.badRead = true;
  const uint64_t beforeRead = ring.cursor();
  assert(ring.read(result, 5) == 0 && ring.cursor() == beforeRead);
  disk.badRead = false;
  assert(!ring.healthy()); // failed SD I/O invalidates the in-memory timeline
  assert(ring.read(result, 5) == 0);
  assert(ring.begin(disk, 4096)); // must explicitly reset after a fault
  assert(ring.append(a.data(), 3000));
  disk.badWrite = true;
  const uint64_t beforeWrite = ring.head();
  assert(!ring.append(b.data(), 100) && ring.head() == beforeWrite);
  disk.badWrite = false;
  assert(!ring.healthy());
  assert(!ring.append(b.data(), 100));
  assert(ring.begin(disk, 4096));
  assert(ring.append(a.data(), 3000));
  assert(ring.append(b.data(), 1500));
  ring.live();
  assert(ring.read(result, 5) == 0);
  assert(ring.seek(4500));
  std::vector<uint8_t> c(4096, 77);
  assert(ring.append(c.data(), c.size()));
  assert(ring.floor() == 4500);
  assert(ring.seek(4500));
  assert(ring.read(result, 2000) == 2000);
  ring.live();
  assert(!ring.seek(4499));
  // Catch up if reader gets overtaken by producer.
  assert(ring.seek(4500));
  assert(ring.append(a.data(), 3000));
  assert(ring.read(result, 1) == 1);
  assert(ring.replayOverrunBytes() > 0);
  return 0;
}
