#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/JazzIcyArtist.h"
#include <cassert>
#include <cstring>

static void accepted(const char* text, const char* artist, const char* songTitle) {
  size_t n = 0;
  const char* song = nullptr;
  assert(pocketjazz::strictIcyArtistTrack(text, n, song));
  assert(strlen(artist) == n && !strncmp(text, artist, n));
  assert(song && !strcmp(song, songTitle));
}
static void rejected(const char* text) {
  size_t n = 999;
  const char* song = text;
  assert(!pocketjazz::strictIcyArtistTrack(text, n, song));
  assert(n == 0 && song == nullptr);
}
int main() {
  accepted("Miles Davis - So What", "Miles Davis", "So What");
  accepted("Miles Davis - A-Tisket, A-Tasket", "Miles Davis", "A-Tisket, A-Tasket");
  accepted("Miles Davis - Song – With Dash", "Miles Davis", "Song – With Dash");
  accepted("Unknown Artist - Unknown Track", "Unknown Artist", "Unknown Track");
  rejected("Miles Davis - John Coltrane - Blue Train");
  rejected("Miles Davis - \"John Coltrane - Blue Train\"");
  rejected("Miles Davis - ");
  rejected("Miles Davis -    ");
  rejected(" - So What");
  rejected("Miles Davis");
  rejected("");
  rejected(nullptr);
  rejected("Miles Davis - Song - Other - Piece");
  return 0;
}
