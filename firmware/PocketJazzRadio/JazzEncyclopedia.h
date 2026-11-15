// SPDX-License-Identifier: MIT
// Streaming, allocation-free offline jazz encyclopedia. No RAM-sized index.
#pragma once
#include "JazzNotesSearch.h"  // Reuse the bounded NotesFile read callback.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
  static constexpr size_t MaxIndexBytes = 256 * 1024;

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

  // Validate the exact TSV + sorted-index artifact ONCE before audio starts.
  // Both CRC32 values are in the v2 index header, produced offline from the
  // immutable source corpus. A size/count match alone cannot authenticate an
  // indexed miss: a same-byte-length stale index can silently hide a musician.
  // CRC32 protects accidental corruption, not malicious tampering. It uses
  // bounded 256-byte chunks; no file-sized allocation, NVS or network.
  bool verifyIndex(NotesFile index) const {
    size_t first = 0, rows = 0, tsvBytes = 0;
    uint32_t expectedTsv = 0, expectedIndex = 0;
    if (!valid() || !validIndex(index) ||
        !indexHeader(index, first, tsvBytes, rows, expectedTsv,
                     expectedIndex) ||
        tsvBytes != file_.size || rows == 0 || rows > MaxRows) return false;
    uint32_t actualTsv = 0, actualIndex = 0;
    if (!crc32(file_, 0, file_.size, actualTsv) ||
        !crc32(index, first, index.size - first, actualIndex) ||
        actualTsv != expectedTsv || actualIndex != expectedIndex) return false;

    // Checksums alone do not prove that an index built by a buggy tool is
    // sorted, complete or points at the right TSV records. Before enabling
    // authoritative negative lookups, verify every ordered entry/offset.
    // Boot/STOP only: never do this O(N) pass in the audio worker or while
    // the stream plays. No index-sized RAM allocation.
    size_t sourceOffset = 0, indexOffset = first;
    char previous[65]{};
    for (size_t i = 0; i < rows; ++i) {
      if (indexOffset >= index.size || sourceOffset >= file_.size) return false;
      char candidate[65]{}, row[MaxLineBytes]{};
      size_t candidateOffset = 0, indexAfter = 0, sourceAfter = 0;
      if (!readIndexEntry(index, indexOffset, candidate,
                          candidateOffset, indexAfter) ||
          candidateOffset != sourceOffset ||
          (i && strcmp(previous, candidate) >= 0) ||
          !readIndexLine(file_, sourceOffset, row, sizeof(row), sourceAfter))
        return false;
      char* tab = strchr(row, '\t');
      if (!tab || size_t(tab - row) >= sizeof(candidate) ||
          strncmp(row, candidate, size_t(tab - row)) ||
          candidate[size_t(tab - row)] != 0)
        return false;
      strcpy(previous, candidate);
      sourceOffset = sourceAfter;
      indexOffset = indexAfter;
    }
    return sourceOffset == file_.size && indexOffset == index.size;
  }

  // Optional sorted SD index: O(log N) random-access probes, no RAM index.
  // Only a previously verified, immutable file pair may produce a definitive
  // indexed MISS. For unverified/corrupt index data, fallback is authoritative.
  bool find(const char* key, JazzBiography& out, NotesFile index = {},
            bool trustedIndex = false) const {
    bool indexedMiss = false;
    if (valid() && validIndex(index) &&
        key && keyValid(key)) {
      if (findIndexed(key, out, index, indexedMiss)) return true;
      if (indexedMiss && trustedIndex) return false;
    }
    size_t found = 0, total = 0;
    return search(key, &out, 1, 0, found, total, true) && found == 1;
  }

 private:
  static bool validIndex(NotesFile index) {
    return index.read && index.size && index.size <= MaxIndexBytes;
  }
  static bool lowerHex8(const char* s, uint32_t& out) {
    if (!s || strlen(s) != 8) return false;
    uint32_t number = 0;
    for (size_t i = 0; i < 8; ++i) {
      const char c = s[i];
      uint8_t digit = 0;
      if (c >= '0' && c <= '9') digit = uint8_t(c - '0');
      else if (c >= 'a' && c <= 'f') digit = uint8_t(c - 'a' + 10);
      else return false;
      number = (number << 4) | digit;
    }
    out = number;
    return true;
  }
  static bool crc32(NotesFile file, size_t offset, size_t length,
                    uint32_t& result) {
    uint32_t crc = 0xffffffffu;
    uint8_t bytes[256]{};
    while (length) {
      const size_t count = length > sizeof(bytes) ? sizeof(bytes) : length;
      if (!file.read(file.context, offset, bytes, count)) return false;
      for (size_t i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
          crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
      }
      length -= count;
      offset += count;
    }
    result = crc ^ 0xffffffffu;
    return true;
  }
  static bool indexHeader(NotesFile index, size_t& first,
                          size_t& tsvBytes, size_t& rows,
                          uint32_t& tsvCrc, uint32_t& indexCrc) {
    char header[128]{};
    if (!readIndexLine(index, 0, header, sizeof(header), first)) return false;
    char tsvHex[9]{}, indexHex[9]{};
    int consumed = 0;
    if (sscanf(header, "#PJ-JAZZ-IDX\t2\t%zu\t%zu\t%8[0-9a-f]\t%8[0-9a-f]%n",
               &tsvBytes, &rows, tsvHex, indexHex, &consumed) != 4 ||
        consumed <= 0 || header[consumed] != 0 ||
        !lowerHex8(tsvHex, tsvCrc) || !lowerHex8(indexHex, indexCrc) ||
        first >= index.size) return false;
    return true;
  }
  static bool readIndexLine(NotesFile index, size_t begin, char* dst,
                            size_t capacity, size_t& next) {
    if (!dst || capacity < 3 || begin >= index.size) return false;
    size_t len = 0, cursor = begin;
    uint8_t bytes[96]{};
    while (cursor < index.size && len + 1 < capacity) {
      size_t n = index.size - cursor;
      if (n > sizeof(bytes)) n = sizeof(bytes);
      if (n > capacity - 1 - len) n = capacity - 1 - len;
      if (!index.read(index.context, cursor, bytes, n)) return false;
      for (size_t i = 0; i < n; ++i) {
        const uint8_t ch = bytes[i];
        if (ch == '\n') {
          dst[len] = 0;
          next = cursor + i + 1;
          return true; // A midpoint may land exactly on an LF separator.
        }
        if ((ch < 32 && ch != '\t') || ch > 126) return false;
        dst[len++] = char(ch);
      }
      cursor += n;
    }
    return false;
  }
  static bool decimal(const char* value, size_t& out) {
    if (!value || !*value) return false;
    size_t n = 0;
    for (; *value; ++value) {
      if (*value < '0' || *value > '9') return false;
      const size_t digit = size_t(*value - '0');
      if (n > (MaxFileBytes - digit) / 10) return false;
      n = n * 10 + digit;
    }
    out = n;
    return true;
  }
  static bool readIndexEntry(NotesFile index, size_t begin,
                             char* key, size_t& offset, size_t& next) {
    char line[112]{};
    if (!readIndexLine(index, begin, line, sizeof(line), next)) return false;
    char* tab = strchr(line, '\t');
    if (!tab || strchr(tab + 1, '\t')) return false;
    *tab++ = 0;
    if (!keyValid(line) || !decimal(tab, offset)) return false;
    strcpy(key, line); // keyValid caps at 64, caller provides >=65 bytes.
    return true;
  }
  bool findIndexed(const char* key, JazzBiography& out, NotesFile index,
                   bool& verifiedMiss) const {
    size_t first = 0, tsvBytes = 0, rows = 0;
    uint32_t tsvCrc = 0, indexCrc = 0;
    if (!indexHeader(index, first, tsvBytes, rows, tsvCrc, indexCrc) ||
        tsvBytes != file_.size ||
        rows == 0 || rows > MaxRows || first >= index.size) return false;
    size_t low = first, high = index.size;
    // Invariant: any matching key must start within [low, high).
    for (unsigned attempt = 0; low < high && attempt < 32; ++attempt) {
      const size_t mid = low + (high - low) / 2;
      // Align DOWN to the start of the record containing mid. Aligning up
      // could leave high unchanged when the next record starts at high.
      // Every indexed row is shorter than 112 bytes (64-char key + offset).
      const size_t from = mid > first + 111 ? mid - 111 : first;
      uint8_t preceding[111]{};
      const size_t precedingSize = mid - from;
      if (precedingSize && !index.read(index.context, from, preceding,
                                       precedingSize)) return false;
      size_t start = from;
      if (precedingSize) {
        bool boundary = from == first;
        for (size_t i = precedingSize; i > 0; --i)
          if (preceding[i - 1] == '\n') {
            start = from + i;
            boundary = true;
            break;
          }
        if (!boundary) return false;
      }
      char candidate[65]{};
      size_t offset = 0, after = 0;
      if (!readIndexEntry(index, start, candidate, offset, after)) return false;
      const int cmp = strcmp(candidate, key);
      if (cmp < 0) { low = after; continue; }
      if (cmp > 0) { high = start; continue; }
      if (offset >= file_.size) return false;
      // Validate an actual TSV row boundary and its exact key. This prevents
      // stale offsets and incorrectly sorted index rows from inventing a hit.
      uint8_t before = 0;
      if (offset && (!file_.read(file_.context, offset - 1, &before, 1) ||
                     before != '\n')) return false;
      char row[MaxLineBytes]{};
      size_t end = 0;
      if (!readIndexLine(file_, offset, row, sizeof(row), end) ||
          !parse(row, out) || strcmp(out.key, key)) return false;
      return true;
    }
    if (low == high) verifiedMiss = true;
    return false;
  }
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
