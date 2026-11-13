#ifdef NDEBUG
#undef NDEBUG  // Assertions must run in MSVC Release.
#endif
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioSourcePullPolicy.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct MockSource {
  int blockingCalls = 0;
  int cooperativeCalls = 0;
  uint32_t available = 0;
  uint32_t read(void* data, uint32_t len) {
    ++blockingCalls;
    std::memset(data, 0xAB, len);
    return len;
  }
  uint32_t readNonBlock(void* data, uint32_t len) {
    ++cooperativeCalls;
    const uint32_t got = available < len ? available : len;
    if (got) std::memset(data, 0xCD, got);
    available -= got;
    return got;
  }
};
int main() {
  using audio_buffer::pull;
  using audio_buffer::actualShortage;
  using audio_buffer::keepOpportunisticBytes;
  MockSource src{};
  uint8_t data[48]{};
  // Upstream clients use the old blocking path until explicitly enabled.
  assert(pull(&src, data, 20, false) == 20);
  assert(src.blockingCalls == 1 && src.cooperativeCalls == 0);
  assert(!actualShortage(20, 20)); // 0 genuine missing bytes
  // Live Pocket Jazz never enters src.read() on a decoder pull.
  assert(pull(&src, data, 20, true) == 0);
  assert(actualShortage(20, 0));
  assert(src.blockingCalls == 1 && src.cooperativeCalls == 1);
  // A 10-byte fragment without a full MP3 frame is a true shortage; a
  // subsequent 20-byte fragment fully supplied is not an underflow event.
  src.available = 10;
  assert(pull(&src, data, 20, true) == 10);
  assert(actualShortage(20, 10));
  src.available = 20;
  assert(pull(&src, data, 20, true) == 20);
  assert(!actualShortage(20, 20));
  // A drained ring with an immediately available 48-byte network burst
  // must NOT increment the actual-shortage counter.
  src.available = 48;
  assert(pull(&src, data, 48, true) == 48);
  assert(!actualShortage(48, 48));
  assert(src.blockingCalls == 1 && src.cooperativeCalls == 4);
  // A ring refilled immediately AFTER a zero/short decoder pull must retain
  // those bytes. Previously, filled=false caused next read to overwrite them.
  assert(!keepOpportunisticBytes(true, false, 0));
  assert(keepOpportunisticBytes(true, false, 16));
  assert(keepOpportunisticBytes(true, true, 0));
  assert(!keepOpportunisticBytes(false, false, 16)); // default upstream unchanged
  return 0;
}
