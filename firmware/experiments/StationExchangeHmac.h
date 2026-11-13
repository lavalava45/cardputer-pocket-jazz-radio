// SPDX-License-Identifier: MIT
// Opt-in ESP-NOW authenticated-envelope backend. NOT integrated into radio.
// Keys and peer identity must be provisioned out-of-band by a trusted caller.
#pragma once
#include "StationExchangeSecure.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#if defined(ARDUINO_ARCH_ESP32)
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#elif defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#error "StationExchangeHmac only supports ESP32 mbedTLS and Windows CNG"
#endif

namespace pocketjazz { namespace exchange { namespace secure {
class BoundPeerExchange;

// MAC key is exactly 256 *bits* (32 bytes) from trusted provisioning.
// Session key establishment, peer fingerprint confirmation and key persistence
// are NOT part of this class. It never reads/writes NVS, SD or flash.
class PeerHmacSha256 {
 public:
  static const size_t kKeyBytes = 32;
  PeerHmacSha256() : peer_{}, active_(false) { std::memset(key_, 0, sizeof(key_)); }
  ~PeerHmacSha256() { clear(); }
  PeerHmacSha256(const PeerHmacSha256&) = delete;
  PeerHmacSha256& operator=(const PeerHmacSha256&) = delete;

  bool load(const PeerId& peer, const uint8_t* key, size_t bytes) {
    clear();
    if (!validPeer(peer) || !key || bytes != kKeyBytes) return false;
    uint8_t combined = 0;
    for (size_t i = 0; i < kKeyBytes; ++i) combined |= key[i];
    if (combined == 0) return false;
    peer_ = peer;
    std::memcpy(key_, key, kKeyBytes);
    active_ = true;
    return true;
  }
  void clear() {
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_platform_zeroize(key_, sizeof(key_));
#else
    SecureZeroMemory(key_, sizeof(key_));
#endif
    peer_ = PeerId{};
    active_ = false;
  }
  bool ready() const { return active_; }
  // Synchronous one-shot MAC; no callback or pointer escapes this object.
  bool signTag(const uint8_t* message, size_t length, uint8_t tag[kTagBytes]) {
    return signCallback(this, peer_, message, length, tag);
  }
  bool verifyTag(const uint8_t* message, size_t length,
                 const uint8_t tag[kTagBytes]) {
    return verifyCallback(this, peer_, message, length, tag);
  }

 private:
  friend class BoundPeerExchange;
  // Intentionally private: handing this raw context to an unrelated
  // StationExchangeSecure permits use-after-free after the provider dies.
  Authenticator authenticator() {
    return active_ ? Authenticator{this, signCallback, verifyCallback} : Authenticator{};
  }
  static bool signCallback(void* context, const PeerId& peer,
                           const uint8_t* msg, size_t length,
                           uint8_t tag[kTagBytes]) {
    PeerHmacSha256* self = static_cast<PeerHmacSha256*>(context);
    uint8_t digest[32] = {};
    const bool valid = self && self->active_ && same(peer, self->peer_) &&
                       msg && tag && self->hmac(msg, length, digest);
    if (valid) std::memcpy(tag, digest, kTagBytes);
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_platform_zeroize(digest, sizeof(digest));
#else
    SecureZeroMemory(digest, sizeof(digest));
#endif
    return valid;
  }
  static bool verifyCallback(void* context, const PeerId& peer,
                             const uint8_t* msg, size_t length,
                             const uint8_t tag[kTagBytes]) {
    uint8_t calculated[kTagBytes] = {};
    const bool valid = tag && signCallback(context, peer, msg, length, calculated);
    // Compare full tag in fixed time (same number of XORs regardless of byte).
    uint8_t difference = 0;
    if (tag) for (size_t i = 0; i < kTagBytes; ++i)
      difference |= uint8_t(calculated[i] ^ tag[i]);
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_platform_zeroize(calculated, sizeof(calculated));
#else
    SecureZeroMemory(calculated, sizeof(calculated));
#endif
    return valid && difference == 0;
  }
  bool hmac(const uint8_t* msg, size_t length, uint8_t digest[32]) const {
    if (!active_ || !msg || !digest) return false;
#if defined(ARDUINO_ARCH_ESP32)
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return md && mbedtls_md_hmac(md, key_, kKeyBytes, msg, length, digest) == 0;
#else
    if (length > 0xffffffffu) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    const NTSTATUS opened = BCryptOpenAlgorithmProvider(&algorithm,
                                BCRYPT_SHA256_ALGORITHM, nullptr,
                                BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (opened < 0 || !algorithm) return false;
    const NTSTATUS hashed = BCryptHash(algorithm,
                                     const_cast<PUCHAR>(key_), ULONG(kKeyBytes),
                                     const_cast<PUCHAR>(msg), ULONG(length),
                                     digest, 32u);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return hashed >= 0;
#endif
  }
  PeerId peer_;
  uint8_t key_[kKeyBytes];
  bool active_;
};

// Safe composition: the authenticator and the protocol state always have the
// SAME C++ lifetime. No raw callbacks escape. A caller must keep THIS object
// alive until it stops using received/sent packets. Not thread-safe: serialize
// all calls on a single control task; do not call from an ESP-NOW ISR/callback.
class BoundPeerExchange {
 public:
  typedef StationExchangeSecure::Event Event;
  BoundPeerExchange() = default;
  BoundPeerExchange(const BoundPeerExchange&) = delete;
  BoundPeerExchange& operator=(const BoundPeerExchange&) = delete;

  bool proposePair(const PeerId& local, const PeerId& remote, uint64_t session,
                   uint32_t nowMs, const uint8_t* trustedKey, size_t keyLength) {
    reset();
    if (!peerAuth_.load(remote, trustedKey, keyLength)) return false;
    if (!exchange_.proposePair(local, remote, session, nowMs,
                               peerAuth_.authenticator())) {
      reset();
      return false;
    }
    pairProposed_ = true;
    pairStartedAt_ = nowMs;
    return true;
  }
  bool confirmPair(uint32_t nowMs, bool humanConfirmed) {
    const bool accepted = exchange_.confirmPair(nowMs, humanConfirmed);
    if (!accepted) reset();  // decline/timeout must erase unneeded key
    else pairProposed_ = false;
    return accepted;
  }
  bool paired() const { return exchange_.paired(); }
  bool pending() const { return exchange_.pending(); }
  void expire(uint32_t nowMs) {
    if (pairProposed_ && uint32_t(nowMs - pairStartedAt_) >
                             StationExchangeSecure::PairTimeoutMs) {
      reset(); // no dangling/expired pairing key after timeout
      return;
    }
    exchange_.expire(nowMs);
  }
  bool offer(const char* name, size_t nameLength, const char* url,
             size_t urlLength, uint32_t nowMs,
             uint8_t* output, size_t capacity, size_t& used) {
    return exchange_.offer(name, nameLength, url, urlLength, nowMs,
                           output, capacity, used);
  }
  bool retry(uint32_t nowMs, uint8_t* output, size_t cap, size_t& used) {
    return exchange_.retry(nowMs, output, cap, used);
  }
  Event receive(const uint8_t* bytes, size_t count, uint32_t nowMs,
                Packet& received) {
    return exchange_.receive(bytes, count, nowMs, received);
  }
  bool decideIncoming(bool accepted, uint32_t nowMs) {
    return exchange_.decideIncoming(accepted, nowMs);
  }
  bool makeAck(uint8_t* output, size_t cap, size_t& used) {
    return exchange_.makeAck(output, cap, used);
  }
  void reset() {
    // Invalidates EVERY stored callback before wiping its key and owner.
    exchange_.reset();
    peerAuth_.clear();
    pairProposed_ = false;
    pairStartedAt_ = 0;
  }

 private:
  PeerHmacSha256 peerAuth_;  // member order: protocol destroyed before HMAC
  StationExchangeSecure exchange_;
  bool pairProposed_ = false;
  uint32_t pairStartedAt_ = 0;
};

}}} // pocketjazz::exchange::secure
