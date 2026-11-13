// Host policy scenario: stream stalls -> old audio cleans up -> another
// station starts -> the NEW generation accepts PCM. This does NOT claim
// physical TCP/TLS/ESP32 speaker testing; those require on-device acceptance.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/AudioIntentPolicy.h"
#include "../PocketJazzRadio/AudioPlayoutTelemetry.h"
#include "../PocketJazzRadio/PrefillBudget.h"
#include <cassert>
#include <cstdint>

struct FakeAudioResources {
  bool decoder = false;
  bool buffer = false;
  bool tls = false;
  bool ring = false;
  unsigned closeCount = 0;

  void opened() {
    assert(!decoder && !buffer && !tls && !ring);
    tls = ring = buffer = decoder = true;
  }
  void cleanup() {
    // Check close/release contract in the same order as audioWorker:
    // decoder -> buffer -> transport -> codec workspace -> compressed ring.
    if (decoder) { decoder = false; ++closeCount; }
    if (buffer) { buffer = false; ++closeCount; }
    if (tls) { tls = false; ++closeCount; }
    if (ring) { ring = false; ++closeCount; }
  }
  bool empty() const { return !decoder && !buffer && !tls && !ring; }
};

int main() {
  using pocketjazz::audioIntentSuperseded;
  pocketjazz::AudioPlayoutTelemetry telemetry;
  FakeAudioResources resources;

  // Original station A has verified transport and a filled compressed ring.
  uint32_t latest = 1;
  const uint32_t stationA = 1;
  telemetry.request(stationA, 100, true);
  telemetry.command(stationA);
  resources.opened();
  assert(pocketjazz::initialMp3PrefillGoal(16384) == 8192);
  telemetry.pcmEnqueued(stationA, 200, 512, 44100);
  assert(telemetry.snapshot().switchCount == 1);

  // Transport stops delivering bytes. The input-ring/PCM projection drains,
  // but no new station is claimed merely because the UI says CONNECTING.
  telemetry.poll(stationA, 900);
  assert(telemetry.snapshot().pcmGapEpisodes == 1);
  assert(telemetry.snapshot().pcmGapMaxMs >= 600);

  // A deliberate switch/STOP supersedes A, even while A's retry may be due.
  latest = 2;
  telemetry.request(latest, 920, false);
  assert(audioIntentSuperseded(stationA, latest));
  resources.cleanup();
  assert(resources.empty() && resources.closeCount == 4);
  telemetry.command(latest);
  telemetry.stopped(latest, 950);
  assert(telemetry.snapshot().stopCount == 1);

  // New station B is allowed to allocate fresh TLS/ring only AFTER cleanup.
  latest = 3;
  telemetry.request(latest, 1000, true);
  telemetry.command(latest);
  resources.opened();
  // Stale A input can arrive late but may not become accepted PCM on B.
  telemetry.pcmEnqueued(stationA, 1120, 512, 44100);
  assert(telemetry.snapshot().switchCount == 1);
  assert(audioIntentSuperseded(stationA, latest));
  telemetry.pcmEnqueued(latest, 1135, 512, 44100);
  assert(telemetry.snapshot().switchCount == 2);
  assert(telemetry.snapshot().switchLastMs == 135);
  assert(resources.closeCount == 4); // no second cleanup hidden in volume

  // Volume-only mailbox updates retain the generation and never reopen B.
  assert(!audioIntentSuperseded(latest, latest));
  telemetry.pcmEnqueued(latest, 1146, 512, 44100);
  assert(telemetry.snapshot().pcmGapEpisodes == 1);
  return 0;
}
