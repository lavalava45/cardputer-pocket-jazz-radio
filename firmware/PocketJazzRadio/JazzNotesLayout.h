// SPDX-License-Identifier: MIT
// Fixed-memory, pixel-measured Notes pagination for the 240x135 LCD.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace pocketjazz {
struct NotesLayout {
  static constexpr size_t kRows = 7;
  static constexpr size_t kLineBytes = 128;
  char lines[kRows][kLineBytes]{};
  uint8_t count = 0;
  bool more = false;
};

// Width callback uses the real display font, not an assumed character count.
// The reader advances past each rendered character exactly once. Unrendered
// text is explicitly exposed through `more`, and can be reached by paging.
template<class Measure>
NotesLayout layoutNotes(const char* text, size_t page, int maxPixels,
                        Measure width, size_t visibleRows = NotesLayout::kRows) {
  NotesLayout result{};
  if (!text || maxPixels <= 0 || !visibleRows ||
      visibleRows > NotesLayout::kRows) return result;
  size_t pos = 0;
  size_t target = page * visibleRows;
  size_t index = 0;
  while (text[pos] && index < target + visibleRows) {
    char line[NotesLayout::kLineBytes]{};
    size_t used = 0, lastSpace = 0;
    while (text[pos + used] && used < sizeof(line) - 1) {
      const unsigned char c = static_cast<unsigned char>(text[pos + used]);
      size_t bytes = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 :
                     (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
      if (used + bytes >= sizeof(line)) break;
      bool valid = true;
      for (size_t j = 1; j < bytes; ++j)
        if (!text[pos + used + j] ||
            (static_cast<unsigned char>(text[pos + used + j]) & 0xc0) != 0x80)
          valid = false;
      if (!valid) bytes = 1;
      memcpy(line + used, text + pos + used, bytes);
      line[used + bytes] = 0;
      if (width(line) > maxPixels) { line[used] = 0; break; }
      used += bytes;
      if (bytes == 1 && line[used - 1] == ' ') lastSpace = used;
    }
    if (!used) break;
    size_t consumed = used;
    // Never split a long but otherwise displayable word just to fill a
    // previous row more tightly: pagination must preserve the underlying
    // biography text when the lines are joined with spaces.
    if (text[pos + used] && lastSpace) {
      consumed = lastSpace;
      while (lastSpace && line[lastSpace - 1] == ' ') --lastSpace;
      line[lastSpace] = 0;
    }
    if (index >= target) {
      memcpy(result.lines[result.count], line, strlen(line) + 1);
      ++result.count;
    }
    pos += consumed;
    while (text[pos] == ' ') ++pos;
    ++index;
  }
  result.more = text[pos] != 0;
  return result;
}
} // namespace pocketjazz
