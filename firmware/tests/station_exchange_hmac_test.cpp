#ifdef NDEBUG
#undef NDEBUG // MSVC Release assertions MUST remain active
#endif
#include "../experiments/StationExchangeHmac.h"
#include <cassert>
#include <cstring>
#include <cstdint>
#include <type_traits>

using namespace pocketjazz::exchange;
using namespace pocketjazz::exchange::secure;

int main() {
  const PeerId sender{{0x02,0x11,0x22,0x33,0x44,0x55}};
  const PeerId receiver{{0x04,0x11,0x22,0x33,0x44,0x66}};
  const PeerId stranger{{0x06,0x12,0x22,0x33,0x44,0x66}};
  uint8_t key[32] = {};
  for (size_t i = 0; i < sizeof(key); ++i) key[i] = static_cast<uint8_t>(i);
  uint8_t allZeros[32] = {};
  PeerHmacSha256 a, b;
  static_assert(!std::is_copy_constructible<BoundPeerExchange>::value,
                "Bound peer context must never copy dangling MAC callbacks");
  static_assert(!std::is_move_constructible<BoundPeerExchange>::value,
                "Bound peer context must never move dangling MAC callbacks");
  assert(!a.ready());
  assert(!a.load(receiver, nullptr, sizeof(key)));
  assert(!a.load(receiver, key, sizeof(key) - 1));
  assert(!a.load(receiver, allZeros, sizeof(allZeros)));
  assert(!a.load(PeerId{{0,0,0,0,0,0}}, key, sizeof(key)));
  assert(a.load(receiver, key, sizeof(key)));
  assert(b.load(sender, key, sizeof(key)));
  const char message[] = "Cardputer ESP-NOW station exchange test vector";
  const uint8_t known128[] = {0x23,0xaf,0xcc,0x3c,0x71,0xd7,0x3e,0x92,
                              0xc9,0x37,0x62,0xeb,0xda,0x12,0xef,0xd6};
  uint8_t tag[16] = {};
  assert(a.signTag(reinterpret_cast<const uint8_t*>(message),sizeof(message)-1,tag));
  assert(std::memcmp(tag, known128, sizeof(tag)) == 0); // independent HMAC-SHA256 KAT
  assert(b.verifyTag(reinterpret_cast<const uint8_t*>(message), sizeof(message)-1,
                     known128));
  uint8_t mutated[16] = {};
  std::memcpy(mutated, known128, 16);
  for (size_t i = 0; i < sizeof(mutated); ++i) {
    mutated[i] ^= 1;
    assert(!b.verifyTag(reinterpret_cast<const uint8_t*>(message), sizeof(message)-1,
                        mutated));
    mutated[i] ^= 1;
  }
  BoundPeerExchange senderSession, receiverSession, rogueSession;
  assert(!senderSession.paired());
  assert(senderSession.proposePair(sender, receiver, 0x1234ULL, 100, key, sizeof(key)));
  assert(receiverSession.proposePair(receiver, sender, 0x1234ULL, 100, key, sizeof(key)));
  assert(rogueSession.proposePair(receiver, stranger, 0x1234ULL, 100, key, sizeof(key)));
  assert(senderSession.confirmPair(110, true));
  assert(receiverSession.confirmPair(110, true));
  assert(rogueSession.confirmPair(110, true));
  BoundPeerExchange denied, expired;
  assert(denied.proposePair(sender, receiver, 9, 100, key, sizeof(key)));
  assert(!denied.confirmPair(110, false));
  assert(!denied.paired() && !denied.confirmPair(115, true));
  assert(expired.proposePair(sender, receiver, 10, 100, key, sizeof(key)));
  expired.expire(100 + StationExchangeSecure::PairTimeoutMs + 1);
  assert(!expired.confirmPair(110, true));
  uint8_t wire[kMaxPacketBytes] = {};
  size_t used = 0;
  const char* url = "https://radio.example.org/live.mp3";
  assert(senderSession.offer("Jazz", 4, url, std::strlen(url), 120,
                             wire, sizeof(wire), used));
  Packet received{};
  assert(rogueSession.receive(wire, used, 130, received) == StationExchangeSecure::Rejected);
  assert(receiverSession.receive(wire, used, 130, received) == StationExchangeSecure::PresentOffer);
  assert(received.nonce == 1 && std::strcmp(received.url, url) == 0);
  wire[26] ^= 0x01;
  assert(receiverSession.receive(wire, used, 131, received) == StationExchangeSecure::Rejected);
  wire[26] ^= 0x01;
  assert(receiverSession.decideIncoming(true, 140));
  uint8_t ack[kMaxPacketBytes] = {};
  size_t ackUsed = 0;
  assert(receiverSession.makeAck(ack, sizeof(ack), ackUsed));
  assert(senderSession.receive(ack, ackUsed, 150, received) ==
         StationExchangeSecure::DeliveryConfirmed);
  receiverSession.reset();
  assert(!receiverSession.paired());
  assert(receiverSession.receive(wire, used, 160, received) == StationExchangeSecure::Rejected);
  // Key provider and protocol cannot outlive each other: both stored as one
  // nonmovable owner, so a shorter-lived local cannot leave a dangling MAC.
  { BoundPeerExchange temporary;
    assert(temporary.proposePair(sender, receiver, 0x1235ULL, 200, key, sizeof(key)));
    assert(temporary.confirmPair(210, true));
  }
  b.clear();
  assert(!b.ready() && !b.verifyTag(reinterpret_cast<const uint8_t*>(message),
                                  sizeof(message)-1, known128));
  a.clear();
  assert(!a.ready());
  return 0;
}
