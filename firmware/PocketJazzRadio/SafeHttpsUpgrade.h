// SPDX-License-Identifier: GPL-3.0-or-later
// Pure, allocation-free allowlist for an explicit HTTP -> verified HTTPS
// upgrade. Never authorizes HTTPS -> HTTP or protocol-relative redirects.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketjazz {

inline bool copySafeHttpsUpgrade(const char* location, char* out, size_t cap) {
  if (!out || !cap) return false;
  out[0] = 0;
  if (!location || cap < 11 || std::strncmp(location, "https://", 8)) return false;
  size_t length = 0;
  // Bounded scan: untrusted Location must never overflow the command URL or
  // retain an unbounded address, even when provided by a malformed server.
  while (length < cap && location[length]) {
    const unsigned char c = static_cast<unsigned char>(location[length]);
    if (c < 33 || c > 126 || c == '#' || c == '@' || c == '\\') return false;
    ++length;
  }
  if (length == cap || length <= 8) return false;
  size_t hostEnd = 8;
  while (hostEnd < length && location[hostEnd] != '/' && location[hostEnd] != '?')
    ++hostEnd;
  if (hostEnd == 8) return false;
  // Hosts are DNS labels or an IPv4 address; no embedded credentials, IPv6
  // literals, userinfo, whitespace, NUL obfuscation or non-numeric ports.
  // HTTPS target is still verified against the CA and original redirect host.
  size_t portAt = hostEnd;
  for (size_t i = 8; i < hostEnd; ++i) {
    if (location[i] == ':') {
      if (portAt != hostEnd) return false;
      portAt = i;
    } else if (i < portAt) {
      const unsigned char c = static_cast<unsigned char>(location[i]);
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    }
  }
  if (portAt == 8) return false;
  if (portAt < hostEnd) {
    if (portAt + 1 == hostEnd) return false;
    uint32_t port = 0;
    for (size_t i = portAt + 1; i < hostEnd; ++i) {
      const char c = location[i];
      if (c < '0' || c > '9') return false;
      port = port * 10 + static_cast<uint32_t>(c - '0');
      if (port > 65535u) return false;
    }
    if (!port) return false;
  }
  // Arduino-ESP32 3.3.9 HTTPClient::beginInternal() finds the hostname's
  // end with indexOf('/'), NOT indexOf('?'). A standards-valid host?query
  // would otherwise attempt DNS for the literal "host?query". Insert the
  // empty path slash before the query, without altering hostname or port.
  const bool needsRootSlash = hostEnd < length && location[hostEnd] == '?';
  if (needsRootSlash && length + 1 >= cap) return false;
  std::memcpy(out, location, hostEnd);
  size_t outLength = hostEnd;
  if (needsRootSlash) out[outLength++] = '/';
  std::memcpy(out + outLength, location + hostEnd, length - hostEnd);
  out[outLength + length - hostEnd] = 0;
  return true;
}

} // namespace pocketjazz
