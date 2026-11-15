// SPDX-License-Identifier: MIT
// Data-driven, bounded, host-testable Jazz Journey; no Arduino, heap or network.
#pragma once
#include "JazzNotesSearch.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace pocketjazz {

struct JazzJourneyLesson {
  uint8_t id = 0, trail = 0, style = 0, correct = 0;
  char title[37]{}, era[29]{}, story[151]{}, artist[65]{};
  char listen[131]{}, question[105]{};
  char choice[3][43]{}, sparkTopic[43]{}, source[209]{};
};
struct JazzListeningTip {
  uint8_t id = 0, lesson = 0;
  char title[37]{}, prompt[141]{}, source[209]{};
};

class JazzJourneyCatalog {
 public:
  static constexpr uint8_t kLessons = 24;
  static constexpr uint8_t kTips = 30;
  static constexpr uint8_t kTrails = 4;
  static constexpr uint8_t kStyles = 11;
  static constexpr size_t kMaxCourseBytes = 32 * 1024;
  static constexpr size_t kMaxTipsBytes = 16 * 1024;

  bool openLessons(NotesFile file) {
    lessons_ = {};
    if (!validFile(file, kMaxCourseBytes)) return false;
    uint8_t count = 0;
    if (!scan<JazzJourneyLesson>(file, -1, nullptr, count, decodeLesson) ||
        count != kLessons) return false;
    lessons_ = file;
    return true;
  }
  bool openTips(NotesFile file) {
    tips_ = {};
    if (!validFile(file, kMaxTipsBytes)) return false;
    uint8_t count = 0;
    if (!scan<JazzListeningTip>(file, -1, nullptr, count, decodeTip) ||
        count != kTips) return false;
    tips_ = file;
    return true;
  }
  bool lesson(uint8_t id, JazzJourneyLesson& out) const {
    return readLesson(lessons_, id, out);
  }
  static bool readLesson(NotesFile file, uint8_t id, JazzJourneyLesson& out) {
    if (!validFile(file, kMaxCourseBytes) || id >= kLessons) return false;
    uint8_t count = 0;
    return scan<JazzJourneyLesson>(file, id, &out, count, decodeLesson) &&
           count == kLessons;
  }
  bool tip(uint8_t id, JazzListeningTip& out) const {
    return readTip(tips_, id, out);
  }
  static bool readTip(NotesFile file, uint8_t id, JazzListeningTip& out) {
    if (!validFile(file, kMaxTipsBytes) || id >= kTips) return false;
    uint8_t count = 0;
    return scan<JazzListeningTip>(file, id, &out, count, decodeTip) &&
           count == kTips;
  }

  static const char* trailName(uint8_t id) {
    static const char* const names[] = {
      "ROOTS & VOICES", "SWING & STANDARDS",
      "MODERN LANGUAGE", "BEYOND BORDERS"
    };
    return id < kTrails ? names[id] : "";
  }
  static const char* styleName(uint8_t id) {
    static const char* const names[] = {
      "Roots", "Swing", "Bebop", "Cool",
      "Hard bop", "Modal", "Free jazz", "Latin jazz",
      "Bossa nova", "Fusion", "Modern jazz"
    };
    return id < kStyles ? names[id] : "";
  }
  // This small lookup must agree with the verified data generator.
  static uint8_t styleForLesson(uint8_t id) {
    static const uint8_t ids[kLessons] = {
      0,0,0,0,0,0, 1,1,1,1,1,1, 2,2,2,3,4,5, 6,5,7,8,9,10
    };
    return id < kLessons ? ids[id] : kStyles;
  }

 private:
  static bool validFile(NotesFile f, size_t max) {
    return f.read && f.size && f.size <= max;
  }
  static bool field(char* dst, size_t cap, const char* src) {
    const size_t len = strlen(src);
    if (!len || len >= cap) return false;
    memcpy(dst, src, len + 1);
    return true;
  }
  static bool number(const char* p, uint8_t max, uint8_t& out) {
    if (!p || !*p) return false;
    uint32_t value = 0;
    for (; *p; ++p) {
      if (*p < '0' || *p > '9') return false;
      value = value * 10 + uint32_t(*p - '0');
      if (value > max) return false;
    }
    out = uint8_t(value);
    return true;
  }
  static bool fields(char* line, char** out, size_t count) {
    out[0] = line;
    size_t n = 1;
    for (char* p = line; *p; ++p) if (*p == '\t') {
      if (n >= count) return false;
      *p = 0;
      out[n++] = p + 1;
    }
    return n == count;
  }
  static bool decodeLesson(char* line, uint8_t ordinal, JazzJourneyLesson& v) {
    char* f[15]{};
    if (!fields(line, f, 15) ||
        !number(f[0], kLessons-1, v.id) || v.id != ordinal ||
        !number(f[1], kTrails-1, v.trail) || v.trail != ordinal/6 ||
        !number(f[2], kStyles-1, v.style) ||
        v.style != styleForLesson(v.id) ||
        !number(f[12], 3, v.correct) || v.correct == 0) return false;
    if (!field(v.title, sizeof(v.title), f[3]) ||
        !field(v.era, sizeof(v.era), f[4]) ||
        !field(v.story, sizeof(v.story), f[5]) ||
        !field(v.artist, sizeof(v.artist), f[6]) ||
        !field(v.listen, sizeof(v.listen), f[7]) ||
        !field(v.question, sizeof(v.question), f[8]) ||
        !field(v.choice[0], sizeof(v.choice[0]), f[9]) ||
        !field(v.choice[1], sizeof(v.choice[1]), f[10]) ||
        !field(v.choice[2], sizeof(v.choice[2]), f[11]) ||
        !field(v.sparkTopic, sizeof(v.sparkTopic), f[13]) ||
        !field(v.source, sizeof(v.source), f[14])) return false;
    return strncmp(v.source, "https://", 8) == 0;
  }
  static bool decodeTip(char* line, uint8_t ordinal, JazzListeningTip& v) {
    char* f[5]{};
    if (!fields(line, f, 5) ||
        !number(f[0], kTips-1, v.id) || v.id != ordinal ||
        !number(f[1], kLessons-1, v.lesson) ||
        !field(v.title, sizeof(v.title), f[2]) ||
        !field(v.prompt, sizeof(v.prompt), f[3]) ||
        !field(v.source, sizeof(v.source), f[4])) return false;
    return strncmp(v.source, "https://", 8) == 0;
  }

  // Validate ALL rows even if a requested row was found early. Unknown or
  // malformed SD data fails closed, never silently presents partial lessons.
  template<class Record>
  static bool scan(NotesFile f, int want, Record* out, uint8_t& total,
                   bool (*decode)(char*, uint8_t, Record&)) {
    total = 0;
    char line[832]{};
    uint8_t cache[256]{};
    size_t offset = 0, used = 0, cached = 0, pos = 0;
    // The selected record is copied into the caller-owned result buffer.
    // No second 900-byte lesson sits on the constrained UI stack.
    Record candidate{};
    bool found = false;
    while (offset < f.size) {
      if (pos == cached) {
        cached = f.size - offset;
        if (cached > sizeof(cache)) cached = sizeof(cache);
        if (!f.read(f.context, offset, cache, cached)) return false;
        pos = 0;
      }
      const uint8_t ch = cache[pos++];
      ++offset;
      if (ch == '\n') {
        if (!used || total == 255) return false;
        line[used] = 0;
        if (!decode(line, total, candidate)) return false;
        if (want == total && out) { *out = candidate; found = true; }
        ++total;
        used = 0;
      } else {
        if ((ch < 32 && ch != '\t') || ch > 126 ||
            used + 1 >= sizeof(line)) return false;
        line[used++] = char(ch);
      }
    }
    if (used || (want >= 0 && !found)) return false;
    return total > 0;
  }
  NotesFile lessons_{}, tips_{};
};

class JazzJourneyProgress {
 public:
  static constexpr uint32_t kValidMask = (1u << JazzJourneyCatalog::kLessons) - 1u;
  uint32_t learned = 0, heard = 0;
  bool dirty = false;

  bool markLearned(uint8_t id) { return mark(learned, id); }
  bool markHeard(uint8_t id) { return mark(heard, id); }
  bool learnedLesson(uint8_t id) const {
    return id < JazzJourneyCatalog::kLessons &&
           (learned & (1u << id)) != 0;
  }
  bool heardLesson(uint8_t id) const {
    return id < JazzJourneyCatalog::kLessons &&
           (heard & (1u << id)) != 0;
  }
  uint16_t styles(uint32_t mask) const {
    uint16_t result = 0;
    for (uint8_t i = 0; i < JazzJourneyCatalog::kLessons; ++i)
      if (mask & (1u << i))
        result |= uint16_t(1u << JazzJourneyCatalog::styleForLesson(i));
    return result;
  }
  uint8_t completedCount() const {
    uint8_t count = 0;
    for (uint8_t i=0; i<JazzJourneyCatalog::kLessons; ++i)
      if (learnedLesson(i)) ++count;
    return count;
  }

  static uint32_t checksum(uint32_t a, uint32_t b) {
    uint32_t h = 2166136261u;
    for (int j = 0; j < 8; ++j) {
      const uint8_t c = j < 4 ? uint8_t(a >> (8*j))
                               : uint8_t(b >> (8*(j-4)));
      h = (h ^ c) * 16777619u;
    }
    return h ^ 0x4a5a5031u;  // "JZP1" record domain separator.
  }
  bool serialize(char* out, size_t cap) const {
    if (!out || cap < 33) return false;
    const int n = snprintf(out, cap, "JZP1\t%08lX\t%08lX\t%08lX\n",
                           (unsigned long)learned, (unsigned long)heard,
                           (unsigned long)checksum(learned, heard));
    return n > 0 && static_cast<size_t>(n) < cap;
  }
  bool parse(const char* row) {
    if (!row || strncmp(row, "JZP1\t", 5)) return false;
    const size_t len = strlen(row);
    if (len != 32 || row[13] != '\t' || row[22] != '\t' ||
        row[31] != '\n') return false;
    uint32_t a=0,b=0,c=0;
    if (!hex8(row+5,a) || !hex8(row+14,b) || !hex8(row+23,c) ||
        (a & ~kValidMask) || (b & ~kValidMask) ||
        checksum(a,b) != c) return false;
    learned = a; heard = b; dirty = false;
    return true;
  }
 private:
  bool mark(uint32_t& target, uint8_t id) {
    if (id >= JazzJourneyCatalog::kLessons) return false;
    const uint32_t bit = 1u << id;
    if (target & bit) return false;
    target |= bit; dirty = true;
    return true;
  }
  static bool hex8(const char* s, uint32_t& out) {
    uint32_t v=0;
    for(int i=0;i<8;++i) {
      const char c=s[i];
      uint8_t digit=0;
      if (c>='0' && c<='9') digit=uint8_t(c-'0');
      else if (c>='A' && c<='F') digit=uint8_t(c-'A'+10);
      else return false;
      v=(v<<4)|digit;
    }
    out=v; return true;
  }
};
} // namespace pocketjazz
