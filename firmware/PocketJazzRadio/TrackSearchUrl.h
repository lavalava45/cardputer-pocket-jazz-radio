// Offline URL builder for a LIKE -> phone QR search.
// No network, tokens, credentials, artist inference or Spotify API calls.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketjazz {
enum class TrackSearchProvider : uint8_t { Spotify, Web };
constexpr size_t kTrackQrMaxUrlBytes = 106; // QR version 6, ECC medium, byte mode

inline bool canSearchTrackTitle(const char* title) {
  if (!title || !*title) return false;
  size_t visible = 0;
  size_t count = 0;
  for (; count < 96 && title[count]; ++count) {
    const unsigned char c = static_cast<unsigned char>(title[count]);
    if (c < 32 || c == 127) return false;
    if (c != ' ') ++visible;
  }
  return visible >= 3 && count < 96 &&
      std::strcmp(title, "track-unknown") != 0;
}

inline int trackUtf8Length(const unsigned char* s, size_t remaining) {
  if (!remaining) return 0;
  const unsigned char a = s[0];
  if (a < 0x80) return a >= 32 && a < 127 ? 1 : 0;
  int n = a >= 0xC2 && a <= 0xDF ? 2
        : a >= 0xE0 && a <= 0xEF ? 3
        : a >= 0xF0 && a <= 0xF4 ? 4 : 0;
  if (!n || remaining < static_cast<size_t>(n)) return 0;
  for (int j = 1; j < n; ++j)
    if ((s[j] & 0xC0) != 0x80) return 0;
  if ((a == 0xE0 && s[1] < 0xA0) ||
      (a == 0xED && s[1] >= 0xA0) ||
      (a == 0xF0 && s[1] < 0x90) ||
      (a == 0xF4 && s[1] > 0x8F)) return 0;
  return n;
}

inline bool buildTrackSearchUrl(const char* title, TrackSearchProvider provider,
                                char* dst, size_t dstSize, bool* shortened = nullptr) {
  if (shortened) *shortened = false;
  if (!dst || dstSize == 0) return false;
  dst[0] = '\0';
  if (!canSearchTrackTitle(title)) return false;
  const char* prefix = provider == TrackSearchProvider::Spotify
      ? "https://open.spotify.com/search/"
      : "https://www.google.com/search?q=";
  const size_t prefixLen = std::strlen(prefix);
  const size_t ceiling = dstSize - 1 < kTrackQrMaxUrlBytes
      ? dstSize - 1 : kTrackQrMaxUrlBytes;
  if (prefixLen + 3 > ceiling) return false;

  // Reject malformed UTF-8/control bytes, even beyond any QR size limit.
  const auto* original = reinterpret_cast<const unsigned char*>(title);
  const size_t originalLen = std::strlen(title);
  for (size_t i = 0; i < originalLen;) {
    const int n = trackUtf8Length(original + i, originalLen - i);
    if (!n) return false;
    i += static_cast<size_t>(n);
  }
  // StreamTitle frequently has leading/trailing whitespace. Without trimming,
  // a title like 70 spaces + 'Miles Davis' can fill the whole QR capacity
  // before a single meaningful character, producing a false 'no like' UI.
  size_t start = 0, end = originalLen;
  while (start < end && original[start] == ' ') ++start;
  while (end > start && original[end - 1] == ' ') --end;
  const auto* bytes = original + start;
  const size_t count = end - start;
  if (!count || (count == 13 && std::memcmp(bytes, "track-unknown", 13) == 0))
    return false;

  std::memcpy(dst, prefix, prefixLen);
  size_t used = prefixLen, consumed = 0;
  constexpr char kHex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < count;) {
    const int n = trackUtf8Length(bytes + i, count - i);
    size_t required = 0;
    for (int j = 0; j < n; ++j) {
      const unsigned char c = bytes[i + j];
      const bool plain = (c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
          c == '-' || c == '_' || c == '.' || c == '~';
      required += plain ? 1 : 3;
    }
    if (used + required > ceiling) {
      if (shortened) *shortened = true;
      break;
    }
    for (int j = 0; j < n; ++j) {
      const unsigned char c = bytes[i + j];
      const bool plain = (c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
          c == '-' || c == '_' || c == '.' || c == '~';
      if (plain) dst[used++] = static_cast<char>(c);
      else {
        dst[used++] = '%';
        dst[used++] = kHex[c >> 4];
        dst[used++] = kHex[c & 15];
      }
    }
    i += static_cast<size_t>(n);
    consumed = i;
  }
  // Avoid appending an unfinished word-space, and never cut within a UTF-8 codepoint.
  while (used >= prefixLen + 3 && dst[used - 3] == '%' &&
         dst[used - 2] == '2' && dst[used - 1] == '0') used -= 3;
  dst[used] = '\0';
  if (consumed == 0 || used <= prefixLen) { dst[0] = '\0'; return false; }
  if (consumed < count && shortened) *shortened = true;
  return true;
}
} // namespace pocketjazz
