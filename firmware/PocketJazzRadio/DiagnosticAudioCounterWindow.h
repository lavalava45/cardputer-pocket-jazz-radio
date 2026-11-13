// User-visible audio counters since the last explicit Diagnostics X reset.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace pocketjazz {

// The underlying counters must remain monotonically increasing: ICY peek
// cancellation and the Serial failure reporter compare their raw values.
// The UI stores only baselines, so X never changes audio-worker state.
class DiagnosticAudioCounterWindow {
 public:
  void reset(uint32_t under, uint32_t opens, uint32_t frames) {
    underBase_ = under;
    opensBase_ = opens;
    framesBase_ = frames;
  }

  uint32_t under(uint32_t raw) const { return raw - underBase_; }
  uint32_t opens(uint32_t raw) const { return raw - opensBase_; }
  uint32_t frames(uint32_t raw) const { return raw - framesBase_; }

 private:
  uint32_t underBase_ = 0;
  uint32_t opensBase_ = 0;
  uint32_t framesBase_ = 0;
};

} // namespace pocketjazz
