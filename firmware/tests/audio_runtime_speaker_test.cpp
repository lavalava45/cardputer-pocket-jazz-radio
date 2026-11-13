#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/AudioOutputM5Speaker.h"
#include <cassert>
int main() {
  m5::Speaker_Class speaker;
  AudioOutputM5Speaker output(&speaker);
  pocketjazz::AudioRuntimeProbe p;
  p.enter(pocketjazz::AudioStage::DECODE, millis());
  output.setup(); output.setRuntimeProbe(&p);
  unsigned rejected = 0;
  output.setEnqueueFailureMonitor([](void* ptr) { ++*static_cast<unsigned*>(ptr); }, &rejected);
  int16_t sample[2] = {1,2};
  speaker.accept = false;
  output.ConsumeSample(sample); output.flush();
  assert(rejected == 1 && output.getFrames() == 0);
  speaker.accept = true;
  bool observed = false;
  delayHook() = [&] {
    pocketjazz::AudioRuntimeSnapshot s;
    assert(p.snapshot(s) && s.stage == pocketjazz::AudioStage::PCM_FLUSH);
    assert(s.detail == 2 || s.detail == 3);
    observed = true;
    if (millis() >= 100) speaker.release();
  };
  output.ConsumeSample(sample); output.flush();
  output.stop(); // a delayed release must remain observable from another owner
  assert(observed && millis() >= 100);
  pocketjazz::AudioRuntimeSnapshot s;
  assert(p.snapshot(s) && s.stage == pocketjazz::AudioStage::DECODE);
}
