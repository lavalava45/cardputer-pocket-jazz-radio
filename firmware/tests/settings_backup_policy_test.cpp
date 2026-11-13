#ifdef NDEBUG
#undef NDEBUG // Release CTest must execute every assertion.
#endif
#include "../PocketJazzRadio/SettingsBackupPolicy.h"
#include <cassert>
#include <cstring>
#include <string>
using namespace pocketjazz;

int main() {
  SettingsBackupData original;
  original.volume = 130; original.list = 3; original.station = 7;
  original.autoplay = false; original.tzOverride = true; original.tzMinutes = 330;
  original.sparkAuto = false;
  std::strcpy(original.lastUrl, "https://radio.example/live?token=private");
  std::strcpy(original.presetUrl[0], "https://station.example/a.mp3");
  std::strcpy(original.presetName[0], "Bossa Night");
  char bytes[SettingsBackupPolicy::kMaxFileBytes]{};
  size_t length = 0;
  assert(SettingsBackupPolicy::encode(original, bytes, sizeof(bytes), length));
  assert(length < sizeof(bytes) && length > 100);
  std::string content(bytes, length);
  assert(content.find("PJSET1\n") == 0);
  assert(content.find("ssid") == std::string::npos);
  assert(content.find("pass=") == std::string::npos);
  assert(content.find("token=private") == std::string::npos); // URL hex encoded
  SettingsBackupData restored;
  assert(SettingsBackupPolicy::decode(bytes, length, restored));
  assert(restored.volume == 130 && restored.tzMinutes == 330 &&
         restored.autoplay == false && restored.sparkAuto == false);
  assert(!std::strcmp(restored.presetName[0], "Bossa Night"));
  assert(!std::strcmp(restored.lastUrl, original.lastUrl));

  // Import must never partly change destination on a malformed/torn SD file.
  SettingsBackupData sentinel;
  sentinel.volume = 42;
  assert(!SettingsBackupPolicy::decode(bytes, length - 1, sentinel));
  assert(sentinel.volume == 42);
  char corrupt[SettingsBackupPolicy::kMaxFileBytes]{};
  std::memcpy(corrupt, bytes, length);
  corrupt[8] = corrupt[8] == '1' ? '2' : '1';
  assert(!SettingsBackupPolicy::decode(corrupt, length, sentinel));
  assert(sentinel.volume == 42);
  std::memcpy(corrupt, bytes, length);
  std::memcpy(corrupt + 9, "ssid", 4); // invalid key and checksum
  assert(!SettingsBackupPolicy::decode(corrupt, length, sentinel));
  assert(sentinel.volume == 42);
  std::memcpy(corrupt, bytes, length);
  std::memcpy(corrupt, "PJSET2", 6);
  assert(!SettingsBackupPolicy::decode(corrupt, length, sentinel));
  assert(sentinel.volume == 42);
  assert(!SettingsBackupPolicy::decode(bytes, 8193, sentinel));
  assert(!SettingsBackupPolicy::decode(nullptr, length, sentinel));

  original.volume = 200;
  assert(!SettingsBackupPolicy::encode(original, bytes, sizeof(bytes), length));
  original.volume = 130;
  original.tzMinutes = 17;
  assert(!SettingsBackupPolicy::encode(original, bytes, sizeof(bytes), length));
  original.tzMinutes = 330;
  std::strcpy(original.presetUrl[1], "file:///secrets");
  assert(!SettingsBackupPolicy::encode(original, bytes, sizeof(bytes), length));
  original.presetUrl[1][0] = 0;
  assert(!SettingsBackupPolicy::encode(original, bytes, 80, length));

  // A nearly-full preset table remains bounded and round-trips without heap.
  for (int i = 0; i < 9; ++i) {
    std::strcpy(original.presetUrl[i], "https://some.example/live.mp3");
    std::strcpy(original.presetName[i], "Jazz playlist");
  }
  assert(SettingsBackupPolicy::encode(original, bytes, sizeof(bytes), length));
  assert(SettingsBackupPolicy::decode(bytes, length, restored));
  assert(!std::strcmp(restored.presetUrl[8], original.presetUrl[8]));
  return 0;
}
