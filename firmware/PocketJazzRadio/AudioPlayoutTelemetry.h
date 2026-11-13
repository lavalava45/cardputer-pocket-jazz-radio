// PCM delivery telemetry for the Cardputer ADV; no allocation or blocking I/O.
// SPDX-License-Identifier: GPL-3.0-or-later
// IMPORTANT: a successful M5.Speaker playRaw() enqueue is not a microphone or
// DAC measurement. The gap estimate uses queued PCM frames/rate; hardware DMA
// and acoustic silence require separate physical measurement.
#pragma once

#include <atomic>
#include <cstdint>
#include <climits>

namespace pocketjazz {

struct AudioTelemetrySnapshot {
  uint32_t pcmGapEpisodes = 0;
  uint32_t pcmGapMaxMs = 0;
  uint32_t pcmGapTotalMs = 0;
  uint32_t pcmHandoffFailures = 0;
  uint32_t stopCount = 0;
  uint32_t stopLastMs = 0;
  uint32_t stopMaxMs = 0;
  uint32_t stopP50UpperMs = 0;
  uint32_t stopP95UpperMs = 0;
  uint32_t switchCount = 0;
  uint32_t switchLastMs = 0;
  uint32_t switchMaxMs = 0;
  uint32_t switchP50UpperMs = 0;
  uint32_t switchP95UpperMs = 0;
};

// One writer in audioWorker; UI only publishes request + resets and reads atomics.
class AudioPlayoutTelemetry {
 public:
  static constexpr uint32_t kGapThresholdMs = 100;

  void request(uint32_t sequence, uint32_t nowMs, bool shouldPlay) {
    requestAtMs_.store(nowMs, std::memory_order_relaxed);
    requestPlay_.store(shouldPlay, std::memory_order_relaxed);
    requestSequence_.store(sequence, std::memory_order_release);
  }

  // Audio worker calls on acceptance of a command, before source cleanup.
  void command(uint32_t sequence) {
    syncReset();
    // Close an already observed live shortage at the UI STOP timestamp,
    // excluding all subsequent intentional stop/decoder-drain time.
    if (!requestPlay_.load(std::memory_order_relaxed) && havePcm_ &&
        sequence != audioSequence_)
      accountGapAt(requestAtMs_.load(std::memory_order_relaxed));
    audioSequence_ = sequence;
    havePcm_ = false;
    projectedEndMs_ = 0;
    remainder_ = 0;
    sourceRate_ = 0;
    stopRecorded_ = false;
    gapOpen_ = false;
    gapAccountedMs_ = 0;
  }

  // Called from AudioOutputM5Speaker's accepted enqueue monitor on audioWorker.
  // Ignore old commands superseded by a newer UI request. `rate` must be real
  // decoded PCM sample-rate, never the HTTP header's nominal bitrate.
  void pcmEnqueued(uint32_t sequence, uint32_t nowMs, uint32_t frames, uint32_t rate) {
    if (!frames || frames > 32768 || rate < 1000 || rate > 384000 ||
        sequence != audioSequence_ ||
        sequence != requestSequence_.load(std::memory_order_acquire) ||
        !requestPlay_.load(std::memory_order_relaxed)) return;
    syncReset();
    if (!havePcm_) {
      havePcm_ = true;
      projectedEndMs_ = nowMs;
      remainder_ = 0;
      sourceRate_ = 0;
      const uint32_t elapsed = nowMs - requestAtMs_.load(std::memory_order_relaxed);
      record(switchCount_, switchLastMs_, switchMaxMs_, switchBuckets_, elapsed);
    } else {
      const int32_t late = static_cast<int32_t>(nowMs - projectedEndMs_);
      accountGapAt(nowMs);
      // Quantization on millis() can cause +/-1ms late reports even for
      // perfectly timed blocks. Do not continuously move the expected clock
      // forward for those; otherwise the detector acquires a false drift.
      if (late > 2) {
        projectedEndMs_ = nowMs;
        remainder_ = 0;
      }
      gapOpen_ = false;
      gapAccountedMs_ = 0;
    }
    // Carry fractional milliseconds across 512-frame handoffs. Ceil-per-block
    // at 44.1kHz adds ~0.39ms per call: ~100ms false drift in 260 calls.
    if (sourceRate_ != rate) { sourceRate_ = rate; remainder_ = 0; }
    const uint64_t numerator = static_cast<uint64_t>(frames) * 1000u + remainder_;
    const uint32_t duration = static_cast<uint32_t>(numerator / rate);
    remainder_ = static_cast<uint32_t>(numerator % rate);
    projectedEndMs_ += duration;
  }

  // Also detect a gap still in progress when no subsequent PCM arrives. Called
  // by audioWorker, NOT by an ISR, only while live stream is desired and the
  // task is servicing an already started decoder. No allocation or callbacks.
  void poll(uint32_t sequence, uint32_t nowMs) {
    syncReset();
    if (!havePcm_ || sequence != audioSequence_ ||
        sequence != requestSequence_.load(std::memory_order_acquire) ||
        !requestPlay_.load(std::memory_order_relaxed)) return;
    accountGapAt(nowMs);
  }

  void rejectedPcm() { saturatedIncrement(pcmHandoffFailures_); }

  // Called only AFTER decoder->stop()/speaker.stop() have drained buffers.
  // Buffer release is not proof that the analog speaker has become silent.
  void stopped(uint32_t sequence, uint32_t nowMs) {
    syncReset();
    if (sequence != audioSequence_ ||
        sequence != requestSequence_.load(std::memory_order_acquire) ||
        requestPlay_.load(std::memory_order_relaxed) || stopRecorded_) return;
    stopRecorded_ = true;
    const uint32_t elapsed = nowMs - requestAtMs_.load(std::memory_order_relaxed);
    record(stopCount_, stopLastMs_, stopMaxMs_, stopBuckets_, elapsed);
    havePcm_ = false;
    gapOpen_ = false;
    gapAccountedMs_ = 0;
  }

  // Explicit user action: no accumulated history is silently reset on station
  // switches. Request/sequence control remains intact.
  void resetCounters() {
    pcmGapEpisodes_.store(0); pcmGapMaxMs_.store(0);
    pcmGapTotalMs_.store(0); pcmHandoffFailures_.store(0);
    stopCount_.store(0); stopLastMs_.store(0); stopMaxMs_.store(0);
    switchCount_.store(0); switchLastMs_.store(0); switchMaxMs_.store(0);
    for (auto& b : stopBuckets_) b.store(0);
    for (auto& b : switchBuckets_) b.store(0);
    resetSequence_.fetch_add(1, std::memory_order_relaxed);
  }

  AudioTelemetrySnapshot snapshot() const {
    AudioTelemetrySnapshot s;
    s.pcmGapEpisodes = pcmGapEpisodes_.load();
    s.pcmGapMaxMs = pcmGapMaxMs_.load();
    s.pcmGapTotalMs = pcmGapTotalMs_.load();
    s.pcmHandoffFailures = pcmHandoffFailures_.load();
    s.stopCount = stopCount_.load();
    s.stopLastMs = stopLastMs_.load();
    s.stopMaxMs = stopMaxMs_.load();
    s.stopP50UpperMs = estimateQuantileUpper(stopBuckets_, s.stopCount, 50);
    s.stopP95UpperMs = estimateP95Upper(stopBuckets_, s.stopCount);
    s.switchCount = switchCount_.load();
    s.switchLastMs = switchLastMs_.load();
    s.switchMaxMs = switchMaxMs_.load();
    s.switchP50UpperMs = estimateQuantileUpper(switchBuckets_, s.switchCount, 50);
    s.switchP95UpperMs = estimateP95Upper(switchBuckets_, s.switchCount);
    return s;
  }

 private:
  static constexpr unsigned kBuckets = 12;
  static constexpr uint32_t kBucketUpper[kBuckets] = {
      5, 10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, UINT32_MAX};
  std::atomic<uint32_t> requestSequence_{0};
  std::atomic<uint32_t> requestAtMs_{0};
  std::atomic<bool> requestPlay_{false};
  std::atomic<uint32_t> resetSequence_{0};
  uint32_t audioSequence_ = 0;
  uint32_t audioResetSequence_ = 0;
  uint32_t projectedEndMs_ = 0;
  uint32_t sourceRate_ = 0;
  uint32_t remainder_ = 0;   // numerator remainder in 1/rate Hz ticks
  uint32_t gapAccountedMs_ = 0;
  bool havePcm_ = false;
  bool stopRecorded_ = false;
  bool gapOpen_ = false;
  std::atomic<uint32_t> pcmGapEpisodes_{0}, pcmGapMaxMs_{0}, pcmGapTotalMs_{0};
  std::atomic<uint32_t> pcmHandoffFailures_{0};
  std::atomic<uint32_t> stopCount_{0}, stopLastMs_{0}, stopMaxMs_{0};
  std::atomic<uint32_t> switchCount_{0}, switchLastMs_{0}, switchMaxMs_{0};
  std::atomic<uint32_t> stopBuckets_[kBuckets]{};
  std::atomic<uint32_t> switchBuckets_[kBuckets]{};

  void syncReset() {
    const uint32_t reset = resetSequence_.load(std::memory_order_relaxed);
    if (reset != audioResetSequence_) {
      audioResetSequence_ = reset;
      // Keep the current source clock and first-PCM epoch. Resetting
      // statistics must not fabricate another station switch.
      gapOpen_ = false;
      gapAccountedMs_ = 0;
    }
  }
  void accountGapAt(uint32_t nowMs) {
    if (!havePcm_) return;
    const int32_t late = static_cast<int32_t>(nowMs - projectedEndMs_);
    if (late < static_cast<int32_t>(kGapThresholdMs)) return;
    const uint32_t observed = static_cast<uint32_t>(late);
    if (!gapOpen_) {
      gapOpen_ = true;
      gapAccountedMs_ = 0;
      saturatedIncrement(pcmGapEpisodes_);
    }
    updateMax(pcmGapMaxMs_, observed);
    if (observed > gapAccountedMs_) {
      saturatedAdd(pcmGapTotalMs_, observed - gapAccountedMs_);
      gapAccountedMs_ = observed;
    }
  }

  static void saturatedIncrement(std::atomic<uint32_t>& field) {
    uint32_t old = field.load(std::memory_order_relaxed);
    while (old != UINT32_MAX && !field.compare_exchange_weak(
        old, old + 1, std::memory_order_relaxed)) {}
  }
  static void saturatedAdd(std::atomic<uint32_t>& field, uint32_t amount) {
    uint32_t old = field.load(std::memory_order_relaxed);
    uint32_t next;
    do { next = UINT32_MAX - old < amount ? UINT32_MAX : old + amount; }
    while (!field.compare_exchange_weak(old, next, std::memory_order_relaxed));
  }
  static void updateMax(std::atomic<uint32_t>& field, uint32_t value) {
    uint32_t old = field.load(std::memory_order_relaxed);
    while (old < value && !field.compare_exchange_weak(
        old, value, std::memory_order_relaxed)) {}
  }
  static void record(std::atomic<uint32_t>& count,
                     std::atomic<uint32_t>& last, std::atomic<uint32_t>& max,
                     std::atomic<uint32_t> (&buckets)[kBuckets], uint32_t elapsed) {
    last.store(elapsed, std::memory_order_relaxed);
    updateMax(max, elapsed);
    for (unsigned i = 0; i < kBuckets; ++i) {
      if (elapsed <= kBucketUpper[i]) { saturatedIncrement(buckets[i]); break; }
    }
    saturatedIncrement(count);
  }
  static uint32_t estimateQuantileUpper(const std::atomic<uint32_t> (&buckets)[kBuckets],
                                        uint32_t count, uint32_t percentile) {
    if (!count) return 0;
    const uint32_t target = static_cast<uint32_t>(
        (static_cast<uint64_t>(count) * percentile + 99u) / 100u);
    uint64_t sum = 0;
    for (unsigned i = 0; i < kBuckets; ++i) {
      sum += buckets[i].load(std::memory_order_relaxed);
      if (sum >= target) return kBucketUpper[i];
    }
    return UINT32_MAX;  // a concurrent reset/snapshot, not fabricated 0 ms
  }
  static uint32_t estimateP95Upper(const std::atomic<uint32_t> (&buckets)[kBuckets],
                                    uint32_t count) {
    return estimateQuantileUpper(buckets, count, 95);
  }
};

} // namespace pocketjazz
