// Host-only negative tests. The fake tag below is DELIBERATELY NOT A MAC:
// production must inject an audited constant-time HMAC/AEAD verifier.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../experiments/StationExchangeSecure.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace pocketjazz::exchange;
using namespace pocketjazz::exchange::secure;
static_assert(kMaxPacketBytes == 250, "ESP-NOW legacy payload cap");
static_assert(kEnvelopeOverhead == 42, "secure envelope overhead");

struct FakeKeys {
  uint8_t seed;
  bool failSign;
  bool failVerify;
};
static void fakeDigest(const uint8_t* bytes, size_t length, uint8_t key,
                       uint8_t out[kTagBytes]) {
  // Deterministic fixture only, absolutely NOT cryptographically secure.
  for (size_t i = 0; i < kTagBytes; ++i) out[i] = uint8_t(key ^ (i * 17));
  for (size_t i = 0; i < length; ++i) {
    out[i % kTagBytes] = uint8_t(out[i % kTagBytes] * 33u + bytes[i]);
    out[(i + 5) % kTagBytes] ^= uint8_t(bytes[i] + (i & 255));
  }
}
static bool signFake(void* ctx, const PeerId&, const uint8_t* message,
                     size_t length, uint8_t tag[kTagBytes]) {
  FakeKeys* keys = static_cast<FakeKeys*>(ctx);
  if (keys->failSign) return false;
  fakeDigest(message, length, keys->seed, tag);
  return true;
}
static bool verifyFake(void* ctx, const PeerId&, const uint8_t* message,
                       size_t length, const uint8_t tag[kTagBytes]) {
  FakeKeys* keys = static_cast<FakeKeys*>(ctx);
  if (keys->failVerify) return false;
  uint8_t expected[kTagBytes];
  fakeDigest(message, length, keys->seed, expected);
  return sameBytes(expected, tag, sizeof(expected));
}
static Authenticator auth(FakeKeys& keys) {
  Authenticator result = {&keys, signFake, verifyFake};
  return result;
}
static const PeerId Alice = {{0x02, 0x11, 0x22, 0x33, 0x44, 0x55}};
static const PeerId Bob = {{0x04, 0x10, 0x20, 0x30, 0x40, 0x50}};
static const PeerId Eve = {{0x06, 0x66, 0x66, 0x66, 0x66, 0x66}};
static const char* Url = "https://radio.example.org/jazz.mp3";
static const uint64_t Session = UINT64_C(0x91e23469aabbccd1);
static void paired(StationExchangeSecure& node, const PeerId& self,
                   const PeerId& remote, FakeKeys& keys,
                   uint64_t epoch = Session) {
  assert(node.proposePair(self, remote, epoch, 100, auth(keys)));
  assert(!node.paired());
  assert(!node.confirmPair(101, false));  // no implicit pairing
  assert(node.confirmPair(102, true));
  assert(node.paired());
}
static size_t makeOffer(StationExchangeSecure& sender, uint8_t* wire,
                        uint32_t now = 200) {
  size_t used = 0;
  assert(sender.offer("Jazz24", 6, Url, std::strlen(Url), now,
                      wire, kMaxPacketBytes, used));
  assert(used > kEnvelopeOverhead && used <= kMaxPacketBytes);
  return used;
}
static void noUnexpectedOffer(StationExchangeSecure& receiver,
                              const uint8_t* wire, size_t len, uint32_t now) {
  Packet response{};
  assert(receiver.receive(wire, len, now, response) !=
         StationExchangeSecure::PresentOffer);
}
int main() {
  FakeKeys shared = {0x9d, false, false};
  StationExchangeSecure sender, receiver;
  uint8_t wire[kMaxPacketBytes] = {};
  size_t used = 123;
  assert(!sender.offer("Jazz24", 6, Url, std::strlen(Url), 200,
                       wire, sizeof(wire), used) && used == 0);
  assert(!sender.proposePair(Alice, Alice, Session, 10, auth(shared)));
  assert(!sender.proposePair(Alice, Bob, 0, 10, auth(shared)));
  assert(!sender.proposePair(Alice, Bob, Session, 10, Authenticator{}));
  const PeerId multicast = {{0xff,0xff,0xff,0xff,0xff,0xff}};
  assert(!sender.proposePair(multicast, Bob, Session, 10, auth(shared)));
  assert(sender.proposePair(Alice, Bob, Session, 100, auth(shared)));
  assert(!sender.confirmPair(30101, true)); // pairing window expired
  assert(!sender.paired());
  paired(sender, Alice, Bob, shared);
  paired(receiver, Bob, Alice, shared);

  // Input lengths must be bounded; no reads beyond caller-supplied lengths.
  assert(!sender.offer("X", 0, Url, std::strlen(Url), 200,
                       wire, sizeof(wire), used));
  assert(!sender.offer("X", 33, Url, std::strlen(Url), 200,
                       wire, sizeof(wire), used));
  const char hiddenNul[] = {'A', 0, 'B'};
  assert(!sender.offer(hiddenNul, sizeof(hiddenNul), Url,
                       std::strlen(Url), 200, wire, sizeof(wire), used));
  assert(!sender.offer("X", 1, "https://host.net/?token=abc", 27, 200,
                       wire, sizeof(wire), used));
  assert(!sender.offer("X", 1, "http://192.168.0.1/radio", 24, 200,
                       wire, sizeof(wire), used));
  assert(!sender.offer("X", 1, "https://user:pass@example.net/x",
                       std::strlen("https://user:pass@example.net/x"),
                       200, wire, sizeof(wire), used));
  char oversized[192];
  std::memset(oversized, 'a', sizeof(oversized));
  assert(!sender.offer("Jazz24", 6, oversized, sizeof(oversized), 200,
                       wire, sizeof(wire), used));

  // A valid packet is signed over the complete envelope, including source,
  // destination, unique session and the CRC-protected PJST payload.
  const size_t offerLength = makeOffer(sender, wire);
  Packet received{};
  assert(receiver.receive(wire, offerLength, 201, received) ==
         StationExchangeSecure::PresentOffer);
  assert(received.nonce == 1 && std::strcmp(received.name, "Jazz24") == 0);
  assert(std::strcmp(received.url, Url) == 0);
  assert(receiver.receive(wire, offerLength, 202, received) ==
         StationExchangeSecure::Rejected); // still needs human decision
  assert(receiver.decideIncoming(true, 203));
  assert(!receiver.decideIncoming(true, 204));
  uint8_t ack[kMaxPacketBytes] = {};
  size_t ackLen = 0;
  assert(receiver.makeAck(ack, sizeof(ack), ackLen));
  assert(sender.receive(ack, ackLen, 205, received) ==
         StationExchangeSecure::DeliveryConfirmed);
  assert(!sender.pending());
  assert(sender.receive(ack, ackLen, 206, received) ==
         StationExchangeSecure::NoAction);
  assert(receiver.receive(wire, offerLength, 207, received) ==
         StationExchangeSecure::AckReady); // duplicate, NEVER second UI
  uint8_t duplicateAck[kMaxPacketBytes] = {};
  size_t duplicateAckLen = 0;
  assert(receiver.makeAck(duplicateAck, sizeof(duplicateAck),
                          duplicateAckLen));
  assert(duplicateAckLen == ackLen &&
         sameBytes(duplicateAck, ack, ackLen));
  // Limit authenticated duplicate ACK amplification to four responses
  // maximum, including the original acknowledgement.
  for (unsigned i = 2; i < StationExchangeSecure::MaxAckTransmissions; ++i) {
    assert(receiver.receive(wire, offerLength, 210 + i, received) ==
           StationExchangeSecure::AckReady);
    assert(receiver.makeAck(duplicateAck, sizeof(duplicateAck),
                            duplicateAckLen));
  }
  assert(!receiver.makeAck(duplicateAck, sizeof(duplicateAck),
                           duplicateAckLen));
  assert(receiver.receive(wire, offerLength, 220, received) ==
         StationExchangeSecure::Rejected);

  // Tamper any header, sender/receiver ID, epoch, body, tag or CRC.
  for (size_t i = 0; i < offerLength; ++i) {
    uint8_t bad[kMaxPacketBytes];
    std::memcpy(bad, wire, offerLength);
    bad[i] ^= 1;
    assert(receiver.receive(bad, offerLength, 250, received) ==
           StationExchangeSecure::Rejected);
  }
  assert(receiver.receive(wire, offerLength - 1, 250, received) ==
         StationExchangeSecure::Rejected);
  assert(receiver.receive(wire, kMaxPacketBytes + 1, 250, received) ==
         StationExchangeSecure::Rejected);
  assert(receiver.receive(nullptr, 0, 250, received) ==
         StationExchangeSecure::Rejected);
  FakeKeys forged = {0x44, false, false};
  StationExchangeSecure wrongKey;
  paired(wrongKey, Bob, Alice, forged);
  assert(wrongKey.receive(wire, offerLength, 251, received) ==
         StationExchangeSecure::Rejected);
  StationExchangeSecure wrongSession;
  paired(wrongSession, Bob, Alice, shared, Session + 1);
  assert(wrongSession.receive(wire, offerLength, 252, received) ==
         StationExchangeSecure::Rejected);
  StationExchangeSecure wrongPeer;
  paired(wrongPeer, Bob, Eve, shared);
  assert(wrongPeer.receive(wire, offerLength, 252, received) ==
         StationExchangeSecure::Rejected);
  shared.failVerify = true;
  assert(receiver.receive(wire, offerLength, 253, received) ==
         StationExchangeSecure::Rejected);
  shared.failVerify = false;

  // Bounded retry schedule: first send + exactly three identical retries.
  const size_t second = makeOffer(sender, wire, 1000);
  assert(sender.pending());
  assert(!sender.offer("X", 1, Url, std::strlen(Url), 1010,
                       ack, sizeof(ack), used));
  assert(!sender.retry(1699, ack, sizeof(ack), used));
  assert(!sender.retry(1700, ack, second - 1, used)); // no truncated frame
  for (unsigned i = 0; i < StationExchangeSecure::MaxRetries; ++i) {
    assert(sender.retry(1700 + i * 700, ack, sizeof(ack), used));
    assert(used == second && sameBytes(ack, wire, second));
  }
  assert(!sender.retry(3900, ack, sizeof(ack), used)); // cap reached
  assert(sender.pending());
  sender.expire(6001);
  assert(!sender.pending());
  assert(sender.receive(ack, ackLen, 6002, received) !=
         StationExchangeSecure::DeliveryConfirmed); // expired ack

  // Human refusal consumes the nonce; replay cannot reopen an offer.
  assert(receiver.receive(wire, second, 1100, received) ==
         StationExchangeSecure::PresentOffer);
  assert(receiver.decideIncoming(false, 1101));
  assert(!receiver.makeAck(ack, sizeof(ack), used));
  assert(receiver.receive(wire, second, 1110, received) ==
         StationExchangeSecure::Rejected);

  // Human decision window expires; late acknowledgement is forbidden and a
  // replayed Offer with the same nonce cannot reopen the UI prompt.
  StationExchangeSecure delayed;
  paired(delayed, Bob, Alice, shared);
  assert(delayed.receive(wire, second, 1200, received) ==
         StationExchangeSecure::PresentOffer);
  assert(!delayed.decideIncoming(true,
      1200 + StationExchangeSecure::OfferDecisionMs + 1));
  assert(!delayed.makeAck(ack, sizeof(ack), used));
  assert(delayed.receive(wire, second, 33000, received) ==
         StationExchangeSecure::Rejected);

  // Transport-side signing errors fail closed with zero output.
  shared.failSign = true;
  assert(!sender.offer("Jazz24", 6, Url, std::strlen(Url), 7000,
                       ack, sizeof(ack), used) && used == 0);
  assert(!sender.pending());
  shared.failSign = false;
  assert(sender.offer("Jazz24", 6, Url, std::strlen(Url), 7100,
                      ack, sizeof(ack), used));

  // Session reset wipes pairing, replay/ACK state and queued transmit.
  receiver.reset();
  assert(!receiver.paired());
  noUnexpectedOffer(receiver, ack, used, 7200);
  paired(receiver, Bob, Alice, shared, Session + 2);
  assert(receiver.receive(ack, used, 7201, received) ==
         StationExchangeSecure::Rejected);
  assert(receiver.proposePair(Bob, Alice, Session + 3, 8000, auth(shared)));
  assert(!receiver.paired()); // re-pair wipes previous authorization
  assert(receiver.receive(ack, used, 8001, received) ==
         StationExchangeSecure::Rejected);

  std::puts("StationExchangeSecure: authenticated-envelope state machine host tests PASS");
  return 0;
}
