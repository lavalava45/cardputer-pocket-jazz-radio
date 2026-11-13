#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/PreferenceGuards.h"
#include <cassert>
#include <cstring>

int main() {
  using namespace pocketjazz;
  assert(canWritePreferenceSchema(0)); // old v0.1 (schema missing)
  assert(canWritePreferenceSchema(1));
  assert(canWritePreferenceSchema(2));
  assert(!canWritePreferenceSchema(3));
  assert(!canWritePreferenceSchema(255));
  assert(validSavedStreamUrl("https://stream.example.org/a.mp3"));
  assert(validSavedStreamUrl("http://example.net/a.mp3"));
  assert(validSavedStreamUrl("https://host.example/a?bitrate=128&v=2"));
  assert(!validSavedStreamUrl(nullptr));
  assert(!validSavedStreamUrl(""));
  assert(!validSavedStreamUrl("file:///sd/jazz.mp3"));
  assert(!validSavedStreamUrl("https://"));
  assert(!validSavedStreamUrl("https://example.org/\nHEADER:injected"));
  assert(!validSavedStreamUrl("https://user:pass@example.org/stream"));
  assert(!validSavedStreamUrl("https://example.org/abc#fragment"));
  char tooLong[200] = {};
  std::memset(tooLong, 'a', sizeof(tooLong));
  assert(!validSavedStreamUrl(tooLong, 192));
  char saved[192] = {};
  const char* valid = "https://stream.example.org/live.mp3";
  assert(copyValidatedSavedStreamUrl(saved, valid, std::strlen(valid)));
  assert(std::strcmp(saved, valid) == 0);
  // 191 bytes plus NUL fits; the first non-NUL byte at index 192 does not.
  char limit[194] = "https://example.org/";
  std::memset(limit + std::strlen(limit), 'a', 191 - std::strlen(limit));
  limit[191] = '\0';
  assert(copyValidatedSavedStreamUrl(saved, limit, 191));
  assert(std::strlen(saved) == 191);
  limit[191] = 'a'; limit[192] = '\0';
  assert(!copyValidatedSavedStreamUrl(saved, limit, 192));
  assert(saved[0] == '\0');
  // Exercise the real restoration sequence: full URL is read, validated,
  // *then* copied. A truncated valid-looking URL must never be admitted.
  assert(!copyValidatedSavedStreamUrl(saved, limit, std::strlen(limit)));
  assert(!copyValidatedSavedStreamUrl(saved, valid, std::strlen(valid) - 1));
  assert(!copyValidatedSavedStreamUrl(saved, "file:///private/song", 20));
  assert(!copyValidatedSavedStreamUrl(saved, nullptr, 0));
  return 0;
}
