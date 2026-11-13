// SPDX-License-Identifier: MIT
// Streaming, allocation-free offline jazz encyclopedia. No RAM-sized index.
#pragma once
#include "JazzNotesSearch.h"  // Reuse the bounded NotesFile read callback.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace pocketjazz {
struct JazzBiography {
  char key[65];
  char name[97];
  char summary[193];
  char album[113];
  char source[225];  // Provenance is retained OFF-SCREEN.
};

class JazzEncyclopedia {
 public:
  static constexpr size_t MaxFileBytes = 3 * 1024 * 1024;
  static constexpr size_t MaxRows = 8000;
  static constexpr size_t MaxLineBytes = 752;

  bool open(NotesFile file) {
    file_ = file;
    return file_.read && file_.size && file_.size <= MaxFileBytes;
  }
  bool valid() const { return file_.read && file_.size && file_.size <= MaxFileBytes; }

  // A page is copied into user-provided storage; input data never resides
  // entirely in RAM. totalMatches counts all matching rows, not just the page.
  // False signals corrupt / unreadable SD data; do not show partial results.
  bool search(const char* query, JazzBiography* out, size_t capacity,
              size_t skip, size_t& stored, size_t& totalMatches,
              bool exactKeyOnly = false) const {
    stored = totalMatches = 0;
    if (!valid() || !query || !*query || (capacity && !out) || capacity > 8)
      return false;
    size_t queryLength = 0;
    for (; query[queryLength]; ++queryLength) {
      const unsigned char ch = static_cast<unsigned char>(query[queryLength]);
      if (queryLength >= 64 || ch < 32 || ch > 126) return false;
    }
    JazzBiography entry = {};
    char line[MaxLineBytes] = {};
    size_t length = 0, count = 0, cursor = 0, fileStart = 0;
    uint8_t cache[512] = {};
    size_t cached = 0, cursorCache = 0;
    while (cursor < file_.size) {
      if (cursorCache == cached) {
        cached = file_.size - cursor;
        if (cached > sizeof(cache)) cached = sizeof(cache);
        if (!file_.read(file_.context, cursor, cache, cached)) return false;
        cursorCache = 0;
      }
      const uint8_t c = cache[cursorCache++];
      ++cursor;
      if (c == '\n') {
        if (length == 0 || count >= MaxRows) return false;
        line[length] = 0;
        if (!parse(line, entry)) return false;
        ++count;
        if ((exactKeyOnly && strcmp(entry.key, query) == 0) ||
            (!exactKeyOnly && (contains(entry.name, query) ||
             contains(entry.key, query) || contains(entry.summary, query) ||
             contains(entry.album, query)))) {
          if (totalMatches >= skip && stored < capacity) out[stored++] = entry;
          ++totalMatches;
        }
        length = 0;
        fileStart = cursor;
      } else {
        if (c == '\r' || (c < 32 && c != '\t') ||
            c >= 127 || length + 1 >= sizeof(line)) return false;
        line[length++] = static_cast<char>(c);
      }
    }
    return !length && cursor == file_.size && fileStart == cursor && count > 0;
  }

  bool find(const char* key, JazzBiography& out) const {
    size_t found = 0, total = 0;
    return search(key, &out, 1, 0, found, total, true) && found == 1;
  }

 private:
  static bool contains(const char* haystack, const char* needle) {
    if (!*needle) return false;
    for (const char* h = haystack; *h; ++h) {
      const char* a = h; const char* b = needle;
      while (*a && *b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = char(x + ('a'-'A'));
        if (y >= 'A' && y <= 'Z') y = char(y + ('a'-'A'));
        if (x == ' ' || x == '-') x = '-';
        if (y == ' ' || y == '-') y = '-';
        if (x != y) break;
        ++a; ++b;
      }
      if (!*b) return true;
    }
    return false;
  }
  static bool copy(char* dst, size_t cap, const char* src) {
    size_t n = strlen(src);
    if (!n || n >= cap) return false;
    memcpy(dst, src, n + 1);
    return true;
  }
  static bool keyValid(const char* key) {
    if (!*key) return false;
    bool dash = false;
    size_t i = 0;
    for (; key[i]; ++i) {
      if (i >= 64) return false;
      const char c = key[i];
      if (c == '-') {
        if (!i || dash) return false;
        dash = true;
      } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        dash = false;
      } else return false;
    }
    return !dash;
  }
  static bool parse(char* line, JazzBiography& out) {
    char* f[5] = {line};
    size_t found = 0;
    for (char* p = line; *p; ++p) {
      if (*p == '\t') {
        if (++found >= 5) return false;
        *p = 0;
        f[found] = p + 1;
      }
    }
    if (found != 4 || !keyValid(f[0])) return false;
    if (!copy(out.key, sizeof(out.key), f[0]) ||
        !copy(out.name, sizeof(out.name), f[1]) ||
        !copy(out.summary, sizeof(out.summary), f[2]) ||
        !copy(out.album, sizeof(out.album), f[3]) ||
        !copy(out.source, sizeof(out.source), f[4])) return false;
    if (strncmp(out.source, "https://", 8)) return false;
    return true;
  }
  NotesFile file_ = {};
};
} // namespace pocketjazz
