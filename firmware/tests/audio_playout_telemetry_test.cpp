#ifdef NDEBUG
#undef NDEBUG // Release CTest must execute every assertion.
#endif
#include "../PocketJazzRadio/AudioPlayoutTelemetry.h"
#include <cassert>
#include <cstdint>

using pocketjazz::AudioPlayoutTelemetry;

int main() {
  AudioPlayoutTelemetry t;
  assert(t.snapshot().stopCount == 0);
  assert(t.snapshot().pcmGapEpisodes == 0);

  t.request(1, 100, true);
  t.command(1);
  t.pcmEnqueued(1, 120, 100, 1000);  // end = 220
  auto s = t.snapshot();
  assert(s.switchCount == 1 && s.switchLastMs == 20);
  t.pcmEnqueued(1, 260, 100, 1000);  // 40-ms gap ignored
  assert(t.snapshot().pcmGapEpisodes == 0);
  t.pcmEnqueued(1, 490, 100, 1000);  // 130-ms gap counted
  s = t.snapshot();
  assert(s.pcmGapEpisodes == 1 && s.pcmGapMaxMs == 130 && s.pcmGapTotalMs == 130);
  t.pcmEnqueued(1, 600, 0, 44100);  // Empty monitor events are not PCM
  t.pcmEnqueued(1, 600, 20, 0);     // Invalid rate is not PCM
  assert(t.snapshot().pcmGapEpisodes == 1);

  // A newer intent supersedes stale queued audio, even before the task accepts
  // the command. Old PCM must not fabricate new station first-sample latency.
  t.request(2, 600, true);
  t.pcmEnqueued(1, 650, 100, 1000);
  assert(t.snapshot().switchCount == 1);
  t.command(2);
  t.pcmEnqueued(1, 700, 100, 1000);
  t.pcmEnqueued(2, 680, 100, 1000);
  assert(t.snapshot().switchCount == 2 && t.snapshot().switchLastMs == 80);

  // Stop latency includes decoder/speaker queue cleanup, not mere receipt.
  t.request(3, 700, false);
  t.command(3);
  t.pcmEnqueued(3, 710, 100, 1000);
  t.stopped(3, 755);
  t.stopped(3, 800);                // duplicate ack ignored
  s = t.snapshot();
  assert(s.stopCount == 1 && s.stopLastMs == 55 && s.stopMaxMs == 55);
  assert(s.stopP50UpperMs == 80);
  assert(s.stopP95UpperMs >= 55 && s.stopP95UpperMs <= 80);
  assert(s.switchP50UpperMs == 20);
  assert(s.switchP95UpperMs >= 80 && s.switchP95UpperMs <= 80);

  // uint32_t millis wraps without negative or enormous elapsed times.
  t.request(4, UINT32_MAX - 20, false);
  t.command(4);
  t.stopped(4, 15);
  assert(t.snapshot().stopLastMs == 36);

  // Explicit reset keeps request generation while discarding counters.
  t.resetCounters();
  s = t.snapshot();
  assert(s.stopCount == 0 && s.switchCount == 0 && s.pcmGapEpisodes == 0);
  t.request(5, 50, true);
  t.command(5);
  t.pcmEnqueued(5, 60, 50, 1000);
  t.pcmEnqueued(5, 200, 50, 1000);
  assert(t.snapshot().pcmGapEpisodes == 0); // 90ms <= 100ms threshold
  t.pcmEnqueued(5, 370, 50, 1000);
  assert(t.snapshot().pcmGapEpisodes == 1);

  // Stall with no future PCM must count an ongoing outage once, not once/tick.
  t.poll(5, 600);
  t.poll(5, 700);
  assert(t.snapshot().pcmGapEpisodes == 2);
  assert(t.snapshot().pcmGapMaxMs >= 280);
  t.pcmEnqueued(5, 705, 50, 1000);
  assert(t.snapshot().pcmGapEpisodes == 2);

  // Intentionally stopped time is NEVER diagnosed as an audio dropout.
  t.request(6, 400, false);
  t.command(6);
  t.stopped(6, 1000000);
  assert(t.snapshot().pcmGapEpisodes == 2);

  // Regression for the real 512-frame @44.1kHz M5.Speaker handoff pattern.
  // Ceil(512*1000/44100)=12ms per block causes >100ms phantom projection
  // after ~260 blocks; it hid a genuine subsequent 120ms shortage.
  AudioPlayoutTelemetry fineClock;
  fineClock.request(11, 990, true);
  fineClock.command(11);
  for (uint32_t i = 0; i < 300; ++i) {
    const uint32_t deliveredAt = 1000 + static_cast<uint32_t>(
        static_cast<uint64_t>(i) * 512u * 1000u / 44100u);
    fineClock.pcmEnqueued(11, deliveredAt, 512, 44100);
  }
  assert(fineClock.snapshot().pcmGapEpisodes == 0);
  const uint32_t nextScheduled = 1000 + static_cast<uint32_t>(
      static_cast<uint64_t>(300) * 512u * 1000u / 44100u);
  fineClock.pcmEnqueued(11, nextScheduled + 120, 512, 44100);
  const auto drift = fineClock.snapshot();
  assert(drift.pcmGapEpisodes == 1);
  assert(drift.pcmGapMaxMs >= 119 && drift.pcmGapMaxMs <= 121);
  assert(drift.pcmGapTotalMs >= 119 && drift.pcmGapTotalMs <= 121);

  // An open 500ms outage gets counted once and total increases only by the
  // new elapsed delta, including the shortage BEFORE intentional STOP.
  AudioPlayoutTelemetry cutOff;
  cutOff.request(1, 100, true);
  cutOff.command(1);
  cutOff.pcmEnqueued(1, 100, 100, 1000); // projected end = 200
  cutOff.poll(1, 700);                    // observed gap = 500
  cutOff.poll(1, 800);                    // additional 100, not 600 again
  assert(cutOff.snapshot().pcmGapEpisodes == 1);
  assert(cutOff.snapshot().pcmGapTotalMs == 600);
  cutOff.request(2, 900, false);
  cutOff.command(2);                      // cut at 900, not cleanup at 1300
  cutOff.stopped(2, 1300);
  assert(cutOff.snapshot().pcmGapEpisodes == 1);
  assert(cutOff.snapshot().pcmGapTotalMs == 700);
  assert(cutOff.snapshot().pcmGapMaxMs == 700);
  assert(cutOff.snapshot().stopLastMs == 400);

  // Clearing counters mid-stream must not fabricate a second first-PCM event.
  AudioPlayoutTelemetry midstream;
  midstream.request(3, 0, true);
  midstream.command(3);
  midstream.pcmEnqueued(3, 100, 100, 1000);
  midstream.resetCounters();
  midstream.pcmEnqueued(3, 200, 100, 1000);
  assert(midstream.snapshot().switchCount == 0);
  assert(midstream.snapshot().pcmGapEpisodes == 0);

  // PCM clock also survives millis() wrap (STOP wrap is exercised above).
  AudioPlayoutTelemetry wrapped;
  wrapped.request(7, UINT32_MAX - 60, true);
  wrapped.command(7);
  wrapped.pcmEnqueued(7, UINT32_MAX - 50, 100, 1000);
  wrapped.pcmEnqueued(7, 60, 100, 1000); // only 11ms late across wrap
  assert(wrapped.snapshot().pcmGapEpisodes == 0);
  wrapped.pcmEnqueued(7, 310, 100, 1000); // 150ms late after second
  assert(wrapped.snapshot().pcmGapEpisodes == 1);
  wrapped.pcmEnqueued(7, 410, UINT32_MAX, 1000); // reject impossible size
  assert(wrapped.snapshot().pcmGapEpisodes == 1);
  return 0;
}
