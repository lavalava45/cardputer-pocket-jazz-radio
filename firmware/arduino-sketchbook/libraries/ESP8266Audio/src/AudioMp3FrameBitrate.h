// Track bitrate from successfully decoded MPEG frames, never ICY headers.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <climits>

namespace audio_mp3 {

class FrameBitrateObservation {
 public:
  void reset() { lastBps_ = 0; minBps_ = 0; maxBps_ = 0; frames_ = 0; }

  // libmad's mad_header.bitrate is in bits/second. Ignore absent or
  // implausible values rather than reporting metadata or corrupt headers.
  void decodedFrame(uint32_t bitrateBps) {
    if (bitrateBps < 8000 || bitrateBps > 640000) return;
    lastBps_ = bitrateBps;
    if (!minBps_ || bitrateBps < minBps_) minBps_ = bitrateBps;
    if (bitrateBps > maxBps_) maxBps_ = bitrateBps;
    if (frames_ != UINT32_MAX) ++frames_;
  }

  uint32_t lastBps() const { return lastBps_; }
  uint32_t observedFrames() const { return frames_; }
  // A changing frame bitrate proves VBR was observed. A fixed sample does
  // NOT prove that a source is CBR or that the nominal ICY rate is correct.
  bool observedVbr() const { return frames_ > 1 && minBps_ != maxBps_; }

 private:
  uint32_t lastBps_ = 0;
  uint32_t minBps_ = 0;
  uint32_t maxBps_ = 0;
  uint32_t frames_ = 0;
};

} // namespace audio_mp3
