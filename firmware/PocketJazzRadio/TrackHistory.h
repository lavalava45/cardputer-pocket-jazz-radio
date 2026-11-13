// SPDX-License-Identifier: GPL-3.0-or-later
// RAM-only history of ICY titles observed in the *currently playing* stream.
// A metadata update is NOT proof of the exact start of a musical track.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace pocketjazz {
template <size_t Capacity = 5>
class TrackHistory {
 public:
  static_assert(Capacity > 0 && Capacity <= 16, "Small bounded history only");
  struct Item {
    char station[56]{};
    char title[96]{};
    uint32_t observedAtMs = 0;
  };
  TrackHistory() : count_(0), next_(0), revision_(0) {}

  bool add(const char* station, const char* title, uint32_t observedAtMs) {
    if (!station || !title || !*station || !*title) return false;
    Item entry{};
    copyClean(entry.station, station);
    copyClean(entry.title, title);
    if (!entry.station[0] || !entry.title[0]) return false;
    const Item* previous = newest();
    // Consecutive broadcaster repeats do not create fake extra tracks.
    if (previous && std::strcmp(previous->station, entry.station) == 0 &&
        std::strcmp(previous->title, entry.title) == 0) return false;
    entry.observedAtMs = observedAtMs;
    data_[next_] = entry;
    next_ = (next_ + 1) % Capacity;
    if (count_ < Capacity) ++count_;
    ++revision_;
    return true;
  }
  const Item* newest(size_t older = 0) const {
    if (older >= count_) return nullptr;
    return &data_[(next_ + Capacity - 1 - older) % Capacity];
  }
  size_t size() const { return count_; }
  uint32_t revision() const { return revision_; }
  void clear() { count_ = next_ = 0; ++revision_; }

 private:
  template <size_t N>
  static void copyClean(char (&dest)[N], const char* src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < N; ++i) {
      const unsigned char c = (unsigned char)src[i];
      if (c < 32 || c == 127) continue;
      dest[j++] = (char)c;
    }
    while (j > 0 && dest[j - 1] == ' ') --j;
    dest[j] = 0;
  }
  Item data_[Capacity]{};
  size_t count_;
  size_t next_;
  uint32_t revision_;
};
} // namespace pocketjazz
