// Per-open compressed MP3 buffer statistics. Pure host-testable policy.
#pragma once
#include <stdint.h>

namespace pocketjazz {
class RadioBufferLevelPolicy {
 public:
  void reset() { current_ = 0; minimum_ = 0; minAtMs_ = 0; valid_ = false; }
  void observe(uint32_t bytes, uint32_t nowMs) {
    current_ = bytes;
    if (!valid_ || bytes < minimum_) {
      minimum_ = bytes;
      minAtMs_ = nowMs;
    }
    valid_ = true;
  }
  uint32_t current() const { return current_; }
  uint32_t minimum() const { return valid_ ? minimum_ : 0; }
  uint32_t minAtMs() const { return valid_ ? minAtMs_ : 0; }
  bool valid() const { return valid_; }

 private:
  uint32_t current_ = 0;
  uint32_t minimum_ = 0;
  uint32_t minAtMs_ = 0;
  bool valid_ = false;
};
}  // namespace pocketjazz
