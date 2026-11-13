// Conservative public-stream QR share card for Cardputer ADV (240x135 LCD).
// Uses the M5GFX-bundled QR encoder, not an extra network dependency.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "TrackSearchUrl.h"
#ifdef ARDUINO
#include <lgfx/utility/lgfx_qrcode.h>
#endif

namespace pocketjazz {
inline bool isPublicShareUrl(const char* url) {
  if (!url) return false;
  const size_t length = std::strlen(url);
  // Version 5 / medium ECC: <=84 byte-mode bytes, QR 37x37 + 4-module margin.
  if (length < 16 || length > 84 || std::strncmp(url, "https://", 8) != 0) return false;
  size_t hostEnd = 8;
  while (hostEnd < length && url[hostEnd] != '/') ++hostEnd;
  if (hostEnd == 8 || hostEnd == length || hostEnd - 8 > 100) return false;
  bool hostDot = false;
  for (size_t i = 8; i < hostEnd; ++i) {
    const char c = url[i];
    if (c == '.') hostDot = true;
    else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '-')) return false;
  }
  if (!hostDot) return false;
  // No literal IP, localhost, private mDNS DNS names or credential-like URLs.
  const size_t hostLen = hostEnd - 8;
  bool numeric = true;
  for (size_t i = 8; i < hostEnd; ++i)
    if (url[i] != '.' && (url[i] < '0' || url[i] > '9')) numeric = false;
  if (numeric) return false;
  if ((hostLen >= 6 && std::strncmp(url + hostEnd - 6, ".local", 6) == 0) ||
      (hostLen >= 4 && std::strncmp(url + hostEnd - 4, ".lan", 4) == 0) ||
      (hostLen >= 9 && std::strncmp(url + hostEnd - 9, ".internal", 9) == 0))
    return false;
  for (size_t i = 8; i < length; ++i) {
    const char c = url[i];
    if (c < 33 || c > 126 || c == '?' || c == '#' || c == '%' ||
        c == '\\' || c == '@' || c == ':') return false;
  }
  // A URL can still be private despite this filter. Only share URLs which
  // the user explicitly requested, and decline common secret-bearing paths.
  const char* deny[] = {"token", "secret", "password", "apikey", "auth", "session", "signature"};
  for (const auto* pattern : deny) {
    const size_t n = std::strlen(pattern);
    for (size_t i = 8; i + n <= length; ++i) {
      bool match = true;
      for (size_t j = 0; j < n; ++j) {
        const char c = url[i + j];
        const char normalized = (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
        if (normalized != pattern[j]) { match = false; break; }
      }
      if (match) return false;
    }
  }
  return true;
}

#ifdef ARDUINO
template<typename Display>
bool drawStationQr(Display& display, const char* url, int x, int y) {
  constexpr int version = 5;
  constexpr int modules = 4 * version + 17;  // 37
  constexpr int margin = 4, pixel = 2;
  constexpr int width = (modules + 2 * margin) * pixel; // 90x90
  if (!isPublicShareUrl(url) || x < 0 || y < 0 ||
      x + width > display.width() || y + width > display.height()) return false;
  uint8_t storage[172] = {};
  QRCode qr{};
  if (lgfx_qrcode_getBufferSize(version) > sizeof(storage) ||
      lgfx_qrcode_initText(&qr, storage, version, ECC_MEDIUM, url) != 0 ||
      qr.size != modules) return false;
  display.fillRect(x, y, width, width, 0xFFFF); // white quiet zone
  for (int yy = 0; yy < modules; ++yy)
    for (int xx = 0; xx < modules; ++xx)
      if (lgfx_qrcode_getModule(&qr, xx, yy))
        display.fillRect(x + (margin + xx) * pixel,
                         y + (margin + yy) * pixel, pixel, pixel, 0x0000);
  return true;
}

// Search QR is always generated from our own two fixed HTTPS URL prefixes.
// Version 6 medium ECC has up to 106 bytes (rather than station-QR v5's 84).
template<typename Display>
bool drawLikedTrackSearchQr(Display& display, const char* title,
                            TrackSearchProvider provider, int x, int y,
                            bool* shortened = nullptr) {
  char url[kTrackQrMaxUrlBytes + 1] = {};
  if (!buildTrackSearchUrl(title, provider, url, sizeof(url), shortened))
    return false;
  constexpr int version = 6, modules = 4 * version + 17;
  constexpr int margin = 4, pixel = 2;
  constexpr int width = (modules + 2 * margin) * pixel; // 98x98
  if (x < 0 || y < 0 || x + width > display.width() ||
      y + width > display.height()) return false;
  uint8_t storage[240] = {};
  QRCode qr{};
  if (lgfx_qrcode_getBufferSize(version) > sizeof(storage) ||
      lgfx_qrcode_initText(&qr, storage, version, ECC_MEDIUM, url) != 0 ||
      qr.size != modules) return false;
  display.fillRect(x, y, width, width, 0xFFFF);
  for (int yy = 0; yy < modules; ++yy)
    for (int xx = 0; xx < modules; ++xx)
      if (lgfx_qrcode_getModule(&qr, xx, yy))
        display.fillRect(x + (margin + xx) * pixel,
                         y + (margin + yy) * pixel, pixel, pixel, 0x0000);
  return true;
}
#endif
} // namespace pocketjazz
