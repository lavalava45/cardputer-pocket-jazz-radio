#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/AudioRuntimeDiagnostics.h"
#include <cassert>
#include <thread>
using namespace pocketjazz;
int main() {
  AudioRuntimeProbe p;
  p.enter(AudioStage::DECODE, UINT32_MAX - 50);
  {
    AudioRuntimeProbe::Scope flush(&p, AudioStage::PCM_FLUSH, 20, 1);
    {
      AudioRuntimeProbe::Scope wait(&p, AudioStage::PCM_FLUSH, 30, 2);
      AudioRuntimeSnapshot s;
      assert(p.snapshot(s) && s.detail == 2 && s.since == 30);
      // A blocked writer does not need to advance for UI to measure its age.
      assert(uint32_t(1030 - s.since) == 1000);
    }
    AudioRuntimeSnapshot s;
    assert(p.snapshot(s) && s.detail == 1 && s.since == 20);
  }
  AudioRuntimeSnapshot s;
  assert(p.snapshot(s) && s.stage == AudioStage::DECODE);
  assert(uint32_t(49 - s.since) == 100); // millis rollover
  p.beginAttempt(5, 90); p.setHop(2); p.enter(AudioStage::TCP_TLS, 100);
  p.fault(150, 1, -1, -9984, 5000, 1200, 30000, 8000);
  p.enter(AudioStage::DNS, 900); // must not erase completed failure
  AudioFaultSnapshot f;
  assert(p.lastFault(f) && f.serial == 1 && f.tls == -9984);
  assert(f.at - f.since == 50 && f.stage == AudioStage::TCP_TLS);
  assert(f.freeHeap == 5000 && f.largest == 1200);
  assert(f.attempt == 5 && f.hop == 2 && f.freeBefore == 30000 && f.largestBefore == 8000);
  for (unsigned n = 0; n < 20; ++n) p.fault(1000+n, 1, -1, -int(n), 5000-n, 1000);
  assert(p.faultCount() == 21);
  assert(!p.readFault(5, f)); // overwritten; caller reports loss, never stale data
  assert(p.readFault(6, f) && f.at == 1004 && f.tls == -4);
  assert(p.readFault(21, f) && f.at == 1019 && f.tls == -19);
  AudioRuntimeJournal journal;
  for (uint32_t n = 0; n < 20; ++n) { AudioRuntimeEvent e; e.at = n; journal.push(e); }
  assert(journal.count() == 16 && journal.newest()->at == 19);
  assert(journal.newest(15)->at == 4 && !journal.newest(16));
  // Real concurrent reader: no mixed stage/since pairs and no blocking reads.
  p.enter(AudioStage::DNS, 1);
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (unsigned n = 0; n < 50000; ++n) {
      p.enter(AudioStage::DNS, 1); p.enter(AudioStage::TCP_TLS, 2);
    }
    done.store(true);
  });
  while (!done.load()) if (p.snapshot(s))
    assert((s.stage == AudioStage::DNS && s.since == 1) ||
           (s.stage == AudioStage::TCP_TLS && s.since == 2));
  writer.join();
  // Fake blocked PLAYER transport concurrent with an unattached PEEK client.
  AudioRuntimeProbe player;
  player.beginAttempt(7, 100); player.setHop(3);
  std::atomic<bool> blocked{false}, release{false};
  std::thread transport([&] {
    player.enter(AudioStage::DNS, 110);
    player.enter(AudioStage::TCP_TLS, 120);
    blocked.store(true);
    while (!release.load()) std::this_thread::yield();
    player.enter(AudioStage::HTTP, 5000);
  });
  while (!blocked.load()) std::this_thread::yield();
  AudioRuntimeProbe* peek = nullptr; // same opt-in contract as SecureICYSource
  for (unsigned n = 0; n < 100; ++n) {
    if (peek) peek->enter(AudioStage::DNS, 1000+n);
    assert(player.snapshot(s) && s.stage == AudioStage::TCP_TLS);
    assert(s.attempt == 7 && s.hop == 3 && uint32_t(4120-s.since) == 4000);
  }
  release.store(true); transport.join();
  player.retry(13000, 5000, 9000);
  assert(player.snapshot(s) && s.stage == AudioStage::RETRY && s.retryAt == 13000);
  assert(s.attempt == 7 && s.since == 5000 && s.heartbeat == 9000);
  player.outstanding(3);
  { AudioRuntimeProbe::Scope wait(&player, AudioStage::PCM_FLUSH, 9000, 2);
    player.outstanding(0); }
  assert(player.snapshot(s) && s.outstanding == 0); // scopes must not restore stale busy counts
}
