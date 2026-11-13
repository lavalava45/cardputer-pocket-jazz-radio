#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../experiments/StationExchangePacket.h"
#include <cassert>
#include <cstring>
#include <type_traits>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// A guard page verifies that validAscii NEVER reads byte maxLength+1.
// ASan alone may not catch an intra-object read from name into adjacent url.
static void testNoOutOfBoundsRead(size_t length) {
#if defined(_WIN32)
  SYSTEM_INFO sys{};
  GetSystemInfo(&sys);
  const size_t page = sys.dwPageSize;
  assert(length < page);
  uint8_t* memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 2,
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  assert(memory);
  DWORD previous = 0;
  assert(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous));
  char* unterminated = reinterpret_cast<char*>(memory + page - length);
  std::memset(unterminated, 'x', length);
  assert(!pocketjazz::exchange::validAscii(unterminated, length - 1));
  assert(VirtualFree(memory, 0, MEM_RELEASE));
#else
  (void)length;
#endif
}

int main() {
  using namespace pocketjazz::exchange;
  Packet item{};
  testNoOutOfBoundsRead(kMaxName + 1);
  testNoOutOfBoundsRead(kMaxUrl + 1);
  item.type = Offer;
  item.nonce = 0x31415926u;
  std::strcpy(item.name, "Jazz24");
  std::strcpy(item.url, "https://radio.example.org/stream.mp3");
  uint8_t buffer[kMaxPacketBytes]{};
  size_t used = 0;
  assert(encode(item, buffer, sizeof(buffer), used));
  assert(used < kMaxPacketBytes);
  Packet received;
  assert(decode(buffer, used, received));
  assert(received.type == Offer && received.nonce == item.nonce);
  assert(std::strcmp(received.name, "Jazz24") == 0);
  assert(std::strcmp(received.url, item.url) == 0);
  assert(!decode(buffer, used - 1, received));
  buffer[8] ^= 0x20;
  assert(!decode(buffer, used, received));
  buffer[8] ^= 0x20;
  assert(!decode(buffer, kMaxPacketBytes + 1, received));
  assert(!encode(item, buffer, used - 1, used));
  std::strcpy(item.url, "http://192.168.0.1/private.mp3");
  assert(!encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.url, "https://example.org/stream?token=password");
  assert(!encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.url, "https://name:pass@example.org/stream");
  assert(!encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.url, "https://example.org/stream#frag");
  assert(!encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.url, "https://example.org/stream");
  item.name[0] = '\n';
  assert(!encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.name, "Good jazz");
  assert(encode(item, buffer, sizeof(buffer), used));

  // Regression: the adversarial field ends without NUL. Address sanitizers
  // must never observe a read of Packet.name[33] or Packet.url[192].
  static_assert(sizeof(item.name) == kMaxName + 1, "name field contract");
  static_assert(sizeof(item.url) == kMaxUrl + 1, "URL field contract");
  std::memset(item.name, 'N', sizeof(item.name));
  assert(!encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.name, "Jazz24");
  std::memset(item.url, 'U', sizeof(item.url));
  assert(!encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.url, "https://radio.example.org/stream.mp3");
  std::memset(item.name, 'X', kMaxName);
  item.name[kMaxName] = '\0';
  assert(encode(item, buffer, sizeof(buffer), used));
  std::strcpy(item.name, "Good jazz");
  const size_t original = used;
  buffer[kHeaderBytes] = '\n';
  write32(buffer + original - 4, pocketjazz::logo::crc32(buffer, original - 4));
  assert(!decode(buffer, original, received)); // even valid CRC can't inject controls

  Packet ack{};
  ack.type = Acknowledge;
  ack.nonce = item.nonce;
  assert(encode(ack, buffer, sizeof(buffer), used));
  assert(used == kHeaderBytes + kCrcBytes);
  assert(decode(buffer, used, received));
  assert(received.type == Acknowledge && received.nonce == item.nonce);
  assert(!received.name[0] && !received.url[0]);
  return 0;
}
