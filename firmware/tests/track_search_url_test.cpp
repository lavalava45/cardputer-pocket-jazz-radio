#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/TrackSearchUrl.h"
#include <cassert>
#include <cstring>
#include <string>

int main() {
  using pocketjazz::TrackSearchProvider;
  using pocketjazz::buildTrackSearchUrl;
  char url[107] = {};
  bool clipped = true;
  assert(buildTrackSearchUrl("John Coltrane - Naima", TrackSearchProvider::Spotify,
                             url, sizeof(url), &clipped));
  assert(!clipped);
  assert(std::strcmp(url, "https://open.spotify.com/search/John%20Coltrane%20-%20Naima") == 0);
  assert(buildTrackSearchUrl("Bossa & tune? #1", TrackSearchProvider::Web,
                             url, sizeof(url), &clipped));
  assert(std::strcmp(url, "https://www.google.com/search?q=Bossa%20%26%20tune%3F%20%231") == 0);
  assert(!clipped);

  const char* accented = "Bj\xC3\xB6rk - J\xC3\xB3ga";
  assert(buildTrackSearchUrl(accented, TrackSearchProvider::Spotify,
                             url, sizeof(url), &clipped));
  assert(std::strstr(url, "Bj%C3%B6rk") != nullptr);
  assert(std::strstr(url, "J%C3%B3ga") != nullptr);
  assert(!clipped);
  assert(!buildTrackSearchUrl("track-unknown", TrackSearchProvider::Spotify,
                              url, sizeof(url), &clipped));
  assert(url[0] == '\0');
  assert(!buildTrackSearchUrl(" ", TrackSearchProvider::Web,
                              url, sizeof(url)));
  assert(buildTrackSearchUrl("  Miles Davis - So What   ", TrackSearchProvider::Spotify,
                             url, sizeof(url), &clipped));
  assert(!clipped);
  assert(std::strcmp(url, "https://open.spotify.com/search/Miles%20Davis%20-%20So%20What") == 0);
  std::string padded(70, ' ');
  padded += "Miles Davis";
  assert(buildTrackSearchUrl(padded.c_str(), TrackSearchProvider::Spotify,
                             url, sizeof(url), &clipped));
  assert(!clipped);
  assert(std::strcmp(url, "https://open.spotify.com/search/Miles%20Davis") == 0);
  assert(!buildTrackSearchUrl("  track-unknown  ", TrackSearchProvider::Web,
                              url, sizeof(url)));
  assert(!buildTrackSearchUrl("abc\r\ndef", TrackSearchProvider::Web,
                              url, sizeof(url)));
  assert(!buildTrackSearchUrl("Invalid\xC0\xAF" "Title", TrackSearchProvider::Spotify,
                              url, sizeof(url)));
  assert(!buildTrackSearchUrl("Invalid\xED\xA0\x80" "Title", TrackSearchProvider::Web,
                              url, sizeof(url))); // UTF-8 surrogate
  assert(!buildTrackSearchUrl("Abc", TrackSearchProvider::Spotify, url, 15));
  assert(url[0] == '\0');

  std::string longTitle(88, 'a');
  longTitle += " x";
  assert(buildTrackSearchUrl(longTitle.c_str(), TrackSearchProvider::Spotify,
                             url, sizeof(url), &clipped));
  assert(clipped);
  assert(std::strlen(url) <= pocketjazz::kTrackQrMaxUrlBytes);
  assert(std::strncmp(url, "https://open.spotify.com/search/", 32) == 0);
  // Long multibyte suffix: never end with a partial %XX or a single UTF-8 byte.
  longTitle = std::string(65, 'a') + " \xD7\x90\xD7\x91\xD7\x92\xD7\x93";
  assert(buildTrackSearchUrl(longTitle.c_str(), TrackSearchProvider::Web,
                             url, sizeof(url), &clipped));
  assert(clipped);
  const size_t n = std::strlen(url);
  assert(n <= pocketjazz::kTrackQrMaxUrlBytes);
  assert(n >= 3 && url[n-1] != '%');
  return 0;
}
