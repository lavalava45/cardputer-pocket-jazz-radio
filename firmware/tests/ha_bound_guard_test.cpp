#ifdef NDEBUG
#undef NDEBUG // MSVC Release must execute these assertions
#endif
#include "../PocketJazzRadio/HomeAssistantBridge.h"
#include <cassert>
#include <cstring>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace pocketjazz::ha;

#ifdef _WIN32
struct GuardedField {
  char* value;
  uint8_t* base;
  size_t size;
  GuardedField(size_t bytes) : value(nullptr), base(nullptr), size(0) {
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    size = info.dwPageSize;
    assert(bytes && bytes <= size);
    base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 2 * size,
                                               MEM_COMMIT | MEM_RESERVE,
                                               PAGE_READWRITE));
    assert(base);
    DWORD old = 0;
    assert(VirtualProtect(base + size, size, PAGE_NOACCESS, &old));
    value = reinterpret_cast<char*>(base + size - bytes);
    std::memset(value, 'X', bytes);
  }
  ~GuardedField() { if (base) assert(VirtualFree(base, 0, MEM_RELEASE)); }
  GuardedField(const GuardedField&) = delete;
  GuardedField& operator=(const GuardedField&) = delete;
};
#endif

int main() {
  size_t length = 1;
  assert(!boundedNul(nullptr, 4, length) && length == 0);
  assert(!boundedNul("", 0, length));
  assert(!validDeviceId(nullptr) && !validDeviceId(""));
  assert(!validForwardUrl(nullptr));

#ifdef _WIN32
  // The very next byte would cross a PAGE_NOACCESS boundary. A single
  // accidental s[maxLength] read terminates the process by access violation.
  GuardedField badUrl(kForwardUrlMax);
  assert(!validForwardUrl(badUrl.value));
  std::memcpy(badUrl.value, "https://radio.example.org/", 26);
  badUrl.value[kForwardUrlMax - 1] = 0;
  assert(validForwardUrl(badUrl.value));

  GuardedField badId(24);
  std::memset(badId.value, 'a', 24);
  assert(!validDeviceId(badId.value));
  badId.value[23] = 0;
  assert(validDeviceId(badId.value));

  HomeAssistantBridge bridge;
  assert(bridge.configure("cardputer_1", true));
  const char* hosts[] = {nullptr};
  GuardedField badHost(kForwardHostMax);
  hosts[0] = badHost.value;
  assert(!bridge.configureStationHosts(hosts, 1));

  GuardedField badRecipient(kRecipientMax);
  const char* recipients[] = {"living_room"};
  const char* domains[] = {"radio.example.org"};
  assert(bridge.configureRecipients(recipients, 1));
  assert(bridge.configureStationHosts(domains, 1));
  VerifiedSession verified;
  verified.clientAuthenticated = verified.commandAclVerified = true;
  verified.tlsPeerVerified = verified.hostnameVerified = true;
  char json[kPayloadMax + 1] = {};
  assert(!bridge.encodeForward(badRecipient.value,
                               "https://radio.example.org/live", 100,
                               1794280000ULL, verified, json, sizeof(json)));

  GuardedField badTitle(kTitleMax);
  std::memset(badTitle.value, 'A', kTitleMax);
  assert(!bridge.encodeState("playing", 50, badTitle.value, json, sizeof(json)));
  badTitle.value[kTitleMax - 1] = 0;
  assert(bridge.encodeState("playing", 50, badTitle.value, json, sizeof(json)));
#endif
  return 0;
}
