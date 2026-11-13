#ifdef NDEBUG
#undef NDEBUG // All assertions are active in the release CTest configuration.
#endif
#include "../PocketJazzRadio/WifiCredentialMigration.h"
#include <cassert>
#include <cstring>
using namespace pocketjazz;

struct FakeAuthenticatedEncryptedStore {
  bool key = true;
  bool committed = false, staged = false, deletedLegacy = false;
  bool restoredByColdBoot = false;
  bool failStage = false, failStagedRead = false, mismatch = false;
  bool failCommit = false, commitButReportFailure = false;
  bool failCommittedRead = false, failPurge = false;
  int stageCalls = 0, commitCalls = 0, purgeCalls = 0;
  int stagedActive = -1, committedActive = -1;
  WifiProfilePolicy stagedProfiles, committedProfiles;
  bool legacyBank0 = true, legacyBank1 = true, legacySingletonPass = true;

  bool hardwareProtected() const { return key; }
  bool recoveredOnColdBoot() const { return restoredByColdBoot; }
  bool hasCommittedRecord() const { return committed; }
  bool stageEncrypted(const WifiProfilePolicy& p, int active) {
    ++stageCalls;
    if (failStage) return false;
    stagedProfiles = p;
    stagedActive = active;
    staged = true;
    return true;
  }
  bool readStaged(WifiProfilePolicy& p, int& active) const {
    if (!staged || failStagedRead) return false;
    p = stagedProfiles; active = stagedActive;
    if (mismatch) p.put("home", "tampered678");
    return true;
  }
  bool commitStaged() {
    ++commitCalls;
    if (failCommit || !staged) return false;
    committedProfiles = stagedProfiles;
    committedActive = stagedActive;
    committed = true;
    return !commitButReportFailure;
  }
  bool readCommitted(WifiProfilePolicy& p, int& active) const {
    if (!committed || failCommittedRead) return false;
    p = committedProfiles; active = committedActive;
    return true;
  }
  bool purgeLegacyPlaintext() {
    ++purgeCalls;
    if (failPurge) return false;
    legacyBank0 = legacyBank1 = legacySingletonPass = false;
    deletedLegacy = true;
    return true;
  }
};

int main() {
  WifiProfilePolicy legacy;
  assert(legacy.put("home", "examplepass123"));
  assert(legacy.put("hotspot", "examplepass456"));
  const int active = 1;
  FakeAuthenticatedEncryptedStore s;
  using Result = WifiCredentialMigrationResult;

  // No key is not a weak-key fallback or permission to write plaintext.
  s.key = false;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::KeyUnavailable);
  assert(s.stageCalls == 0 && !s.committed && s.legacySingletonPass);
  s.key = true;
  assert(WifiCredentialMigration::prepare(s, legacy, -1) == Result::InvalidProfiles);
  assert(WifiCredentialMigration::prepare(s, legacy, 2) == Result::InvalidProfiles);
  assert(s.stageCalls == 0);

  s.failStage = true;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::StagingFailed);
  assert(!s.committed && s.legacyBank0 && s.legacyBank1);
  s.failStage = false;
  s.failStagedRead = true;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::ReadbackFailed);
  assert(!s.committed && s.purgeCalls == 0);
  s.failStagedRead = false;
  s.mismatch = true;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::ReadbackFailed);
  assert(!s.committed && s.purgeCalls == 0);
  s.mismatch = false;

  s.failCommit = true;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::CommitFailed);
  assert(!s.committed && s.legacySingletonPass);
  s.failCommit = false;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::ReadyForColdBoot);
  assert(s.committed && !s.deletedLegacy && s.commitCalls == 2);
  assert(s.committedActive == 1 && sameWifiProfiles(s.committedProfiles, legacy));
  assert(WifiCredentialMigration::afterColdBoot(s, true, true) ==
         Result::AwaitingColdBootProof);
  assert(s.purgeCalls == 0);
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::SecurePresent);
  assert(s.stageCalls == 5); // Existing secure marker never triggers a rewrite.

  // Simulated cold-boot: only committed data survives. No plaintext cleanup
  // until both real network association and explicit user permission.
  FakeAuthenticatedEncryptedStore boot = s;
  boot.staged = false;
  boot.stagedProfiles.clear();
  boot.stagedActive = -1;
  boot.restoredByColdBoot = true;
  assert(WifiCredentialMigration::afterColdBoot(boot, false, true) ==
         Result::AwaitingNetworkProof);
  assert(WifiCredentialMigration::afterColdBoot(boot, true, false) ==
         Result::ReadyForLegacyCleanup);
  assert(boot.purgeCalls == 0 && boot.legacyBank0 &&
         boot.legacyBank1 && boot.legacySingletonPass);

  boot.failPurge = true;
  assert(WifiCredentialMigration::afterColdBoot(boot, true, true) ==
         Result::CleanupFailed);
  assert(boot.legacySingletonPass);
  boot.failPurge = false;
  assert(WifiCredentialMigration::afterColdBoot(boot, true, true) ==
         Result::Migrated);
  assert(boot.deletedLegacy && !boot.legacyBank0 &&
         !boot.legacyBank1 && !boot.legacySingletonPass);
  assert(boot.committed && sameWifiProfiles(boot.committedProfiles, legacy));

  // A secure active marker prevents silent downgrade even when decryption
  // fails or the protected key becomes unavailable after a reboot.
  s.failCommittedRead = true;
  int previousWrites = s.stageCalls;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::SecureUnreadable);
  assert(WifiCredentialMigration::afterColdBoot(s, true, true) ==
         Result::SecureUnreadable);
  assert(s.stageCalls == previousWrites && s.purgeCalls == 0);
  s.key = false;
  assert(WifiCredentialMigration::prepare(s, legacy, active) == Result::KeyUnavailable);
  assert(s.purgeCalls == 0);

  // Power-loss ambiguity: commit marker may have been persisted even if
  // commitStaged() reported failure. Next boot must still use secure storage.
  FakeAuthenticatedEncryptedStore ambiguous;
  ambiguous.commitButReportFailure = true;
  assert(WifiCredentialMigration::prepare(ambiguous, legacy, active) ==
         Result::CommitFailed);
  assert(ambiguous.hasCommittedRecord());
  assert(WifiCredentialMigration::prepare(ambiguous, legacy, active) ==
         Result::SecurePresent);
  assert(ambiguous.stageCalls == 1 && ambiguous.purgeCalls == 0);

  // Open network has an empty passphrase and must survive exact readback.
  WifiProfilePolicy mixed;
  assert(mixed.put("Cafe", ""));
  assert(mixed.put("home", "examplepass123"));
  FakeAuthenticatedEncryptedStore openNet;
  assert(WifiCredentialMigration::prepare(openNet, mixed, 0) ==
         Result::ReadyForColdBoot);
  assert(std::strlen(openNet.committedProfiles.at(0)->password) == 0);
  return 0;
}
