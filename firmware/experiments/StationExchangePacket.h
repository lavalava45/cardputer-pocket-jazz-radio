// SPDX-License-Identifier: MIT
// Host-only ESP-NOW station-offer/ack wire format. NO transport, pairing or
// automatic playback. Total packet never exceeds the legacy 250-byte MTU.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "../PocketJazzRadio/StationLogoFormat.h"

namespace pocketjazz { namespace exchange {
constexpr size_t kMaxPacketBytes = 250;
constexpr size_t kHeaderBytes = 12;
constexpr size_t kCrcBytes = 4;
constexpr size_t kMaxName = 32;
constexpr size_t kMaxUrl = 191;
enum Type : uint8_t { Offer = 1, Acknowledge = 2 };
struct Packet {
  Type type = Offer;
  uint32_t nonce = 0;
  char name[kMaxName + 1]{};
  char url[kMaxUrl + 1]{};
};

inline void write32(uint8_t* p, uint32_t n) {
  p[0] = uint8_t(n); p[1] = uint8_t(n >> 8);
  p[2] = uint8_t(n >> 16); p[3] = uint8_t(n >> 24);
}
inline bool validAscii(const char* s, size_t maxLength) {
  if (!s || !maxLength) return false;
  size_t i = 0;
  // Both Packet fields reserve maxLength+1 bytes for a terminator.
  // Check the bound BEFORE dereferencing, including an untrusted field with
  // no NUL in its entire fixed-size storage.
  while (i <= maxLength && s[i]) {
    if ((unsigned char)s[i] < 32 || (unsigned char)s[i] > 126) return false;
    ++i;
  }
  return i > 0 && i <= maxLength; // NUL observed within the actual field
}
// Never broadcast credentials, query tokens, userinfo, fragments or private
// LAN links. The receiver must STILL require explicit user confirmation.
inline bool validOfferUrl(const char* url) {
  if (!validAscii(url, kMaxUrl)) return false;
  const size_t scheme = std::strncmp(url, "https://", 8) == 0 ? 8 :
                        std::strncmp(url, "http://", 7) == 0 ? 7 : 0;
  if (!scheme) return false;
  const char* host = url + scheme;
  const char* slash = std::strchr(host, '/');
  const size_t hostLen = slash ? size_t(slash - host) : std::strlen(host);
  if (!hostLen || hostLen > 120 || !std::memchr(host, '.', hostLen)) return false;
  for (const char* c = url; *c; ++c)
    if (*c == '?' || *c == '#' || *c == '@' || *c == '\\' || *c == ' ') return false;
  // Refuse obvious loopback/local-address literals; this is a UI convenience
  // filter, NOT a DNS/public-network security proof.
  if (std::strncmp(host, "127.", 4) == 0 ||
      std::strncmp(host, "10.", 3) == 0 ||
      std::strncmp(host, "192.168.", 8) == 0 ||
      std::strncmp(host, "169.254.", 8) == 0 ||
      std::strncmp(host, "172.16.", 7) == 0 ||
      std::strncmp(host, "localhost", 9) == 0) return false;
  return true;
}

inline bool encode(const Packet& item, uint8_t* out, size_t cap, size_t& used) {
  used = 0;
  if (!out || cap < kHeaderBytes + kCrcBytes ||
      (item.type != Offer && item.type != Acknowledge)) return false;
  if (item.type == Offer && (!validAscii(item.name, kMaxName) || !validOfferUrl(item.url))) return false;
  // Never strlen an unterminated caller-owned fixed-size packet field.
  const size_t nl = item.type == Offer ? std::strlen(item.name) : 0;
  const size_t ul = item.type == Offer ? std::strlen(item.url) : 0;
  if (nl + ul + kHeaderBytes + kCrcBytes > cap ||
      nl + ul + kHeaderBytes + kCrcBytes > kMaxPacketBytes) return false;
  const size_t end = kHeaderBytes + nl + ul;
  std::memset(out, 0, end + kCrcBytes);
  out[0] = 'P'; out[1] = 'J'; out[2] = 'S'; out[3] = 'T';
  out[4] = 1; out[5] = item.type;
  write32(out + 6, item.nonce);
  out[10] = uint8_t(nl); out[11] = uint8_t(ul);
  if (nl) std::memcpy(out + kHeaderBytes, item.name, nl);
  if (ul) std::memcpy(out + kHeaderBytes + nl, item.url, ul);
  write32(out + end, pocketjazz::logo::crc32(out, end));
  used = end + kCrcBytes;
  return true;
}

inline bool decode(const uint8_t* bytes, size_t len, Packet& out) {
  if (!bytes || len < kHeaderBytes + kCrcBytes || len > kMaxPacketBytes ||
      bytes[0] != 'P' || bytes[1] != 'J' || bytes[2] != 'S' || bytes[3] != 'T' ||
      bytes[4] != 1 || (bytes[5] != Offer && bytes[5] != Acknowledge)) return false;
  const size_t nl = bytes[10], ul = bytes[11];
  if (nl > kMaxName || ul > kMaxUrl ||
      len != kHeaderBytes + nl + ul + kCrcBytes) return false;
  if ((bytes[5] == Offer && (!nl || !ul)) ||
      (bytes[5] == Acknowledge && (nl || ul))) return false;
  if (pocketjazz::logo::get32le(bytes + len - kCrcBytes) !=
      pocketjazz::logo::crc32(bytes, len - kCrcBytes)) return false;
  Packet candidate{};
  candidate.type = static_cast<Type>(bytes[5]);
  candidate.nonce = pocketjazz::logo::get32le(bytes + 6);
  if (nl) std::memcpy(candidate.name, bytes + kHeaderBytes, nl);
  if (ul) std::memcpy(candidate.url, bytes + kHeaderBytes + nl, ul);
  candidate.name[nl] = 0; candidate.url[ul] = 0;
  if (candidate.type == Offer &&
      (!validAscii(candidate.name, kMaxName) || !validOfferUrl(candidate.url))) return false;
  out = candidate;
  return true;
}
}} // namespace pocketjazz::exchange
