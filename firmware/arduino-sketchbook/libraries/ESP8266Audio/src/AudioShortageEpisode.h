/*
 * Pocket Jazz diagnostic policy: count a contiguous audio shortage ONCE.
 * A cooperative nonblocking poll may run hundreds of times during one
 * missing network packet. Reporting every poll distorts the Underruns metric.
 * This is not a sample-level speaker underrun counter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>

namespace audio_buffer {
class ShortageEpisode {
 public:
  bool onRead(uint32_t requested, uint32_t received) {
    if (!requested || received >= requested) {
      starving_ = false;
      return false;
    }
    if (starving_) return false;
    starving_ = true;
    return true;
  }
  void reset() { starving_ = false; }
  bool starving() const { return starving_; }
 private:
  bool starving_ = false;
};
} // namespace audio_buffer
