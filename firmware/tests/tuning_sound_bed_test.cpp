#include "../PocketJazzRadio/TuningSoundBed.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>

using namespace pocketjazz::tuning;
int main() {
  static_assert(kRateHz == 8000, "mono low-cost output");
  static_assert(kChannel != 0, "never replace the MP3 speaker channel");
  static_assert(kSamples == 8192, "loop length is fixed and bounded");
  static_assert(kQuietChannelVolume <= 8, "prevent loud hiss regression");
  assert(channelVolumeForMaster(0) == 0);
  assert(channelVolumeForMaster(1) == 1);
  assert(channelVolumeForMaster(57) <= 3);
  assert(channelVolumeForMaster(100) < channelVolumeForMaster(185));
  assert(channelVolumeForMaster(185) == 8);
  assert(channelVolumeForMaster(255) == 8);
  assert(selectClip(false, false) == nullptr);
  assert(selectClip(true, false) == kCrackleOnly.data());
  assert(selectClip(false, true) == kHissOnly.data());
  assert(selectClip(true, true) == kCrackleAndHiss.data());
  int hissNonZero=0, clickNonZero=0, bothNonZero=0;
  for (size_t i=0; i<kSamples; ++i) {
    const int h = kHissOnly[i];
    const int c = kCrackleOnly[i];
    const int b = kCrackleAndHiss[i];
    assert(std::abs(h) <= 2);
    assert(std::abs(c) <= 20);
    assert(std::abs(b) <= 22);
    assert(static_cast<int>(h+c)==b);
    hissNonZero += (h!=0);
    clickNonZero += (c!=0);
    bothNonZero += (b!=0);
  }
  assert(hissNonZero > 2000 && hissNonZero < kSamples);
  assert(clickNonZero > 0 && clickNonZero <= 72);
  assert(bothNonZero >= hissNonZero);
  return 0;
}
