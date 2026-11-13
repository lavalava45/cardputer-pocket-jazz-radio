// SPDX-License-Identifier: GPL-3.0-or-later
// Host-testable policy for a FUTURE hardware-backed encrypted Wi-Fi store.
//
// This is intentionally NOT wired into PocketJazzRadioApp.cpp. The shipping
// Arduino-ESP32 3.3.9 SDK has CONFIG_NVS_ENCRYPTION disabled and no verified
// protected key/secure partition. A software key derived from chip MAC or
// compiled into a BIN must never satisfy the hardwareProtected() precondition.
//
// SecureStore must authenticate ONE complete versioned record containing the
// active index, count and BOTH Wi-Fi slots (not accept partial slot recovery),
// provide encrypted persistence and ensure that commit() changes the
// active-record marker only AFTER the staged record has been durably written
// and authenticated. No plaintext fallback is allowed after a secure marker
// exists, even on decryption failure.
#pragma once
#include "WifiProfilePolicy.h"
#include <cstdint>
#include <cstring>

namespace pocketjazz {

enum class WifiCredentialMigrationResult : uint8_t {
  KeyUnavailable,
  InvalidProfiles,
  SecurePresent,
  SecureUnreadable,
  StagingFailed,
  ReadbackFailed,
  CommitFailed,
  ReadyForColdBoot,
  AwaitingColdBootProof,
  AwaitingNetworkProof,
  ReadyForLegacyCleanup,
  CleanupFailed,
  Migrated
};

inline bool sameWifiProfiles(const WifiProfilePolicy& a,
                             const WifiProfilePolicy& b) {
  if (a.count() != b.count()) return false;
  for (int i = 0; i < WifiProfilePolicy::kCapacity; ++i) {
    const WifiProfile* x = a.at(i);
    const WifiProfile* y = b.at(i);
    if ((x == nullptr) != (y == nullptr)) return false;
    if (!x) continue;
    if (!WifiProfilePolicy::valid(x->ssid, x->password) ||
        !WifiProfilePolicy::valid(y->ssid, y->password) ||
        std::strcmp(x->ssid, y->ssid) ||
        std::strcmp(x->password, y->password)) return false;
  }
  return true;
}

// Backend contract:
//  bool hardwareProtected() const;
//  bool hasCommittedRecord() const;
//  bool stageEncrypted(const WifiProfilePolicy&, int active);
//  bool readStaged(WifiProfilePolicy&, int& active) const;
//  bool commitStaged();
//  bool readCommitted(WifiProfilePolicy&, int& active) const;
//  bool recoveredOnColdBoot() const; // record loaded by a NEW boot, not writer
//  bool purgeLegacyPlaintext();
//
// No implementation may generate or expose a key through plaintext NVS/flash,
// diagnostics, SD exports, logs, or a fixed compile-time constant.
class WifiCredentialMigration {
 public:
  template<class SecureStore>
  static WifiCredentialMigrationResult prepare(
      SecureStore& store, const WifiProfilePolicy& legacy, int active) {
    if (!store.hardwareProtected()) return WifiCredentialMigrationResult::KeyUnavailable;

    if (store.hasCommittedRecord()) {
      WifiProfilePolicy existing;
      int selected = -1;
      // IMPORTANT: a damaged secure record is not permission to load plaintext.
      return store.readCommitted(existing, selected) &&
                     validSelection(existing, selected)
                 ? WifiCredentialMigrationResult::SecurePresent
                 : WifiCredentialMigrationResult::SecureUnreadable;
    }
    if (!validSelection(legacy, active))
      return WifiCredentialMigrationResult::InvalidProfiles;

    if (!store.stageEncrypted(legacy, active))
      return WifiCredentialMigrationResult::StagingFailed;

    WifiProfilePolicy readback;
    int readbackActive = -1;
    if (!store.readStaged(readback, readbackActive) ||
        readbackActive != active ||
        !sameWifiProfiles(legacy, readback))
      return WifiCredentialMigrationResult::ReadbackFailed;

    if (!store.commitStaged())
      return WifiCredentialMigrationResult::CommitFailed;

    WifiProfilePolicy committed;
    int committedActive = -1;
    if (!store.hasCommittedRecord() ||
        !store.readCommitted(committed, committedActive) ||
        committedActive != active ||
        !sameWifiProfiles(legacy, committed))
      return WifiCredentialMigrationResult::SecureUnreadable;

    // Preserve old plaintext until an actual cold boot and successful Wi-Fi
    // join have proved the encrypted record can recover autonomously.
    return WifiCredentialMigrationResult::ReadyForColdBoot;
  }

  template<class SecureStore>
  static WifiCredentialMigrationResult afterColdBoot(
      SecureStore& store, bool wifiAssociationVerified,
      bool explicitLegacyCleanupPermission) {
    if (!store.hardwareProtected()) return WifiCredentialMigrationResult::KeyUnavailable;
    if (!store.hasCommittedRecord()) return WifiCredentialMigrationResult::SecureUnreadable;
    WifiProfilePolicy recovered;
    int active = -1;
    if (!store.readCommitted(recovered, active) ||
        !validSelection(recovered, active))
      return WifiCredentialMigrationResult::SecureUnreadable;

    // A same-session readback cannot prove that the encrypted store and its
    // key are recoverable after power loss. Backend records this only after
    // an actual fresh boot has reloaded the committed encrypted marker.
    if (!store.recoveredOnColdBoot())
      return WifiCredentialMigrationResult::AwaitingColdBootProof;
    if (!wifiAssociationVerified)
      return WifiCredentialMigrationResult::AwaitingNetworkProof;
    if (!explicitLegacyCleanupPermission)
      return WifiCredentialMigrationResult::ReadyForLegacyCleanup;
    // The backend must purge BOTH legacy banks AND the singleton "pass".
    // NVS key removal is NOT a secure physical erase of old flash pages.
    if (!store.purgeLegacyPlaintext())
      return WifiCredentialMigrationResult::CleanupFailed;
    return WifiCredentialMigrationResult::Migrated;
  }

 private:
  static bool validSelection(const WifiProfilePolicy& p, int active) {
    if (active < 0 || active >= WifiProfilePolicy::kCapacity || !p.at(active) ||
        p.count() == 0) return false;
    for (int i = 0; i < WifiProfilePolicy::kCapacity; ++i) {
      const auto* item = p.at(i);
      if (item && !WifiProfilePolicy::valid(item->ssid, item->password))
        return false;
    }
    return true;
  }
};

} // namespace pocketjazz
