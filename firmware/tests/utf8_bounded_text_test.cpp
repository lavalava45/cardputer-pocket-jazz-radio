#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/Utf8BoundedText.h"
#include <cassert>
#include <cstring>
using pocketjazz::utf8SafePrefix;
using pocketjazz::copyUtf8Bounded;
int main() {
  constexpr char cyrillic[] = "Привет мир";
  const size_t count = strlen(cyrillic);
  assert(count == 19);
  assert(utf8SafePrefix(cyrillic, count, 7) == 6);
  assert(utf8SafePrefix(cyrillic, count, 8) == 8);
  char out[8] = {};
  assert(copyUtf8Bounded(out, sizeof(out), cyrillic, count) == 6);
  assert(!strcmp(out, "При"));
  char full[64]{};
  assert(copyUtf8Bounded(full, sizeof(full), cyrillic, count) == count);
  assert(!strcmp(full, cyrillic));
  assert(utf8SafePrefix("A\xE2\x82\xAC" "B", 5, 3) == 1);
  assert(utf8SafePrefix("A\xE2\x82\xAC" "B", 5, 4) == 4);
  assert(utf8SafePrefix("\xE2\x82", 2, 8) == 0);
  assert(utf8SafePrefix("\xC0\x80", 2, 2) == 0); // overlong NUL
  assert(utf8SafePrefix("\xED\xA0\x80", 3, 3) == 0); // surrogate
  assert(utf8SafePrefix("\xF4\x90\x80\x80", 4, 4) == 0);
  assert(utf8SafePrefix("\xF0\x9F\x8E\xB7", 4, 4) == 4); // saxophone symbol
  char untouched[] = "hi";
  assert(copyUtf8Bounded(untouched, 0, cyrillic, count) == 0);
  assert(!strcmp(untouched, "hi"));
  return 0;
}
