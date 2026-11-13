#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../experiments/TimeShiftHostFileStore.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

int main() {
  using pocketjazz::TimeShiftHostFileStore;
  std::FILE* file = std::tmpfile();
  assert(file);
  TimeShiftHostFileStore store;
  assert(!store.attach(file, 4096)); // no implicit file creation/resize
  assert(std::fseek(file, 4095, SEEK_SET) == 0 && std::fputc(0, file) != EOF);
  assert(std::fflush(file) == 0);
  assert(!store.attach(file, 8192));
  assert(store.attach(file, 4096));
  pocketjazz::TimeShiftRing ring;
  assert(ring.begin(store, 4096));
  std::vector<uint8_t> first(3000), second(1500);
  for (size_t i=0;i<first.size();++i) first[i]=static_cast<uint8_t>(i%251);
  for (size_t i=0;i<second.size();++i) second[i]=static_cast<uint8_t>((i+39)%251);
  assert(ring.append(first.data(), first.size()));
  assert(ring.append(second.data(), second.size()));
  assert(ring.floor()==404 && ring.head()==4500);
  assert(!ring.seek(0));
  assert(ring.seek(3000));
  std::vector<uint8_t> read(second.size());
  assert(ring.read(read.data(), read.size())==second.size());
  assert(read == second);
  store.detach();
  assert(!ring.append(first.data(), 200));
  assert(!ring.healthy()); // fail closed after SD-equivalent removal
  assert(ring.read(read.data(), 100)==0);
  // File ownership remains with caller, and data is still there.
  assert(store.attach(file, 4096));
  assert(std::fseek(file, 3000, SEEK_SET)==0);
  uint8_t check[4] = {};
  assert(std::fread(check, 1, sizeof(check), file)==sizeof(check));
  assert(std::memcmp(check, second.data(), sizeof(check))==0);
  std::fclose(file);
  return 0;
}
