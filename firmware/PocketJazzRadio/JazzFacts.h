// SPDX-License-Identifier: MIT
// SD-backed jazz facts and historically-attributed quotes (ASCII TSV).
#pragma once
#include "JazzNotesSearch.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace pocketjazz {
struct JazzFact {
  char type[7];       // FACT or QUOTE (only a verified direct quote).
  char text[193];
  char subject[97];
  char source[225];    // For auditing only; never shown on tiny LCD.
};

class JazzFacts {
 public:
  static constexpr size_t MaxFileBytes = 512 * 1024;
  static constexpr size_t MaxLineBytes = 544;
  static constexpr size_t MaxRows = 2048;

  bool open(NotesFile file) {
    count_ = quoteCount_ = 0;
    file_ = file;
    if (!file_.read || !file_.size || file_.size > MaxFileBytes) return false;
    size_t count = 0;
    JazzFact tmp = {};
    if (!walk(-1, tmp, count, &quoteCount_)) return false;
    count_ = count;
    return count_ > 0;
  }
  size_t size() const { return count_; }
  size_t quoteCount() const { return quoteCount_; }
  bool at(size_t index, JazzFact& fact) const {
    if (index >= count_) return false;
    size_t visited = 0;
    return walk(static_cast<int>(index), fact, visited);
  }
  bool choose(uint32_t entropy, JazzFact& fact) const {
    return count_ && at(entropy % count_, fact);
  }

  // Quote order may be anywhere in the TSV, not just five hard-coded final
  // rows. Search by ordinal on demand without caching row offsets in RAM.
  bool quoteAt(size_t ordinal, JazzFact& out, size_t& absoluteIndex) const {
    if (ordinal >= quoteCount_) return false;
    size_t seen = 0, count = 0;
    return walk(-1, out, count, &seen, ordinal, &absoluteIndex);
  }

 private:
  bool walk(int want, JazzFact& out, size_t& count,
            size_t* quotes = nullptr, size_t quoteTarget = size_t(-1),
            size_t* absoluteIndex = nullptr) const {
    count = 0;
    size_t offset = 0, last = 0, pos = 0, available = 0;
    uint8_t cache[512] = {};
    char line[MaxLineBytes] = {};
    size_t length = 0;
    while (offset < file_.size) {
      if (pos == available) {
        available = file_.size - offset;
        if (available > sizeof(cache)) available = sizeof(cache);
        if (!file_.read(file_.context, offset, cache, available)) return false;
        pos = 0;
      }
      const uint8_t c = cache[pos++];
      ++offset;
      if (c == '\n') {
        if (!length || count >= MaxRows) return false;
        line[length] = 0;
        JazzFact current = {};
        if (!decode(line, current)) return false;
        if (quotes && strcmp(current.type, "QUOTE") == 0) {
          if (*quotes == quoteTarget && absoluteIndex) {
            out = current;
            *absoluteIndex = count;
            return true;
          }
          ++*quotes;
        }
        if (want == static_cast<int>(count)) {
          out = current;
          return true;
        }
        ++count;
        length = 0; last = offset;
      } else {
        if (c == '\r' || (c < 32 && c != '\t') ||
            c >= 127 || length + 1 >= sizeof(line)) return false;
        line[length++] = char(c);
      }
    }
    return !length && offset == file_.size && last == offset;
  }
  static bool copy(char* dest, size_t cap, const char* src) {
    const size_t n = strlen(src);
    if (!n || n >= cap) return false;
    memcpy(dest, src, n + 1);
    return true;
  }
  static bool decode(char* line, JazzFact& out) {
    char* f[4] = {line};
    size_t n = 0;
    for (char* p = line; *p; ++p) {
      if (*p == '\t') {
        if (++n >= 4) return false;
        *p = 0; f[n] = p + 1;
      }
    }
    if (n != 3 || (strcmp(f[0], "FACT") && strcmp(f[0], "QUOTE"))) return false;
    return copy(out.type, sizeof(out.type), f[0]) &&
           copy(out.text, sizeof(out.text), f[1]) &&
           copy(out.subject, sizeof(out.subject), f[2]) &&
           copy(out.source, sizeof(out.source), f[3]) &&
           !strncmp(out.source, "https://", 8);
  }
  NotesFile file_ = {};
  size_t count_ = 0;
  size_t quoteCount_ = 0;
};
} // namespace pocketjazz
