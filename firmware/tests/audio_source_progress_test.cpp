#include "../PocketJazzRadio/AudioSourceProgress.h"
#include <cassert>
#include <cstdint>

int main() {
  pocketjazz::AudioSourceProgress progress;
  progress.reset(1000, 8192);
  assert(progress.bytes() == 8192);
  assert(progress.idleMs(1000) == 0);
  progress.observe(1015, 8192);
  assert(progress.idleMs(1015) == 15);
  progress.observe(1120, 9216);
  assert(progress.maximumGapMs() == 120);
  assert(progress.idleMs(1120) == 0);
  progress.observe(2800, 9216); // an artificial 1,680 ms source pause
  assert(progress.idleMs(2800) == 1680);
  progress.observe(2801, 10240);
  assert(progress.maximumGapMs() == 1681);
  progress.reset(3000, 0); // new TLS/ICY attempt without reboot
  assert(progress.maximumGapMs() == 0 && progress.bytes() == 0);
  progress.observe(3100, 4096);
  assert(progress.maximumGapMs() == 100);
  progress.reset(UINT32_MAX - 10, 500);
  progress.observe(11, 900); // millis rollover
  assert(progress.maximumGapMs() == 22);
}
