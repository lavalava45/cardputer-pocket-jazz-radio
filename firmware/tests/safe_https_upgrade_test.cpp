#ifdef NDEBUG
#undef NDEBUG // MSVC Release must execute negative-case assertions.
#endif
#include "../PocketJazzRadio/SafeHttpsUpgrade.h"
#include <cassert>
#include <cstring>
#include <string>

// Mirror the exact authority/path split performed in Arduino-ESP32 3.3.9
// HTTPClient::beginInternal() (indexOf('/') after removing "https://").
// This is deliberately an integration contract, not a generic URL parser.
static void expectSdk339HostAndUri(const char* normalized,
                                   const char* host, const char* uri) {
  assert(normalized && !std::strncmp(normalized, "https://", 8));
  const std::string afterScheme(normalized + 8);
  const size_t slash = afterScheme.find('/');
  assert(slash != std::string::npos);
  assert(afterScheme.substr(0, slash) == host);
  assert(afterScheme.substr(slash) == uri);
}

static bool allowed(const char* candidate, const char* expected = nullptr) {
  char out[192] = "uninitialized";
  const bool ok = pocketjazz::copySafeHttpsUpgrade(candidate, out, sizeof(out));
  if (ok) assert(expected && !std::strcmp(out, expected));
  else assert(!out[0]);
  return ok;
}

int main() {
  const char* swiss = "https://stream.srg-ssr.ch/srgssr/rsj/mp3/128";
  assert(allowed(swiss, swiss));
  const char* query = "https://audio.example.org:8443/live?token=value&x=1";
  assert(allowed(query, query));
  const char* root = "https://radio.example.com";
  assert(allowed(root, root));
  assert(allowed("https://radio.example.org?stream=1",
                 "https://radio.example.org/?stream=1"));
  expectSdk339HostAndUri("https://radio.example.org/?stream=1",
                         "radio.example.org", "/?stream=1");
  assert(allowed("https://radio.example.org:8443?stream=1&token=a%2Fb",
                 "https://radio.example.org:8443/?stream=1&token=a%2Fb"));
  expectSdk339HostAndUri("https://radio.example.org:8443/?stream=1&token=a%2Fb",
                         "radio.example.org:8443", "/?stream=1&token=a%2Fb");
  assert(allowed("https://radio.example.org?", "https://radio.example.org/?"));
  assert(allowed("https://radio.example.org/?stream=1",
                 "https://radio.example.org/?stream=1"));
  // Expansion must not overrun the 192-byte AudioCommand/upgrade URL.
  std::string edge = "https://radio.example.org?" + std::string(165, 'q');
  assert(edge.size() == 191);
  assert(!allowed(edge.c_str()));
  edge.pop_back();
  assert(allowed(edge.c_str(), ("https://radio.example.org/?" +
                                    std::string(164, 'q')).c_str()));
  const char* invalid[] = {
      nullptr, "", "http://station.example/live", "//station.example/live",
      "ftp://station.example/live", "https:///no-host", "https://",
      "https://user:pass@station.example/live", "https://station.example:0/live",
      "https://station.example:65536/live", "https://station.example:123invalid/live",
      "https://station.example:/live", "https://station.example:80:443/live",
      "https://station.example/live#fragment", "https://station.example\\attack/live",
      "https://station.example/hi\r\nHost: attacker", "https://bad host/live",
      "https://[2001:db8::1]/live", "HTTPS://station.example/live",
      "https://radio.example.org?x=1#fragment",
      "https://radio.example.org?x=1\r\nHost:attacker",
  };
  for (auto candidate : invalid) assert(!allowed(candidate));
  // At most 191 URL bytes may be accepted for the existing 192-B command
  // buffer. A truncated station address is NOT a valid fallback.
  std::string longUrl = std::string("https://radio.example/") + std::string(192, 'x');
  assert(!allowed(longUrl.c_str()));
  char tooSmall[9];
  assert(!pocketjazz::copySafeHttpsUpgrade(swiss, tooSmall, sizeof(tooSmall)) &&
         !tooSmall[0]);
  assert(!pocketjazz::copySafeHttpsUpgrade(swiss, nullptr, 0));
  return 0;
}
