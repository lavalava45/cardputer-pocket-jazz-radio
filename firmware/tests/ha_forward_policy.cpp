#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/HomeAssistantBridge.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdint>
using namespace pocketjazz::ha;

int main() {
  HomeAssistantBridge bridge;
  char out[kPayloadMax + 1] = {};
  VerifiedSession trusted;
  trusted.tlsPeerVerified = true;
  trusted.hostnameVerified = true;
  trusted.clientAuthenticated = true;
  trusted.commandAclVerified = true;
  const char* names[] = {"living_room", "bedroom"};
  const char* domains[] = {"radio.example.org"};
  const char* good = "https://radio.example.org/live.mp3?quality=high&mode=1";
  assert(!bridge.encodeForward("living_room", good, 10000, 1794280000ULL, trusted, out, sizeof(out)));
  assert(bridge.configure("cardputer_1", true));
  assert(std::strcmp(bridge.forwardTopic(), "pocketjazz/cardputer_1/forward") == 0);
  assert(!bridge.encodeForward("living_room", good, 10000, 1794280000ULL, trusted, out, sizeof(out)));
  assert(bridge.configureRecipients(names, 2));
  assert(!bridge.encodeForward("living_room", good, 10000, 1794280000ULL, trusted, out, sizeof(out)));
  assert(bridge.configureStationHosts(domains, 1));
  assert(!bridge.encodeForward("media_player.everywhere", good, 10000, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", good, 10000, 1794280000ULL, VerifiedSession(), out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", good, 10000, 0, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", good, 10000, 4102444801ULL, trusted, out, sizeof(out)));
  assert(bridge.encodeForward("living_room", good, 10000, 1794280000ULL, trusted, out, sizeof(out)));
  assert(std::strcmp(out, "{\"recipient\":\"living_room\",\"url\":\"https://radio.example.org/live.mp3?quality=high&mode=1\",\"issued_at\":1794280000}") == 0);
  assert(!bridge.encodeForward("bedroom", good, 19999, 1794280000ULL, trusted, out, sizeof(out)));
  assert(bridge.encodeForward("bedroom", good, 20000, 1794280000ULL, trusted, out, sizeof(out)));
  bridge.resetForwardThrottle();
  assert(!bridge.encodeForward("living_room", "http://radio.example.org/mp3", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://user:pw@radio.example.org/mp3", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://radio.example.org:444/mp3", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://radio.example.org/\r\nmal", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://radio.example.org/\"evil", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://radio.example.org/\\bad", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://radio.example.org/mp3#fragment", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://a..example.org/mp3", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://-a.example.org/mp3", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://localhost/mp3", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!validForwardUrl("https://127.0.0.1/live.mp3"));
  assert(!bridge.encodeForward("living_room", "https://evil.example.org/live.mp3", 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", "https://127.0.0.1/mp3", 0, 1794280000ULL, trusted, out, 15));
  std::string huge = "https://radio.example.org/" + std::string(kForwardUrlMax, 'a');
  assert(!bridge.encodeForward("living_room", huge.c_str(), 0, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", good, 0, 1794280000ULL, trusted, out, 10));
  assert(bridge.encodeForward("living_room", good, UINT32_MAX - 3000u, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.encodeForward("living_room", good, 1000, 1794280000ULL, trusted, out, sizeof(out)));
  assert(bridge.encodeForward("living_room", good, 7000, 1794280000ULL, trusted, out, sizeof(out)));

  const char* duplicates[] = {"living_room", "living_room"};
  assert(!bridge.configureRecipients(duplicates, 2));
  assert(!bridge.encodeForward("living_room", good, 30000, 1794280000ULL, trusted, out, sizeof(out)));
  assert(!bridge.configureRecipients(names, 5));
  assert(!bridge.configureRecipients(nullptr, 2));
  assert(bridge.configureRecipients(names, 2));
  assert(bridge.configureStationHosts(domains, 1));
  assert(bridge.configure("cardputer_1", false));
  assert(!bridge.encodeForward("living_room", good, 30000, 1794280000ULL, trusted, out, sizeof(out)));

  NoticeRatePolicy rate;
  NoticePolicy action;
  assert(rate.decide(0, false, action));
  assert(action.display && action.duckPercent == 30 && action.durationMs == 1000);
  assert(!rate.decide(14999, false, action));
  assert(!action.display && action.duckPercent == 0 && action.durationMs == 0);
  assert(rate.decide(15000, true, action));
  assert(action.display && action.duckPercent == 0 && action.durationMs == 0);
  assert(!rate.decide(16000, false, action));
  rate.clear();
  assert(rate.decide(UINT32_MAX - 2000u, false, action));
  assert(!rate.decide(500, false, action));
  assert(rate.decide(13000, true, action));
  return 0;
}
