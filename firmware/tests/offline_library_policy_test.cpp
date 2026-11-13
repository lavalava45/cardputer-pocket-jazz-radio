#ifdef NDEBUG
#undef NDEBUG // Release assertions are actually evaluated.
#endif
#include "../PocketJazzRadio/OfflineLibraryPolicy.h"
#include <cassert>
#include <cstring>

using Library = pocketjazz::OfflineLibraryPolicy<3>;

int main() {
  Library lib;
  assert(lib.count() == 0 && !lib.at(0));
  assert(!lib.add("../../private.mp3", 12345, false));
  assert(!lib.add("folder/song.mp3", 12345, false));
  assert(!lib.add("folder\\song.mp3", 12345, false));
  assert(!lib.add(".hidden.mp3", 12345, false));
  assert(!lib.add("song.mp3", 2000, true));
  assert(!lib.add("song.flac", 2000, false));
  assert(!lib.add("song.mp3", 1000, false));
  assert(!lib.add("song.mp3", 513ull * 1024 * 1024, false));
  assert(!lib.add("bad\xC0\x80.mp3", 2000, false)); // overlong UTF-8
  assert(lib.add("Song.MP3", 1024, false));
  assert(!lib.add("song.mp3", 2048, false)); // case-insensitive FAT duplicate
  assert(lib.add("\xD0\x94\xD0\xB6\xD0\xB0\xD0\xB7.mp3", 3000, false)); // UTF-8 jazz
  assert(lib.add("bossa.mp3", 5000000, false));
  assert(lib.count() == 3);
  assert(!lib.add("extra.mp3", 5000, false));
  assert(!lib.at(3));
  char path[192];
  assert(Library::makePath(lib.at(1)->filename, path, sizeof(path)));
  assert(std::strstr(path, "/PocketJazz/offline/") == path);
  assert(!Library::makePath("../my.mp3", path, sizeof(path)));
  assert(!Library::makePath("okay.mp3", path, 10));
  lib.clear();
  assert(lib.count() == 0 && !lib.at(0));

  const uint8_t bare[10] = {0xFF, 0xFB, 0x90, 0x64, 0, 0, 0, 0, 0, 0};
  uint32_t offset = 99;
  assert(Library::id3Start(bare, sizeof(bare), 12345, offset));
  assert(offset == 0);
  const uint8_t id3[10] = {'I','D','3',4,0,0,0,0,0x20,0}; // 4096 bytes
  assert(Library::id3Start(id3, sizeof(id3), 5000, offset));
  assert(offset == 4106);
  assert(!Library::id3Start(id3, sizeof(id3), 4106, offset));
  uint8_t corrupt[10]; std::memcpy(corrupt, id3, 10);
  corrupt[9] = 0x80;
  assert(!Library::id3Start(corrupt, 10, 5000, offset));
  std::memcpy(corrupt, id3, 10); corrupt[3] = 5;
  assert(!Library::id3Start(corrupt, 10, 5000, offset));
  std::memcpy(corrupt, id3, 10); corrupt[5] = 0x10; // v2.4 footer
  assert(Library::id3Start(corrupt, 10, 5000, offset) && offset == 4116);
  assert(!Library::id3Start(corrupt, 9, 5000, offset));
  assert(!Library::id3Start(nullptr, 10, 5000, offset));
  std::memcpy(corrupt, id3, 10); corrupt[6] = 0x7f;
  assert(!Library::id3Start(corrupt, 10, 0x2000000, offset)); // oversized tag
  return 0;
}
