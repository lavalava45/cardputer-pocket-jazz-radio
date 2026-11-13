#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/StationPeekPolicy.h"
#include <cassert>
#include <cstring>

int main() {
  pocketjazz::StationPeekPolicy<2> cache;
  const int visible[] = {4, 5, 6, 7};
  assert(cache.next(visible, 4, 4, 1000, true, true, 150000, 100000) == 5);
  assert(cache.next(visible, 4, 4, 1000, false, true, 150000, 100000) == -1);
  assert(cache.next(visible, 4, 4, 1000, true, false, 150000, 100000) == -1);
  assert(cache.next(visible, 4, 4, 1000, true, true, 119999, 100000) == -1);
  assert(cache.next(visible, 4, 4, 1000, true, true, 150000, 79999) == -1);
  assert(cache.next(visible, 5, 4, 1000, true, true, 150000, 100000) == -1);
  assert(cache.start(5, 1000, 10, 1));
  assert(!cache.start(6, 1001, 10, 2) && cache.busy());
  assert(cache.next(visible, 4, 4, 1001, true, true, 150000, 100000) == -1);
  assert(!cache.complete(6, "wrong station", 2000, 10, 1));
  assert(!cache.complete(5, "wrong generation", 2000, 11, 1));
  assert(!cache.complete(5, "wrong request", 2000, 10, 2));
  assert(cache.complete(5, "Miles Davis - So What", 2000, 10, 1));
  assert(cache.lookup(5)->hasTitle);
  assert(std::strcmp(cache.lookup(5)->title, "Miles Davis - So What") == 0);
  assert(cache.fresh(cache.lookup(5), 2001));
  assert(!cache.fresh(cache.lookup(5), 2000 + 180000));
  assert(cache.next(visible, 4, 4, 9999, true, true, 150000, 100000) == -1);
  assert(cache.next(visible, 4, 4, 11001, true, true, 150000, 100000) == 6);
  assert(cache.start(6, 11001, 10, 2));
  assert(cache.complete(6, nullptr, 12000, 10, 2));
  assert(cache.lookup(6) && !cache.lookup(6)->hasTitle);
  assert(cache.next(visible, 4, 4, 22000, true, true, 150000, 100000) == 7);
  assert(cache.start(7, 22000, 10, 3));
  assert(cache.complete(7, "new artist", 23000, 10, 3));
  assert(cache.lookup(7) && cache.lookup(7)->hasTitle);
  assert(cache.lookup(5) == nullptr); // bounded eviction
  cache.clear();
  assert(!cache.busy() && cache.lookup(7) == nullptr);
  assert(cache.next(nullptr, 4, -1, 100000, true, true, 150000, 100000) == -1);
  assert(cache.next(visible, 5, -1, 100000, true, true, 150000, 100000) == -1);
  // Zero origin and millisecond counter wrap cannot skip the spacing gate.
  pocketjazz::StationPeekPolicy<> wrap;
  assert(wrap.start(1, 0xfffffff0u, 3, 1));
  assert(wrap.complete(1, "Track", 0xfffffff5u, 3, 1));
  assert(wrap.next(visible, 4, -1, 10, true, true, 150000, 100000) == -1);
  assert(wrap.next(visible, 4, -1, 10100, true, true, 150000, 100000) == 4);

  // Playlist R race: old generation reply arrives after station ID reuse and
  // even after a new probe of that SAME numerical ID has started.
  pocketjazz::StationPeekPolicy<> reuse;
  assert(reuse.start(5, 500, 100, 77));
  reuse.clear();
  assert(!reuse.complete(5, "old stream", 501, 100, 77));
  assert(reuse.start(5, 502, 101, 78));
  assert(!reuse.complete(5, "old stream", 503, 100, 77));
  assert(!reuse.complete(5, "wrong request", 504, 101, 77));
  assert(reuse.busy() && reuse.lookup(5) == nullptr);
  assert(reuse.complete(5, "new stream", 505, 101, 78));
  assert(std::strcmp(reuse.lookup(5)->title, "new stream") == 0);
  // Reopening browse without R also invalidates old token, not just station.
  assert(reuse.start(5, 100000, 101, 79));
  reuse.cancel();
  assert(reuse.start(5, 100001, 101, 80));
  assert(!reuse.complete(5, "late view", 100002, 101, 79));
  assert(reuse.complete(5, "current view", 100003, 101, 80));

  // Valid UTF-8 exactly on byte 95, and trimming only INCOMPLETE codepoints.
  pocketjazz::StationPeekPolicy<> utf;
  char complete[128] = {};
  std::memset(complete, 'a', 93);
  complete[93] = char(0xc3);
  complete[94] = char(0xa9); // 93 ASCII + 2-byte é, exactly 95 bytes
  complete[95] = 'Z';
  assert(utf.start(4, 1, 1, 1));
  assert(utf.complete(4, complete, 2, 1, 1));
  assert(std::strlen(utf.lookup(4)->title) == 95);
  assert((unsigned char)utf.lookup(4)->title[94] == 0xa9);
  std::memset(complete, 'b', 94);
  complete[94] = char(0xc3); complete[95] = char(0xa9); complete[96] = '\0';
  assert(utf.start(4, 3, 1, 2));
  assert(utf.complete(4, complete, 4, 1, 2));
  assert(std::strlen(utf.lookup(4)->title) == 94); // no dangling lead byte
  std::memset(complete, 'c', 91);
  complete[91] = char(0xf0); complete[92] = char(0x9f);
  complete[93] = char(0x8e); complete[94] = char(0xb7); // complete 4-byte symbol
  complete[95] = 'Q'; complete[96] = '\0';
  assert(utf.start(4, 5, 1, 3));
  assert(utf.complete(4, complete, 6, 1, 3));
  assert(std::strlen(utf.lookup(4)->title) == 95);
  std::memset(complete, 'd', 94);
  complete[94] = char(0xe2); complete[95] = '\0'; // malformed end
  assert(utf.start(4, 7, 1, 4));
  assert(utf.complete(4, complete, 8, 1, 4));
  assert(std::strlen(utf.lookup(4)->title) == 94);
  std::memset(complete, 'e', 93);
  complete[93] = char(0xe2); complete[94] = char(0x82); complete[95] = '\0';
  assert(utf.start(4, 9, 1, 5));
  assert(utf.complete(4, complete, 10, 1, 5));
  assert(std::strlen(utf.lookup(4)->title) == 93);
  return 0;
}
