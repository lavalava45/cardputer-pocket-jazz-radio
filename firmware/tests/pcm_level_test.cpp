#ifdef NDEBUG
#undef NDEBUG // Keep release-build regression checks active.
#endif
#include "../PocketJazzRadio/PCMLevel.h"
#include "../PocketJazzRadio/PrefillBudget.h"
#include <cassert>
#include <cstdint>

int main() {
  using pocketjazz::stereoSourcePeakPercent;
  const int16_t silent[] = {0, 0, 0, 0, 0, 0, 0, 0};
  assert(stereoSourcePeakPercent(silent, 4) == 0);
  const int16_t leftOnly[] = {16000, 0, 16000, 0, 16000, 0, 16000, 0};
  const int16_t rightOnly[] = {0, -16000, 0, -16000, 0, -16000, 0, -16000};
  const int16_t antiPhase[] = {16000, -16000, 16000, -16000, 16000, -16000, 16000, -16000};
  const uint8_t l = stereoSourcePeakPercent(leftOnly, 4);
  const uint8_t r = stereoSourcePeakPercent(rightOnly, 4);
  const uint8_t a = stereoSourcePeakPercent(antiPhase, 4);
  assert(l >= 48 && l <= 50);
  assert(l == r && r == a);
  const int16_t negFull[] = {-32768, 0};
  assert(stereoSourcePeakPercent(negFull, 1) == 100);
  assert(stereoSourcePeakPercent(nullptr, 0) == 0);
  // Explicit regression: slow DNS/TCP/TLS open (3.2-4.5 sec) must NOT count
  // against prefill. A subsequent slow-but-valid MP3 trickle reaches PLAYING.
  const uint32_t handshakes[] = {3200u, 4500u};
  for (uint32_t tlsOpenMs : handshakes) {
    uint32_t prefillStarted = tlsOpenMs + 100;
    uint32_t available = 0;
    uint32_t now = prefillStarted;
    while (pocketjazz::shouldKeepPrefilling(now, prefillStarted, available,
                                             2048, 5000)) {
      available += 128; // 128 bytes every 500ms; no data during handshake
      now += 500;
    }
    assert(pocketjazz::hasPlayablePrefill(available, 512));
    // Under the previous shared openStarted 3.5-second deadline, a 4.5s
    // TLS handshake would consume the entire prefill window.
    assert(now > tlsOpenMs);
  }
  assert(!pocketjazz::shouldKeepPrefilling(6000, 1000, 0, 2048, 5000));
  assert(!pocketjazz::hasPlayablePrefill(511, 512));
  return 0;
}
