#ifdef NDEBUG
#undef NDEBUG // Release CTest must execute every assertion.
#endif
#include "../PocketJazzRadio/WifiProfilePolicy.h"
#include <cassert>
#include <cstring>
using namespace pocketjazz;

int main() {
  WifiProfilePolicy p;
  assert(p.count() == 0 && p.find("home") == -1);
  assert(!p.put("", "password123"));
  assert(!p.put("home", "short"));
  assert(!p.put("home", "bad\npass123"));
  assert(!p.put("home\n", "password123"));
  char tooLong[35]; std::memset(tooLong, 'a', 34); tooLong[34] = 0;
  assert(!p.put(tooLong, "password123"));
  char longPass[65]; std::memset(longPass, 'b', 64); longPass[64] = 0;
  assert(!p.put("home", longPass));
  assert(p.put("home", "password123"));
  assert(p.put("hotspot", "abcdefgh"));
  assert(p.count() == 2);
  assert(!p.put("third", "abcdefgh")); // no silent eviction
  assert(p.put("home", "newpassword123"));
  assert(p.count() == 2 && !std::strcmp(p.at(0)->password, "newpassword123"));
  assert(!p.at(-1) && !p.at(2));
  WifiScanCandidate scan[] = {{"untrusted", -20}, {"home", -75}, {"hotspot", -45}};
  assert(p.choose(scan, 3) == 1);           // strongest known, not strongest unknown
  assert(p.choose(scan, 3, 1) == 0);        // avoid failed network
  assert(p.choose(scan, 3, 0) == 1);
  WifiScanCandidate onlyHome[] = {{"home", -80}};
  assert(p.choose(onlyHome, 1, 0) == 0);   // bounded retry when alone
  WifiScanCandidate strangers[] = {{"other", -15}};
  assert(p.choose(strangers, 1) == -1);
  assert(p.choose(nullptr, 1) == -1);
  assert(p.put("hotspot", ""));           // explicitly open network
  assert(p.count() == 2);
  p.clear();
  assert(p.count() == 0 && p.find("home") == -1);
  return 0;
}
