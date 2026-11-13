#include "../PocketJazzRadio/StationAltUrlPool.h"
#ifdef NDEBUG
#undef NDEBUG // These runtime contract checks must execute in Release/CTest.
#endif
#include <cassert>
#include <cstring>
#include <string>

int main() {
  using pocketjazz::StationAltUrlPool;
  StationAltUrlPool pool;
  assert(pool.capacity() == 0);
  assert(pool.get(StationAltUrlPool::kNone)[0] == '\0');
  assert(pool.add(nullptr) == StationAltUrlPool::kNone);
  assert(pool.add("") == StationAltUrlPool::kNone);

  const uint16_t a = pool.add("https://ice5.somafm.com/sonicuniverse-128-mp3");
  const uint16_t b = pool.add("https://ice2.somafm.com/sonicuniverse-128-mp3");
  assert(a == 0 && b != StationAltUrlPool::kNone);
  assert(std::strcmp(pool.get(a), "https://ice5.somafm.com/sonicuniverse-128-mp3") == 0);
  assert(std::strcmp(pool.get(b), "https://ice2.somafm.com/sonicuniverse-128-mp3") == 0);

  // Reallocate beyond the first 1 KiB and verify earlier indexed strings.
  for (unsigned i=0; i<32; ++i) {
    const std::string url = "https://radio.example/backup" + std::to_string(i) + ".mp3";
    assert(pool.add(url.c_str()) != StationAltUrlPool::kNone);
  }
  assert(std::strcmp(pool.get(a), "https://ice5.somafm.com/sonicuniverse-128-mp3") == 0);
  assert(std::strcmp(pool.get(b), "https://ice2.somafm.com/sonicuniverse-128-mp3") == 0);

  // A playlist reload invalidates old indexes but leaves the buffer allocated.
  const size_t oldCapacity = pool.capacity();
  pool.reset();
  assert(pool.used() == 0);
  assert(pool.get(a)[0] == '\0');
  assert(pool.capacity() == oldCapacity);

  // Worst-case 64 stations each with a valid 191-byte fallback URL.
  std::string maxUrl = "https://" + std::string(183, 'x');
  assert(maxUrl.size() == 191);
  for (unsigned i=0; i<64; ++i) {
    const uint16_t offset = pool.add(maxUrl.c_str());
    assert(offset != StationAltUrlPool::kNone);
    assert(std::strcmp(pool.get(offset), maxUrl.c_str()) == 0);
  }
  assert(pool.used() == StationAltUrlPool::kMaxPoolBytes);
  assert(pool.add(maxUrl.c_str()) == StationAltUrlPool::kNone);
  assert(pool.add(std::string(192, 'y').c_str()) == StationAltUrlPool::kNone);
  return 0;
}
