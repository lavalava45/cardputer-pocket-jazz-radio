#ifdef NDEBUG
#undef NDEBUG // Assertions MUST execute in the MSVC Release gate.
#endif
#include <cassert>
#include <climits>
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioMp3FrameBitrate.h"

int main() {
  audio_mp3::FrameBitrateObservation probe;
  assert(probe.lastBps() == 0 && !probe.observedVbr());
  probe.decodedFrame(0);
  probe.decodedFrame(7999);
  probe.decodedFrame(640001);
  assert(probe.observedFrames() == 0);
  probe.decodedFrame(128000);
  probe.decodedFrame(128000);
  assert(probe.lastBps() == 128000);
  assert(probe.observedFrames() == 2);
  assert(!probe.observedVbr());
  probe.decodedFrame(192000);
  assert(probe.lastBps() == 192000 && probe.observedVbr());
  probe.decodedFrame(96000);
  assert(probe.lastBps() == 96000 && probe.observedVbr());
  probe.decodedFrame(640000);
  assert(probe.observedFrames() == 5 && probe.observedVbr());
  probe.reset();
  assert(probe.lastBps() == 0 && probe.observedFrames() == 0 && !probe.observedVbr());
  probe.decodedFrame(320000);
  assert(probe.lastBps() == 320000 && !probe.observedVbr());
  return 0;
}
