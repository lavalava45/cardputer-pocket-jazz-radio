#ifdef NDEBUG
#undef NDEBUG // Keep release-build regression checks active.
#endif
#include "../PocketJazzRadio/ShareStationQR.h"
#include <cassert>
#include <cstring>

int main() {
  using pocketjazz::isPublicShareUrl;
  assert(isPublicShareUrl("https://ice5.somafm.com/sonicuniverse-128-mp3"));
  assert(isPublicShareUrl("https://stream.srg-ssr.ch/srgssr/rsj/mp3/128"));
  assert(!isPublicShareUrl("http://example.org/jazz.mp3"));
  assert(!isPublicShareUrl("https://localhost/jazz"));
  assert(!isPublicShareUrl("https://mydevice.local/stream"));
  assert(!isPublicShareUrl("https://192.168.1.2/stream"));
  assert(!isPublicShareUrl("https://example.com/stream?token=secret"));
  assert(!isPublicShareUrl("https://user:pass@example.com/mp3"));
  assert(!isPublicShareUrl("https://example.com/api/session/abc"));
  assert(!isPublicShareUrl("https://example.com/a%2fb"));
  assert(!isPublicShareUrl("https://example.com/jazz#frag"));
  assert(!isPublicShareUrl(nullptr));
  const char *base = "https://example.com/";
  char maxUrl[100] = {};
  std::strcpy(maxUrl, base);
  std::memset(maxUrl + std::strlen(base), 'x', 84 - std::strlen(base));
  maxUrl[84] = '\0';
  assert(isPublicShareUrl(maxUrl));
  maxUrl[84] = 'x'; maxUrl[85] = '\0';
  assert(!isPublicShareUrl(maxUrl));
  return 0;
}
