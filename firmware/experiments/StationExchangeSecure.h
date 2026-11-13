// SPDX-License-Identifier: MIT
// HOST-ONLY protocol. No ESP-NOW driver, keys, pairing transport or playback.
// Never treat StationExchangePacket CRC32 as authentication.
#pragma once
#include "StationExchangePacket.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketjazz { namespace exchange { namespace secure {

// Envelope: magic(4) session(8 LE) sender(6) recipient(6) length(2 LE),
// PJST v1 wire packet(variable) then external authenticator(16).
// The FULL preceding envelope, including IDs and session, MUST be MACed.
// Verifier must cryptographically authenticate a peer-specific key and must
// compare tags in constant time. Do not implement it with CRC/FNV/plain hash.
enum : size_t {
  kPeerBytes = 6, kSessionBytes = 8, kTagBytes = 16,
  kEnvelopeHeader = 4 + kSessionBytes + 2 * kPeerBytes + 2,
  kEnvelopeOverhead = kEnvelopeHeader + kTagBytes,
  kMaxWire = kMaxPacketBytes - kEnvelopeOverhead
};
static_assert(kEnvelopeHeader == 26 && kMaxWire == 208,
              "authenticated frame must fit legacy ESP-NOW 250 byte MTU");
struct PeerId { uint8_t b[kPeerBytes]; };
inline bool same(const PeerId& a, const PeerId& b) {
  return std::memcmp(a.b, b.b, kPeerBytes) == 0;
}
inline bool validPeer(const PeerId& peer) {
  uint8_t zero = 0, allFF = 0xff;
  for (size_t i = 0; i < kPeerBytes; ++i) {
    zero |= peer.b[i]; allFF &= peer.b[i];
  }
  return zero != 0 && allFF != 0xff && !(peer.b[0] & 1); // unicast MAC
}
inline bool sameBytes(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t different = 0;
  for (size_t i = 0; i < n; ++i) different |= a[i] ^ b[i];
  return different == 0;
}
inline void write64(uint8_t* dst, uint64_t value) {
  for (unsigned i = 0; i < 8; ++i) dst[i] = uint8_t(value >> (8 * i));
}
inline uint64_t read64(const uint8_t* src) {
  uint64_t result = 0;
  for (unsigned i = 0; i < 8; ++i)
    result |= uint64_t(src[i]) << (8 * i);
  return result;
}

// Caller provisions peer-specific cryptographic keys through a trusted setup
// path. These functions must use an audited MAC (e.g. HMAC-SHA256 truncation)
// and MUST NOT reuse one unscoped key for arbitrary unpaired peers.
struct Authenticator {
  void* context;
  bool (*sign)(void*, const PeerId&, const uint8_t*, size_t,
               uint8_t[kTagBytes]);
  bool (*verify)(void*, const PeerId&, const uint8_t*, size_t,
                 const uint8_t[kTagBytes]);
};
inline bool usable(const Authenticator& a) {
  return a.sign != nullptr && a.verify != nullptr;
}

inline bool frame(const Packet& packet, const PeerId& sender,
                  const PeerId& recipient, uint64_t session,
                  const Authenticator& auth, uint8_t* output, size_t capacity,
                  size_t& used) {
  used = 0;
  if (!output || !validPeer(sender) || !validPeer(recipient) ||
      same(sender, recipient) || !session || !usable(auth) ||
      capacity < kEnvelopeOverhead + kHeaderBytes + kCrcBytes) return false;
  uint8_t wire[kMaxWire] = {};
  size_t wireUsed = 0;
  if (!encode(packet, wire, sizeof(wire), wireUsed) || !packet.nonce ||
      wireUsed > kMaxWire || capacity < kEnvelopeOverhead + wireUsed)
    return false;
  uint8_t data[kMaxPacketBytes] = {};
  data[0] = 'P'; data[1] = 'J'; data[2] = 'S'; data[3] = '2';
  write64(data + 4, session);
  std::memcpy(data + 12, sender.b, kPeerBytes);
  std::memcpy(data + 18, recipient.b, kPeerBytes);
  data[24] = uint8_t(wireUsed);
  data[25] = uint8_t(wireUsed >> 8);
  std::memcpy(data + kEnvelopeHeader, wire, wireUsed);
  const size_t signedLength = kEnvelopeHeader + wireUsed;
  uint8_t tag[kTagBytes] = {};
  if (!auth.sign(auth.context, recipient, data, signedLength, tag)) return false;
  std::memcpy(data + signedLength, tag, kTagBytes);
  used = signedLength + kTagBytes;
  std::memcpy(output, data, used); // no partial output after sign failure
  return true;
}

inline bool unframe(const uint8_t* data, size_t length,
                    const PeerId& expectedSender,
                    const PeerId& expectedRecipient, uint64_t session,
                    const Authenticator& auth, Packet& out) {
  if (!data || !usable(auth) || !session ||
      !validPeer(expectedSender) || !validPeer(expectedRecipient) ||
      length < kEnvelopeOverhead + kHeaderBytes + kCrcBytes ||
      length > kMaxPacketBytes ||
      data[0] != 'P' || data[1] != 'J' || data[2] != 'S' ||
      data[3] != '2' || read64(data + 4) != session ||
      !sameBytes(data + 12, expectedSender.b, kPeerBytes) ||
      !sameBytes(data + 18, expectedRecipient.b, kPeerBytes))
    return false;
  const size_t wireUsed = size_t(data[24]) | (size_t(data[25]) << 8);
  if (wireUsed < kHeaderBytes + kCrcBytes || wireUsed > kMaxWire ||
      length != kEnvelopeOverhead + wireUsed) return false;
  const size_t signedLength = kEnvelopeHeader + wireUsed;
  if (!auth.verify(auth.context, expectedSender, data, signedLength,
                   data + signedLength)) return false;
  Packet candidate{};
  if (!decode(data + kEnvelopeHeader, wireUsed, candidate) ||
      !candidate.nonce) return false;
  out = candidate;
  return true;
}

// One explicit peer, one outstanding offer, one last received offer and no heap.
// Session is an EXTERNALLY established unpredictable NONZERO 64-bit pairwise
// epoch; it MUST change after restart/re-pair. This API does not perform a
// cryptographic key exchange and cannot attest a peer on its own.
class StationExchangeSecure {
 public:
  enum Event { Rejected, NoAction, PresentOffer, AckReady, DeliveryConfirmed };
  enum { MaxRetries = 3, MaxAckTransmissions = 4,
         RetryIntervalMs = 700, MaxPendingMs = 5000,
         PairTimeoutMs = 30000, OfferDecisionMs = 30000 };
  StationExchangeSecure()
      : auth_{}, local_{}, peer_{}, session_(0), pairStarted_(0),
        paired_(false), proposed_(false), awaiting_(false), accepted_(false),
        lastIncomingNonce_(0), nextOutgoingNonce_(1), retryCount_(0),
        ackSentCount_(0),
        sentAt_(0), firstSentAt_(0), cachedOfferLen_(0), acceptedWireLen_(0) {}

  // Begin human-controlled pairing: the local UI must DISPLAY peer identity
  // and secure fingerprint acquired out-of-band. No wireless frame pairs us.
  // The session and keys must be authenticated/provisioned separately.
  bool proposePair(const PeerId& local, const PeerId& peer, uint64_t session,
                   uint32_t now, const Authenticator& auth) {
    reset();
    if (!usable(auth) || !validPeer(local) || !validPeer(peer) ||
        same(local, peer) || !session) return false;
    local_ = local; peer_ = peer; session_ = session;
    auth_ = auth; pairStarted_ = now; proposed_ = true;
    return true;
  }
  bool confirmPair(uint32_t now, bool humanConfirmed) {
    if (!proposed_ || paired_ || !humanConfirmed ||
        uint32_t(now - pairStarted_) > PairTimeoutMs) {
      if (proposed_ && uint32_t(now - pairStarted_) > PairTimeoutMs) reset();
      return false;
    }
    paired_ = true;
    proposed_ = false;
    return true;
  }
  bool paired() const { return paired_; }
  bool pending() const { return paired_ && awaiting_; }
  void reset() {
    // Session lifetime ends. No accepted offer automatically modifies radio.
    auth_ = Authenticator{};
    local_ = PeerId{}; peer_ = PeerId{};
    session_ = 0; pairStarted_ = 0;
    paired_ = proposed_ = awaiting_ = accepted_ = hasUnconfirmed_ = false;
    pendingIncoming_ = Packet{};
    lastIncomingNonce_ = 0; nextOutgoingNonce_ = 1;
    incomingAt_ = 0;
    ackSentCount_ = 0;
    retryCount_ = 0; sentAt_ = firstSentAt_ = 0;
    cachedOfferLen_ = acceptedWireLen_ = 0;
    std::memset(cachedOffer_, 0, sizeof(cachedOffer_));
    std::memset(acceptedWire_, 0, sizeof(acceptedWire_));
  }

  // Fresh transaction nonce allocated by this bounded state machine.
  // Refuse wrap, rather than reuse nonce in a still-valid pairing session.
  bool offer(const char* name, size_t nameLength,
             const char* url, size_t urlLength, uint32_t now,
             uint8_t* output, size_t cap, size_t& used) {
    used = 0;
    if (!paired_ || awaiting_ || nextOutgoingNonce_ == 0 ||
        !name || !url || !output || !nameLength ||
        nameLength > kMaxName || !urlLength || urlLength > kMaxUrl)
      return false;
    Packet packet{};
    packet.type = Offer;
    packet.nonce = nextOutgoingNonce_;
    std::memcpy(packet.name, name, nameLength);
    std::memcpy(packet.url, url, urlLength);
    packet.name[nameLength] = 0;
    packet.url[urlLength] = 0;
    // Reject embedded NULs and malformed text before entering PJST encoder.
    if (std::strlen(packet.name) != nameLength ||
        std::strlen(packet.url) != urlLength) return false;
    uint8_t tmp[kMaxPacketBytes] = {};
    size_t length = 0;
    if (!frame(packet, local_, peer_, session_, auth_,
               tmp, sizeof(tmp), length) || length > cap) return false;
    std::memcpy(cachedOffer_, tmp, length);
    std::memcpy(output, tmp, length);
    cachedOfferLen_ = length;
    awaiting_ = true; sentAt_ = firstSentAt_ = now; retryCount_ = 0;
    ++nextOutgoingNonce_; // wrap disables new sends until a new session
    used = length;
    return true;
  }

  // Exactly three retries after the original transmission, then stop. A
  // duplicate authenticated ACK may be received but cannot issue a new offer.
  bool retry(uint32_t now, uint8_t* output, size_t cap, size_t& used) {
    used = 0;
    if (!paired_ || !awaiting_ || !output ||
        uint32_t(now - firstSentAt_) > MaxPendingMs) {
      if (awaiting_ && uint32_t(now - firstSentAt_) > MaxPendingMs)
        awaiting_ = false;
      return false;
    }
    if (retryCount_ >= MaxRetries ||
        uint32_t(now - sentAt_) < RetryIntervalMs ||
        cap < cachedOfferLen_) return false;
    std::memcpy(output, cachedOffer_, cachedOfferLen_);
    used = cachedOfferLen_;
    sentAt_ = now; ++retryCount_;
    return true;
  }
  void expire(uint32_t now) {
    if (awaiting_ && uint32_t(now - firstSentAt_) > MaxPendingMs)
      awaiting_ = false;
    if (proposed_ && uint32_t(now - pairStarted_) > PairTimeoutMs)
      reset();
    if (hasUnconfirmed_ && uint32_t(now - incomingAt_) > OfferDecisionMs) {
      hasUnconfirmed_ = false;
      accepted_ = false;
      acceptedWireLen_ = 0;
      pendingIncoming_ = Packet{};
      std::memset(acceptedWire_, 0, sizeof(acceptedWire_));
    }
  }

  // On a new offer return PresentOffer and copy bounded fields to 'received'.
  // Do not accept/auto-play the URL until a SEPARATE human decision.
  // Authenticated exact duplicate of previously accepted offer yields
  // AckReady, but never re-presents it; caller must send via makeAck().
  Event receive(const uint8_t* data, size_t length,
                uint32_t now, Packet& received) {
    expire(now);
    if (!paired_) return Rejected;
    Packet candidate{};
    if (!unframe(data, length, peer_, local_, session_, auth_, candidate))
      return Rejected;
    if (candidate.type == Acknowledge) {
      if (awaiting_ &&
          candidate.nonce == nextOutgoingNonce_ - 1) {
        awaiting_ = false;
        return DeliveryConfirmed;
      }
      return NoAction;
    }
    if (candidate.nonce < lastIncomingNonce_) return Rejected;
    if (candidate.nonce == lastIncomingNonce_) {
      if (accepted_ && ackSentCount_ < MaxAckTransmissions &&
          length == acceptedWireLen_ &&
          sameBytes(data, acceptedWire_, length)) return AckReady;
      return Rejected;
    }
    // Do not permit a newer offer while an earlier one awaits the human UI.
    if (hasUnconfirmed_) return Rejected;
    lastIncomingNonce_ = candidate.nonce;
    hasUnconfirmed_ = true;
    incomingAt_ = now;
    accepted_ = false;
    ackSentCount_ = 0;
    pendingIncoming_ = candidate;
    acceptedWireLen_ = length;
    std::memcpy(acceptedWire_, data, length);
    received = candidate;
    return PresentOffer;
  }

  // Explicit human acceptance, never automatic. Rejected offers consume nonce
  // so the same sender cannot replay it to re-open a dismissed prompt.
  bool decideIncoming(bool humanAccepted, uint32_t now) {
    expire(now);
    if (!paired_ || !hasUnconfirmed_) return false;
    hasUnconfirmed_ = false;
    accepted_ = humanAccepted;
    if (!humanAccepted) {
      acceptedWireLen_ = 0;
      std::memset(acceptedWire_, 0, sizeof(acceptedWire_));
    }
    return true;
  }
  bool makeAck(uint8_t* output, size_t cap, size_t& used) {
    used = 0;
    if (!paired_ || !accepted_ || hasUnconfirmed_ || !lastIncomingNonce_ ||
        ackSentCount_ >= MaxAckTransmissions)
      return false;
    Packet ack{};
    ack.type = Acknowledge;
    ack.nonce = lastIncomingNonce_;
    if (!frame(ack, local_, peer_, session_, auth_, output, cap, used))
      return false;
    ++ackSentCount_;
    return true;
  }

 private:
  Authenticator auth_;
  PeerId local_, peer_;
  uint64_t session_;
  uint32_t pairStarted_;
  bool paired_, proposed_, awaiting_, accepted_;
  bool hasUnconfirmed_ = false;
  Packet pendingIncoming_{};
  uint32_t lastIncomingNonce_, nextOutgoingNonce_;
  uint32_t incomingAt_;
  unsigned retryCount_;
  unsigned ackSentCount_;
  uint32_t sentAt_, firstSentAt_;
  size_t cachedOfferLen_, acceptedWireLen_;
  uint8_t cachedOffer_[kMaxPacketBytes];
  uint8_t acceptedWire_[kMaxPacketBytes];
};

}}} // namespace pocketjazz::exchange::secure
