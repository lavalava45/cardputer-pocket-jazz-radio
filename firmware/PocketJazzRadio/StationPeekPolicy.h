// SPDX-License-Identifier: GPL-3.0-or-later
// Small, allocation-free ICY peek cache and rate/health gate. UI-task only.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace pocketjazz {

template <size_t Slots = 6>
class StationPeekPolicy {
 public:
  static_assert(Slots > 0 && Slots <= 8, "Limit off-channel cache memory");
  static constexpr uint32_t kRefreshMs = 90000;
  static constexpr uint32_t kRetryMs = 45000;
  static constexpr uint32_t kSpacingMs = 10000;
  static constexpr uint32_t kStaleMs = 180000;
  static constexpr uint32_t kMinFreeHeap = 120000;
  static constexpr uint32_t kMinLargestBlock = 80000;

  struct Entry {
    int16_t stationId = -1;
    uint32_t checkedAt = 0;
    bool checked = false;
    bool hasTitle = false;
    char title[96]{};
  };

  StationPeekPolicy() = default;
  bool busy() const { return busy_; }
  int inFlight() const { return inFlight_; }
  uint32_t revision() const { return revision_; }
  uint32_t attempts() const { return attempts_; }
  uint32_t activeGeneration() const { return playlistGeneration_; }
  uint32_t activeRequest() const { return requestId_; }

  const Entry* lookup(int stationId) const {
    for (const auto& item : entries_)
      if (item.stationId == stationId) return &item;
    return nullptr;
  }
  bool fresh(const Entry* entry, uint32_t now) const {
    return entry && entry->checked && entry->hasTitle &&
           (uint32_t)(now - entry->checkedAt) < kStaleMs;
  }

  // The network task MUST NOT be launched unless the caller has checked the
  // current browse screen, Wi-Fi, decoder buffering, underruns, and heap.
  // This gate repeats the memory/rate checks so a misplaced caller fails shut.
  int next(const int* visible, size_t count, int playingId, uint32_t now,
           bool browseOpen, bool networkHealthy, uint32_t freeHeap,
           uint32_t largestBlock) const {
    if (!browseOpen || !networkHealthy || busy_ || !visible || count > 4 ||
        freeHeap < kMinFreeHeap || largestBlock < kMinLargestBlock ||
        (everStarted_ && (uint32_t)(now - startedAt_) < kSpacingMs)) return -1;
    for (size_t pass = 0; pass < count; ++pass) {
      const size_t n = (pass + roundRobin_) % count;
      const int candidate = visible[n];
      if (candidate < 0 || candidate == playingId) continue;
      const Entry* entry = lookup(candidate);
      if (!entry || !entry->checked ||
          (uint32_t)(now - entry->checkedAt) >=
              (entry->hasTitle ? kRefreshMs : kRetryMs)) return candidate;
    }
    return -1;
  }
  // Both generation and request ID must travel across FreeRTOS queues. A
  // recycled stationId by itself is never sufficient to complete a probe.
  bool start(int stationId, uint32_t now, uint32_t generation, uint32_t request) {
    if (busy_ || stationId < 0 || stationId > 63 || !generation || !request) return false;
    busy_ = true;
    inFlight_ = stationId;
    playlistGeneration_ = generation;
    requestId_ = request;
    startedAt_ = now;
    everStarted_ = true;
    ++attempts_;
    return true;
  }
  // A completed probe may return no title: an absent/invalid ICY block is
  // NOT the start of a new track. Keep negative-cache TTL bounded too.
  bool complete(int stationId, const char* title, uint32_t now,
                uint32_t generation, uint32_t request) {
    if (!busy_ || stationId != inFlight_ ||
        generation != playlistGeneration_ || request != requestId_) return false;
    busy_ = false;
    inFlight_ = -1;
    playlistGeneration_ = requestId_ = 0;
    Entry* target = nullptr;
    for (auto& item : entries_) if (item.stationId == stationId) target = &item;
    if (!target) {
      for (auto& item : entries_) if (item.stationId < 0) { target = &item; break; }
    }
    if (!target) { target = &entries_[nextVictim_]; nextVictim_ = (nextVictim_ + 1) % Slots; }
    *target = Entry{};
    target->stationId = (int16_t)stationId;
    target->checkedAt = now;
    target->checked = true;
    if (title && *title) {
      // Walk COMPLETE UTF-8 code points, never checking an excluded byte to
      // decide whether the final included byte is valid. This also refuses
      // malformed endings rather than publishing a dangling lead byte.
      size_t at = 0, used = 0;
      while (title[at] && used < sizeof(target->title) - 1) {
        const uint8_t lead = static_cast<uint8_t>(title[at]);
        const size_t len = lead < 0x80u ? 1 :
            lead >= 0xc2u && lead <= 0xdfu ? 2 :
            lead >= 0xe0u && lead <= 0xefu ? 3 :
            lead >= 0xf0u && lead <= 0xf4u ? 4 : 0;
        if (!len || len > sizeof(target->title) - 1 - used) break;
        uint32_t scalar = lead & (len == 1 ? 0x7fu : len == 2 ? 0x1fu :
                                  len == 3 ? 0x0fu : 0x07u);
        bool valid = true;
        for (size_t i = 1; i < len; ++i) {
          const uint8_t continuation = static_cast<uint8_t>(title[at + i]);
          if (!continuation || (continuation & 0xc0u) != 0x80u) {
            valid = false;
            break;
          }
          scalar = (scalar << 6) | (continuation & 0x3fu);
        }
        if (!valid || (len == 2 && scalar < 0x80u) ||
            (len == 3 && scalar < 0x800u) ||
            (len == 4 && scalar < 0x10000u) ||
            scalar > 0x10ffffu || (scalar >= 0xd800u && scalar <= 0xdfffu)) break;
        std::memcpy(target->title + used, title + at, len);
        used += len;
        at += len;
      }
      target->title[used] = '\0';
      target->hasTitle = used != 0;
    }
    ++revision_;
    return true;
  }
  void cancel() {
    busy_ = false;
    inFlight_ = -1;
    playlistGeneration_ = requestId_ = 0;
    ++revision_;
  }
  void clear() {
    for (auto& item : entries_) item = Entry{};
    cancel();
  }
 private:
  Entry entries_[Slots]{};
  size_t nextVictim_ = 0;
  size_t roundRobin_ = 0;
  bool busy_ = false;
  bool everStarted_ = false;
  int inFlight_ = -1;
  uint32_t playlistGeneration_ = 0;
  uint32_t requestId_ = 0;
  uint32_t startedAt_ = 0;
  uint32_t revision_ = 0;
  uint32_t attempts_ = 0;
};

} // namespace pocketjazz
