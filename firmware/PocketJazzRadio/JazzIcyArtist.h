// SPDX-License-Identifier: MIT
// Conservative ICY interpretation for UI previews and offline artist lookup.
#pragma once
#include <stddef.h>
#include <string.h>

namespace pocketjazz {
// A station's free-form StreamTitle is not authoritative artist metadata.
// Accept only one exact, nonempty "Artist - Track" separator. A second one
// (even in a quoted title) is ambiguous and must not trigger attribution.
inline bool strictIcyArtistTrack(const char* metadata, size_t& artistBytes,
                                 const char*& song) {
  artistBytes = 0;
  song = nullptr;
  if (!metadata || !*metadata) return false;
  const char* delimiter = strstr(metadata, " - ");
  if (!delimiter || delimiter == metadata || !delimiter[3] ||
      strstr(delimiter + 3, " - ")) return false;
  const char* first = metadata;
  const char* last = delimiter;
  while (first < last && *first == ' ') ++first;
  while (last > first && last[-1] == ' ') --last;
  const char* track = delimiter + 3;
  while (*track == ' ') ++track;
  if (last == first || !*track || strlen(track) > 95) return false;
  // Keep the artist prefix faithful to source: callers must not assign an
  // identity based on a truncated name.
  if (first != metadata || size_t(last - first) > 95) return false;
  artistBytes = size_t(last - first);
  song = track;
  return true;
}
} // namespace pocketjazz
