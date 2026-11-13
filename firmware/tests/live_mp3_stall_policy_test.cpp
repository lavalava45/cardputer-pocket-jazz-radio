#ifdef NDEBUG
#undef NDEBUG // Enforce assertions even in MSVC Release gate.
#endif
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioLiveStallPolicy.h"
#include <cassert>
#include <cstdint>
#include <initializer_list>

int main() {
  AudioLiveStallPolicy policy;
  // Defaults to zero: upstream finite files and ordinary MP3 users unchanged.
  assert(!policy.shouldWait(100, true, 0, 0));
  policy.setTimeoutMs(4000);
  // A transient 501-ms gap is not EOF on a still-open live stream.
  assert(policy.shouldWait(100, true, 0, 0));
  assert(policy.waiting());
  assert(policy.shouldWait(601, true, 0, 0));
  assert(policy.shouldWait(4099, true, 0, 0));
  assert(!policy.shouldWait(4100, true, 0, 0));
  policy.reset();
  // Unknown Content-Length may be 0 or UINT32_MAX. A live response with
  // finite Content-Length and pos < size must NOT be treated as immediate EOF.
  assert(policy.shouldWait(5000, true, UINT32_MAX, 500));
  assert(policy.shouldWait(5001, true, 100000, 200));
  assert(policy.shouldWait(8999, true, 100000, 200));
  assert(!policy.shouldWait(9000, true, 100000, 200));
  assert(!policy.shouldWait(9001, true, 100000, 100000)); // true EOF
  assert(!policy.waiting());
  assert(!policy.shouldWait(9002, false, 100000, 200));  // disconnect
  assert(!policy.waiting());
  // A new finite source can begin waiting, but each received byte clears
  // the shortage; being one byte short of the end is still not EOF.
  assert(policy.shouldWait(9200, true, 100000, 99999));
  policy.progress();
  assert(!policy.waiting());
  assert(!policy.shouldWait(9201, true, 100000, 100001)); // position overshoot
  // Every actual byte of forward progress starts a new no-data deadline.
  assert(policy.shouldWait(6000, true, 0, 0));
  assert(policy.shouldWait(7500, true, 0, 0));
  policy.progress();
  assert(!policy.waiting());
  assert(policy.shouldWait(10000, true, 0, 0));
  assert(policy.shouldWait(13999, true, 0, 0));
  assert(!policy.shouldWait(14000, true, 0, 0));
  // Safe across the 32-bit Arduino millis() wraparound.
  policy.reset();
  assert(policy.shouldWait(UINT32_MAX - 100, true, 0, 0));
  assert(policy.shouldWait(1000, true, 0, 0));
  assert(!policy.shouldWait(3899, true, 0, 0));
  // Byte-level progress must NOT perpetually reset a missing-frame deadline.
  AudioLiveFrameWatchdog frame;
  frame.setDeadlineMs(12000);
  frame.start(1000);
  for (uint32_t at : {3500u, 7000u, 10999u}) {
    policy.progress(); // a trickle of *unusable* bytes
    assert(!frame.expired(at));
  }
  assert(frame.expired(13000));
  frame.decodedFrame(13001); // a genuine playable MPEG frame
  assert(!frame.expired(13002));
  assert(frame.expired(25001));
  frame.setDeadlineMs(0);
  assert(!frame.expired(UINT32_MAX)); // upstream callers remain unchanged
  frame.setDeadlineMs(12000);
  frame.start(UINT32_MAX - 150);
  assert(!frame.expired(10000));
  assert(frame.expired(11849));
  return 0;
}
