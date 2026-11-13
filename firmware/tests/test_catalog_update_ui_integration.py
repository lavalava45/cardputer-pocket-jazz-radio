"""Manual catalog update is a separate confirmed, STOP-only UI action.

The test never downloads anything, opens SD, or changes any user data.
Power-failure and file-integrity scenarios are covered by the updater
host-policy tests; here we guard the firmware call-site and screen routing.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
HEADER = (ROOT / "PocketJazzRadio/CatalogUpdater.h").read_text("utf-8")
UPDATER = (ROOT / "PocketJazzRadio/CatalogUpdater.cpp").read_text("utf-8")


class CatalogUpdateUiIntegration(unittest.TestCase):
    def test_boot_recovery_occurs_after_mount_before_any_catalog_scan(self):
        load = APP.split("bool loadPlaylists() {", 1)[1].split(
            "void enterOfflineLibrary()", 1
        )[0]
        self.assertLess(load.index("if (!SD.begin(SD_CS, SPI, 10000000))"),
                        load.index("pocketjazz::recoverCatalogUpdate()"))
        self.assertLess(load.index("pocketjazz::recoverCatalogUpdate()"),
                        load.index('SD.open("/PocketJazz")'))
        self.assertIn("Catalog recovery failed", load)
        self.assertIn("if (!catalogRecoveryChecked)", load)
        self.assertIn("audioState.load() != STOPPED || wantPlay", load)

    def test_explicit_radio_key_opens_review_screen_but_never_downloads(self):
        handle = APP.split("void handleKeys(const Press& k) {", 1)[1].split(
            "void serviceDailyRadioAlarm(", 1
        )[0]
        self.assertIn("if (pressed(k, 'd')) {", handle)
        self.assertIn("screen = CATALOG_UPDATE;", handle)
        self.assertIn("if (screen == CATALOG_UPDATE) {", handle)
        screen = handle.split("if (screen == CATALOG_UPDATE) {", 1)[1].split(
            "if (screen == DIAGNOSTICS)", 1
        )[0]
        self.assertIn("if (k.del) { screen = RADIO; return; }", screen)
        self.assertIn("if (k.enter && !catalogUpdateInProgress)", screen)
        self.assertIn("pocketjazz::updateCatalogManually(stopped, sdAvailable)", screen)
        self.assertIn("!offlineSourceOpen.load", screen)
        self.assertIn("!stationPeek.busy()", screen)
        self.assertIn("!wantPlay && audioState.load() == STOPPED", screen)
        self.assertIn("catalogUpdateStatus == pocketjazz::CatalogUpdateResult::Ok", screen)
        self.assertIn("invalidateStationPeek(true)", screen)
        self.assertIn("loadPlaylists()", screen)
        self.assertIn("const bool reloaded = loadPlaylists();", screen)
        self.assertIn("catalogDefaultListsLoadedMask == 0x1fu", screen)
        self.assertIn("catalogActivationMatches(catalogDefaultStationCounts)", screen)
        self.assertIn("catalogUpdateStatus = pocketjazz::finalizeCatalogUpdate(activated);", screen)
        self.assertIn("if (!activated || catalogUpdateStatus != pocketjazz::CatalogUpdateResult::Ok)", screen)
        self.assertIn("catalogRecoveryChecked = false;", screen)
        self.assertIn("markPrefs()", screen)
        self.assertNotIn("SD.remove(", screen)
        self.assertNotIn("SD.rename(", screen)

    def test_screen_and_help_explicitly_show_confirmed_update(self):
        self.assertIn("case CATALOG_UPDATE: drawCatalogUpdate();", APP)
        self.assertIn("case CATALOG_UPDATE:", APP)
        body = APP.split("void drawCatalogUpdate() {", 1)[1].split(
            "void drawOfflineLibrary()", 1
        )[0]
        for fragment in ("STATION CATALOG", "5 standard MP3 playlists",
                         "Personal stations untouched", "ENTER update  DEL back",
                         "Checks HTTPS / SHA256 / limits"):
            self.assertIn(fragment, body)
        self.assertIn('"CATALOG UPDATE"', APP)
        self.assertIn('"D OPEN FROM RADIO"', APP)
        self.assertIn("CatalogUpdateResult updateCatalogManually(bool audioStopped, bool sdMounted)", HEADER)
        self.assertIn("tls.setCACert(kRadioTrustedRootsPem)", UPDATER)
        self.assertNotIn("setInsecure(", UPDATER)

    def test_activation_verifies_actual_reload_and_manifest_counts(self):
        self.assertIn("uint8_t catalogDefaultStationCounts[5]{}", APP)
        load = APP.split("bool loadPlaylists() {", 1)[1].split(
            "void enterOfflineLibrary()", 1
        )[0]
        self.assertIn("memset(catalogDefaultStationCounts, 0", load)
        self.assertIn("catalogDefaultStationCounts[i] = static_cast<uint8_t>(p.count)", load)
        self.assertIn("if (p.count)", load)
        self.assertIn("bool catalogActivationMatches(const uint8_t observedStationCounts[5])", HEADER)
        self.assertIn("activationExpectedStations[i] = static_cast<uint8_t>(entries[i].stations)", UPDATER)
        self.assertIn("observedStationCounts[i] != activationExpectedStations[i]", UPDATER)
        self.assertNotIn("outcome = CatalogUpdateResult::Ok;", UPDATER.split(
            "CatalogUpdateResult updateCatalogManually(", 1
        )[1].split("bool catalogActivationMatches(", 1)[0])


if __name__ == "__main__":
    unittest.main()
