#ifdef NDEBUG
#undef NDEBUG // Assertions remain active in MSVC Release.
#endif
#include "../PocketJazzRadio/BoundedLog.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

using pocketjazz::log::canAppend;
using pocketjazz::log::cleanTsvField;

int main() {
  const size_t max = std::numeric_limits<size_t>::max();
  assert(canAppend(0, 16384));
  assert(canAppend(16384, 0));
  assert(!canAppend(16384, 1));
  assert(!canAppend(16385, 0));
  assert(!canAppend(max, 1));
  assert(!canAppend(1, max));
  assert(!canAppend(max, max));
  assert(canAppend(max - 5, 5, max));
  assert(!canAppend(max - 5, 6, max));
  assert(canAppend(0, 0, 0));
  assert(!canAppend(1, 0, 0));

  char out[128];
  const char* metadata = "Miles\tDavis\r\n Live\x01 Jazz";
  const size_t n = cleanTsvField(metadata, out, sizeof(out));
  assert(std::string(out) == "Miles Davis   Live  Jazz");
  assert(n == std::strlen(out));
  assert(out[n] == '\0');

  const char* unicode = "Caf\xc3\xa9 \xe2\x80\x93 \xe6\x9d\xb1\xe4\xba\xac \xc3\xa9";
  assert(cleanTsvField(unicode, out, sizeof(out)) == std::strlen(unicode));
  assert(std::strcmp(out, unicode) == 0);
  const char* dangerous = u8"a\u0085b\u2028c\u2029d\u200be\u202ef\ufeffg";
  cleanTsvField(dangerous, out, sizeof(out));
  assert(std::strcmp(out, "a b c d e f g") == 0);

  const char invalid[] = {'A', char(0xff), char(0xc0), char(0xaf), 'Z', 0};
  cleanTsvField(invalid, out, sizeof(out));
  assert(std::strcmp(out, "A   Z") == 0);
  const char invalidSurrogate[] = {char(0xed), char(0xa0), char(0x80), 0};
  cleanTsvField(invalidSurrogate, out, sizeof(out));
  assert(std::strcmp(out, "   ") == 0);
  const char overlong[] = {char(0xf0), char(0x80), char(0x80), char(0xaf), 0};
  cleanTsvField(overlong, out, sizeof(out));
  assert(std::strcmp(out, "    ") == 0);

  // Do not split a valid UTF-8 sequence at a destination byte boundary.
  char tiny[4] = {'x','x','x','x'};
  const char* eAcuteA = "\xc3\xa9" "A";
  assert(cleanTsvField(eAcuteA, tiny, 2) == 0);
  assert(tiny[0] == '\0');
  assert(cleanTsvField(eAcuteA, tiny, 3) == 2);
  assert(static_cast<unsigned char>(tiny[0]) == 0xc3);
  assert(static_cast<unsigned char>(tiny[1]) == 0xa9);
  assert(tiny[2] == '\0');
  assert(cleanTsvField(eAcuteA, tiny, sizeof(tiny)) == 3);
  assert(std::strcmp(tiny, eAcuteA) == 0);
  assert(cleanTsvField("hello", tiny, 1) == 0 && tiny[0] == '\0');
  assert(cleanTsvField("hello", nullptr, 5) == 0);
  assert(cleanTsvField(nullptr, tiny, sizeof(tiny)) == 0 && tiny[0] == '\0');
  assert(cleanTsvField("hello", tiny, 0) == 0);

  const std::string huge(2000, 'x');
  char bounded[600];
  assert(cleanTsvField(huge.c_str(), bounded, sizeof(bounded)) == 512);
  assert(bounded[512] == '\0');
  const std::string edge(511, 'a');
  const std::string split = edge + "\xc3\xa9" + "ignored";
  assert(cleanTsvField(split.c_str(), bounded, sizeof(bounded)) == 512);
  assert(bounded[511] == ' ' && bounded[512] == '\0');
  return 0;
}
