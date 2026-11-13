#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/HomeAssistantBridge.h"
#include <cassert>
#include <cstring>
#include <string>

using namespace pocketjazz::ha;

static bool decode(HomeAssistantBridge& bridge, const char* payload,
                   bool retained, unsigned qos, const VerifiedSession& auth,
                   Command& command) {
  return bridge.decodeCommand(bridge.commandTopic(),
      reinterpret_cast<const uint8_t*>(payload), std::strlen(payload),
      retained, static_cast<uint8_t>(qos), auth, command);
}

int main() {
  HomeAssistantBridge bridge;
  VerifiedSession auth;
  Command command;
  assert(!bridge.enabled());
  assert(!bridge.configure("../admin", true));
  assert(!bridge.configure("UPPER", true));
  assert(!bridge.configure("", true));
  assert(!bridge.configure("abcdefghijklmnopqrstuvwxy", true));
  assert(bridge.configure("cardputer_1", false));
  assert(!decode(bridge, "play", false, 0, auth, command));
  assert(bridge.configure("cardputer_1", true));
  assert(std::strcmp(bridge.commandTopic(), "pocketjazz/cardputer_1/cmd") == 0);
  assert(std::strcmp(bridge.notifyTopic(), "pocketjazz/cardputer_1/notify") == 0);
  assert(!decode(bridge, "play", false, 0, auth, command));
  auth.tlsPeerVerified = auth.hostnameVerified = auth.clientAuthenticated = true;
  assert(!decode(bridge, "play", false, 0, auth, command));
  auth.commandAclVerified = true;
  assert(decode(bridge, "play", false, 0, auth, command));
  assert(command.kind == kPlay);
  assert(decode(bridge, "pause", false, 0, auth, command) && command.kind == kPause);
  assert(decode(bridge, "stop", false, 0, auth, command) && command.kind == kStop);
  assert(decode(bridge, "next", false, 0, auth, command) && command.kind == kNext);
  assert(decode(bridge, "previous", false, 0, auth, command) && command.kind == kPrevious);
  assert(decode(bridge, "volume:0", false, 0, auth, command) && command.volume == 0);
  assert(decode(bridge, "volume:100", false, 0, auth, command) && command.volume == 100);
  assert(!decode(bridge, "play", true, 0, auth, command));
  assert(command.kind == kNone);
  assert(!decode(bridge, "play", false, 1, auth, command));
  assert(!decode(bridge, "play", false, 2, auth, command));
  assert(!decode(bridge, "volume:101", false, 0, auth, command));
  assert(!decode(bridge, "volume:01", false, 0, auth, command));
  assert(!decode(bridge, "volume:-1", false, 0, auth, command));
  assert(!decode(bridge, "volume:1x", false, 0, auth, command));
  assert(!decode(bridge, "PLAY", false, 0, auth, command));
  assert(!decode(bridge, "reboot", false, 0, auth, command));
  assert(!decode(bridge, "{\"command\":\"play\"}", false, 0, auth, command));
  assert(!decode(bridge, "play\n", false, 0, auth, command));
  assert(!decode(bridge, "play", false, 0, VerifiedSession(), command));
  const uint8_t badUtf8[] = {0xc0, 0xaf};
  assert(!bridge.decodeCommand(bridge.commandTopic(), badUtf8, 2, false, 0, auth, command));
  const uint8_t surrogate[] = {0xed, 0xa0, 0x80};
  assert(!bridge.decodeCommand(bridge.commandTopic(), surrogate, 3, false, 0, auth, command));
  const uint8_t truncated[] = {0xe2, 0x82};
  assert(!bridge.decodeCommand(bridge.commandTopic(), truncated, 2, false, 0, auth, command));
  const uint8_t oversize[33] = {};
  assert(!bridge.decodeCommand(bridge.commandTopic(), oversize, sizeof(oversize), false, 0, auth, command));
  assert(!bridge.decodeCommand("pocketjazz/other/cmd", reinterpret_cast<const uint8_t*>("play"), 4, false, 0, auth, command));
  assert(!bridge.decodeCommand(bridge.commandTopic(), nullptr, 4, false, 0, auth, command));

  char state[kPayloadMax + 1] = {};
  assert(bridge.encodeState("playing", 42, "A \"quoted\" \\ title", state, sizeof(state)));
  assert(std::strcmp(state, "{\"state\":\"playing\",\"volume\":42,\"title\":\"A \\\"quoted\\\" \\\\ title\"}") == 0);
  assert(!bridge.encodeState("loading", 42, "title", state, sizeof(state)));
  assert(!bridge.encodeState("playing", 101, "title", state, sizeof(state)));
  assert(!bridge.encodeState("playing", 42, "title", state, 8));
  const char badTitle[] = {char(0xf0), char(0x80), char(0x80), char(0x80), 0};
  assert(!bridge.encodeState("playing", 42, badTitle, state, sizeof(state)));
  std::string longTitle(97, 'a');
  assert(!bridge.encodeState("playing", 42, longTitle.c_str(), state, sizeof(state)));

  const uint8_t notice[] = {'j', 'a', 'z', 'z'};
  assert(!bridge.receiveNotice(bridge.notifyTopic(), notice, sizeof(notice), false, 0, VerifiedSession()));
  assert(!bridge.receiveNotice(bridge.notifyTopic(), notice, sizeof(notice), true, 0, auth));
  assert(!bridge.receiveNotice(bridge.notifyTopic(), notice, sizeof(notice), false, 1, auth));
  assert(!bridge.receiveNotice(bridge.commandTopic(), notice, sizeof(notice), false, 0, auth));
  assert(!bridge.receiveNotice(bridge.notifyTopic(), badUtf8, sizeof(badUtf8), false, 0, auth));
  for (int i = 0; i < kQueueCapacity; ++i)
    assert(bridge.receiveNotice(bridge.notifyTopic(), notice, sizeof(notice), false, 0, auth));
  assert(!bridge.enqueueNotice(notice, sizeof(notice)));
  assert(!bridge.enqueueNotice(badUtf8, sizeof(badUtf8)));
  const uint8_t huge[kNoticeMax + 1] = {};
  assert(!bridge.enqueueNotice(huge, sizeof(huge)));
  assert(bridge.pendingNotices() == kQueueCapacity);
  char tiny[4];
  assert(!bridge.popNotice(tiny, sizeof(tiny)));
  assert(bridge.pendingNotices() == kQueueCapacity);
  char message[kNoticeMax + 1];
  for (int i = 0; i < kQueueCapacity; ++i) {
    assert(bridge.popNotice(message, sizeof(message)));
    assert(std::strcmp(message, "jazz") == 0);
  }
  assert(!bridge.popNotice(message, sizeof(message)));
  assert(bridge.enqueueNotice(notice, sizeof(notice)));
  assert(bridge.configure("cardputer_2", false));
  assert(bridge.pendingNotices() == 0);
  return 0;
}
