#pragma once
#include <Arduino.h>

namespace pocketjazz {
// Call recoverCatalogUpdate() after mounting SD, BEFORE scanning playlists.
// Call updateCatalogManually() only on user request with audio stopped,
// SD mounted and synchronized clock. ReadyToActivate means the five new files
// are on SD, but old copies and the pending journal are still retained.
// After remounting/re-reading playlists, call finalizeCatalogUpdate(true)
// ONLY when the new catalog was actually activated. Call false to rollback.
// A reboot before finalize automatically rolls back on recoverCatalogUpdate().
enum class CatalogUpdateResult : uint8_t {
  Ok, NoChange, Busy, InvalidManifest, DownloadFailed, InvalidPlaylist,
  InvalidCatalog, StorageError, RecoveryFailed, ReadyToActivate, ActivationFailed
};
CatalogUpdateResult recoverCatalogUpdate();
CatalogUpdateResult updateCatalogManually(bool audioStopped, bool sdMounted);
// Reconcile actual playlist-parser counts, not merely SD.exists().
// Expected counts apply only to the current pending manual transaction.
bool catalogActivationMatches(const uint8_t observedStationCounts[5]);
CatalogUpdateResult finalizeCatalogUpdate(bool activated);
const char* catalogUpdateResultText(CatalogUpdateResult result);
}
