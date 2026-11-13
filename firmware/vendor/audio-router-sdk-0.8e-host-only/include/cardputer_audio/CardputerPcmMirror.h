// SPDX-License-Identifier: MIT
// v0.8d SDK host-only, non-owning stereo PCM16 SPSC observation queue.
// Never owns Speaker, calls playRaw, schedules I/O, or controls a decoder.
#ifndef CARDPUTER_AUDIO_CARDPUTER_PCM_MIRROR_H
#define CARDPUTER_AUDIO_CARDPUTER_PCM_MIRROR_H
#include "cardputer_audio/AudioRouter.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>

namespace cardputer_audio {

template <size_t FramesPerBlock = 256, size_t Blocks = 3>
class CardputerPcmMirror final {
 public:
  static_assert(FramesPerBlock == 256, "Monitor splits callbacks into exact <=256-frame blocks");
  static_assert(Blocks >= 2 && Blocks <= 6, "Bounded 2..6 block queue only");
  static_assert(FramesPerBlock * Blocks * 4 <= 6144, "PCM allocation budget");

  struct Stats {
    uint64_t acceptedFrames;
    uint64_t droppedFrames;
    uint64_t acceptedBlocks;
    uint64_t droppedCallbacks;
    uint64_t invalidCallbacks;
    uint64_t staleBlocks;
    uint64_t flushedBlocks;
  };

  CardputerPcmMirror() : head_(0), tail_(0), active_(false),
      exhausted_(false), epoch_(0), highestEpoch_(0), format_{0,0},
      acceptedFrames_(0), droppedFrames_(0), acceptedBlocks_(0),
      droppedCallbacks_(0), invalidCallbacks_(0), staleBlocks_(0), flushedBlocks_(0) {}
  CardputerPcmMirror(const CardputerPcmMirror&) = delete;
  CardputerPcmMirror& operator=(const CardputerPcmMirror&) = delete;

  // CONTROL/QUIESCENCE ONLY: No observeAccepted or take in flight; producer
  // and consumer stopped. Epoch must strictly increase across station changes
  // and reconnections, preventing re-acceptance of stale callback data.
  bool beginQuiescent(Format format, uint32_t newEpoch) {
    if (active_ || !supported(format) || newEpoch == 0 ||
        newEpoch <= highestEpoch_ || highestEpoch_ == UINT32_MAX) return false;
    discardQuiescent();
    exhausted_ = false;
    format_ = format;
    epoch_ = highestEpoch_ = newEpoch;
    active_ = true;
    return true;
  }

  // CONTROL/QUIESCENCE ONLY: Previous callbacks and consumer MUST be stopped.
  // Unconsumed mirror samples are discarded, never replayed after reconnect.
  void stopQuiescent() {
    active_ = false;
    epoch_ = 0;
    format_ = {0,0};
    discardQuiescent();
  }

  // Producer only, after *the existing* AudioOutputM5Speaker::playRaw returned
  // success. Copies exact interleaved stereo samples immediately. Not proof
  // the speaker actually played them. No blocking I/O, heap, extra downmix,
  // hardware callback changes or second Speaker owner. At most 512 frames
  // per call, split into <=256 chunks; all-or-nothing on queue capacity.
  bool observeAccepted(const int16_t* interleaved, size_t frames,
                       Format inputFormat, uint32_t inputEpoch) {
    if (!active_ || exhausted_ || !interleaved || !frames || frames > 512 ||
        !sameFormat(inputFormat, format_) || inputEpoch != epoch_) {
      invalidCallbacks_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    const uint32_t required = uint32_t((frames + FramesPerBlock - 1) / FramesPerBlock);
    const uint32_t head = head_.load(std::memory_order_relaxed);
    // A 2^32 wrap would shift physical slot alignment whenever Blocks does
    // not divide 2^32 (notably the default Blocks=3). Fail closed and demand
    // a quiescent stop/new epoch before any wrap instead of aliasing slots.
    if (head > UINT32_MAX - required) {
      exhausted_ = true;
      droppedCallbacks_.fetch_add(1, std::memory_order_relaxed);
      droppedFrames_.fetch_add(frames, std::memory_order_relaxed);
      return false;
    }
    if (head - tail_.load(std::memory_order_acquire) > Blocks - required) {
      droppedCallbacks_.fetch_add(1, std::memory_order_relaxed);
      droppedFrames_.fetch_add(frames, std::memory_order_relaxed);
      return false;
    }
    size_t offset = 0;
    for (uint32_t n = 0; n < required; ++n) {
      Entry& entry = blocks_[(head + n) % Blocks];
      const size_t batch = (frames - offset < FramesPerBlock) ? frames - offset : FramesPerBlock;
      entry.frames = static_cast<uint16_t>(batch);
      entry.epoch = inputEpoch;
      entry.sampleRate = inputFormat.sample_rate;
      entry.channels = inputFormat.channels;
      std::memcpy(entry.pcm, interleaved + offset * 2, batch * 2 * sizeof(int16_t));
      offset += batch;
    }
    // Publish entire callback as one contiguous generation-safe batch.
    head_.store(head + required, std::memory_order_release);
    acceptedBlocks_.fetch_add(required, std::memory_order_relaxed);
    acceptedFrames_.fetch_add(frames, std::memory_order_relaxed);
    return true;
  }

  // Consumer only. Copies at most one block. Caller must hold a stable
  // generation, and should stop the consumer before control changes. The
  // caller's PCM output is unchanged if a block is absent/invalid/stale.
  bool take(uint32_t expectedEpoch, Format expectedFormat,
            int16_t* output, size_t capacityFrames, size_t& receivedFrames) {
    receivedFrames = 0;
    if (!active_ || !output || !expectedEpoch || expectedEpoch != epoch_ ||
        !sameFormat(expectedFormat, format_)) return false;
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) return false;
    const Entry& entry = blocks_[tail % Blocks];
    if (entry.epoch != expectedEpoch || entry.sampleRate != expectedFormat.sample_rate ||
        entry.channels != expectedFormat.channels || entry.frames == 0 ||
        entry.frames > FramesPerBlock) {
      tail_.store(tail + 1, std::memory_order_release);
      staleBlocks_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    if (capacityFrames < entry.frames) return false;
    std::memcpy(output, entry.pcm, entry.frames * 2 * sizeof(int16_t));
    receivedFrames = entry.frames;
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  bool active() const { return active_; } // control/quiescent inspection only
  uint32_t epoch() const { return epoch_; } // same-thread/quiescent inspection
  size_t pendingBlocks() const {
    return static_cast<size_t>(head_.load(std::memory_order_acquire) -
                               tail_.load(std::memory_order_acquire));
  }
  Stats stats() const {
    return {acceptedFrames_.load(std::memory_order_relaxed),
            droppedFrames_.load(std::memory_order_relaxed),
            acceptedBlocks_.load(std::memory_order_relaxed),
            droppedCallbacks_.load(std::memory_order_relaxed),
            invalidCallbacks_.load(std::memory_order_relaxed),
            staleBlocks_.load(std::memory_order_relaxed),
            flushedBlocks_.load(std::memory_order_relaxed)};
  }

#ifdef CARDPUTER_PCM_MIRROR_TESTING
  // Host-only wrap fixture, never compiled into firmware.
  bool seedEmptyCountersQuiescentForTest(uint32_t value) {
    if (!active_ || pendingBlocks()) return false;
    head_.store(value, std::memory_order_relaxed);
    tail_.store(value, std::memory_order_relaxed);
    return true;
  }
#endif

 private:
  struct Entry {
    int16_t pcm[FramesPerBlock * 2];
    uint32_t sampleRate;
    uint32_t epoch;
    uint16_t frames;
    uint8_t channels;
  };
  static bool sameFormat(Format a, Format b) {
    return a.sample_rate == b.sample_rate && a.channels == b.channels;
  }
  static bool supported(Format format) {
    return format.channels == 2 && format.sample_rate >= 8000 && format.sample_rate <= 96000;
  }
  void discardQuiescent() {
    flushedBlocks_.fetch_add(pendingBlocks(), std::memory_order_relaxed);
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }
  Entry blocks_[Blocks];
  std::atomic<uint32_t> head_, tail_;
  bool active_;
  bool exhausted_;
  uint32_t epoch_, highestEpoch_;
  Format format_;
  std::atomic<uint64_t> acceptedFrames_, droppedFrames_, acceptedBlocks_;
  std::atomic<uint64_t> droppedCallbacks_, invalidCallbacks_, staleBlocks_, flushedBlocks_;
};

} // namespace cardputer_audio
#endif
