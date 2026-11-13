// SPDX-License-Identifier: MIT
// One audio writer; UI observes via bounded, short cross-core snapshots.
#pragma once
#include <cstdint>
#include <cstddef>
#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
#include <mutex>
#endif

namespace pocketjazz {
enum class AudioStage : uint32_t {
  IDLE, DNS, TCP_TLS, HTTP, PREFILL, DECODE, PCM_FLUSH, RETRY, STOP, NET_READ, META, OPEN
};
inline const char* audioStageName(AudioStage s) {
  switch (s) {
    case AudioStage::DNS: return "DNS";
    case AudioStage::TCP_TLS: return "TCP/TLS";
    case AudioStage::HTTP: return "HTTP";
    case AudioStage::PREFILL: return "PREFILL";
    case AudioStage::DECODE: return "DECODE";
    case AudioStage::PCM_FLUSH: return "PCM FLUSH";
    case AudioStage::RETRY: return "RETRY";
    case AudioStage::STOP: return "STOP";
    case AudioStage::NET_READ: return "NET READ";
    case AudioStage::META: return "META";
    case AudioStage::OPEN: return "OPEN";
    default: return "IDLE";
  }
}
struct AudioRuntimeSnapshot {
  AudioStage stage = AudioStage::IDLE;
  uint32_t since = 0, heartbeat = 0, detail = 0;
  uint32_t attempt = 0, hop = 0, retryAt = 0, outstanding = 0;
};
struct AudioFaultSnapshot {
  uint32_t serial = 0, at = 0, freeHeap = 0, largest = 0;
  AudioStage stage = AudioStage::IDLE;
  uint32_t since = 0, attempt = 0, hop = 0, freeBefore = 0, largestBefore = 0;
  int32_t failure = 0, http = 0, tls = 0;
};
class AudioRuntimeProbe {
 public:
  // Never called from peek, speaker task, or ISR. ESP32-S3 is built with
  // -mdisable-hardware-atomics; unlike host MSVC, std::atomic<T> is NOT
  // guaranteed lock-free there. A portMUX protects only fixed-size copies,
  // never any network, heap, SD, Serial or speaker operation.
  void enter(AudioStage s, uint32_t now, uint32_t detail = 0) {
    Guard guard(*this);
    state_.stage = s; state_.since = now; state_.heartbeat = now;
    state_.detail = detail;
  }
  void beginAttempt(uint32_t attempt, uint32_t now) {
    Guard guard(*this);
    state_.stage = AudioStage::OPEN;
    state_.since = state_.heartbeat = now;
    state_.detail = state_.hop = state_.retryAt = 0;
    state_.attempt = attempt;
  }
  void setHop(uint32_t hop) { Guard guard(*this); state_.hop = hop; }
  void retry(uint32_t deadline, uint32_t started, uint32_t now) {
    Guard guard(*this);
    state_.stage = AudioStage::RETRY; state_.detail = 0;
    state_.retryAt = deadline; state_.since = started; state_.heartbeat = now;
  }
  void outstanding(uint32_t count) { Guard guard(*this); state_.outstanding = count; }
  void beat(uint32_t now) { Guard guard(*this); state_.heartbeat = now; }
  void fault(uint32_t now, int32_t failure, int32_t http, int32_t tls,
             uint32_t freeHeap, uint32_t largest, uint32_t freeBefore = 0, uint32_t largestBefore = 0) {
    Guard guard(*this);
    AudioFaultSnapshot v;
    v.serial = ++faultSerial_; v.at = now; v.stage = state_.stage;
    v.since = state_.since; v.attempt = state_.attempt; v.hop = state_.hop;
    v.failure = failure; v.http = http; v.tls = tls;
    v.freeHeap = freeHeap; v.largest = largest;
    v.freeBefore = freeBefore; v.largestBefore = largestBefore;
    faults_[v.serial % 16u] = v;
  }
  bool readFault(uint32_t serial, AudioFaultSnapshot& out) const {
    Guard guard(*this);
    const auto& slot = faults_[serial % 16u];
    if (!serial || slot.serial != serial) return false;
    out = slot;
    return true;
  }
  uint32_t faultCount() const { Guard guard(*this); return faultSerial_; }
  bool lastFault(AudioFaultSnapshot& out) const {
    Guard guard(*this);
    if (!faultSerial_) { out = AudioFaultSnapshot{}; return true; }
    out = faults_[faultSerial_ % 16u]; return true;
  }
  bool snapshot(AudioRuntimeSnapshot& out) const {
    Guard guard(*this);
    out = state_;
    return true;
  }
  class Scope {
   public:
    Scope(AudioRuntimeProbe* p, AudioStage stage, uint32_t now, uint32_t detail = 0)
        : probe_(p) {
      if (probe_) { probe_->snapshot(saved_); probe_->enter(stage, now, detail); }
    }
    ~Scope() { if (probe_) probe_->publish(saved_); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
   private:
    AudioRuntimeProbe* probe_;
    AudioRuntimeSnapshot saved_;
  };
 private:
  struct Guard {
    explicit Guard(const AudioRuntimeProbe& p) : p_(p) { p_.lock(); }
    ~Guard() { p_.unlock(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    const AudioRuntimeProbe& p_;
  };
  void lock() const {
#ifdef ARDUINO
    portENTER_CRITICAL(&mux_);
#else
    mux_.lock();
#endif
  }
  void unlock() const {
#ifdef ARDUINO
    portEXIT_CRITICAL(&mux_);
#else
    mux_.unlock();
#endif
  }
#ifdef ARDUINO
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#else
  mutable std::mutex mux_;
#endif
  AudioRuntimeSnapshot state_{};
  AudioFaultSnapshot faults_[16]{};
  uint32_t faultSerial_ = 0;
  void publish(const AudioRuntimeSnapshot& v) {
    Guard guard(*this);
    // Outstanding belongs to the speaker wait and must not be restored by
    // nested stage Scope destructors while callbacks release buffers.
    const uint32_t currentOutstanding = state_.outstanding;
    state_ = v;
    state_.outstanding = currentOutstanding;
  }
};

// UI-owned numeric journal: no URLs, credentials, filesystem, or network I/O.
struct AudioRuntimeEvent {
  uint32_t at = 0, observedAt = 0, stageMs = 0, heartbeatMs = 0, detail = 0;
  AudioStage stage = AudioStage::IDLE;
  uint32_t buffer = 0, freeHeap = 0, largest = 0, opens = 0, frames = 0;
  uint32_t rejects = 0, drops = 0, under = 0, stopMs = 0;
  uint32_t pcmAgeMs = UINT32_MAX;
  uint32_t attempt = 0, hop = 0, retryAt = 0, outstanding = 0;
  uint32_t freeBefore = 0, largestBefore = 0;
  bool inputWaiting = false;
  int32_t rssi = 0, state = 0, failure = 0, http = 0, tls = 0;
  bool wifi = false;
};
class AudioRuntimeJournal {
 public:
  static constexpr size_t capacity = 16;
  void push(const AudioRuntimeEvent& e) {
    events_[next_] = e; next_ = (next_ + 1) % capacity;
    if (count_ < capacity) ++count_;
  }
  const AudioRuntimeEvent* newest(size_t offset = 0) const {
    return offset < count_ ? &events_[(next_ + capacity - 1 - offset) % capacity] : nullptr;
  }
  size_t count() const { return count_; }
 private:
  AudioRuntimeEvent events_[capacity]{};
  size_t next_ = 0, count_ = 0;
};
// The Xtensa GCC toolchain reports conservative ATOMIC_*_LOCK_FREE == 1
// even when these concrete, naturally aligned std::atomic types are always
// lock-free. Test the actual types instead; verified with esp-x32 GCC 14.2.
static_assert(sizeof(AudioRuntimeProbe) <= 1024, "Probe must stay bounded in DRAM");
static_assert(sizeof(AudioRuntimeJournal) <= 2048, "Journal must stay bounded in DRAM");
} // namespace pocketjazz
