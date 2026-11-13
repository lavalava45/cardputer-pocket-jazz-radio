"""Source-level Wi-Fi state checks; hardware connection still requires device tests."""

import re
import unittest
from pathlib import Path


SOURCE = (Path(__file__).resolve().parents[1] / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


class WifiStateContract(unittest.TestCase):
    def test_eight_page_diagnostics_clear_previous_page(self):
        self.assertIn("constexpr uint8_t DIAG_PAGE_COUNT = 8;", SOURCE)
        self.assertIn("diagPage = (diagPage + DIAG_PAGE_COUNT - 1) % DIAG_PAGE_COUNT", SOURCE)
        self.assertIn("diagPage = (diagPage + 1) % DIAG_PAGE_COUNT", SOURCE)
        self.assertIn('String(diagPage + 1) + "/" + String(DIAG_PAGE_COUNT)', SOURCE)
        self.assertIn("heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)", SOURCE)
        self.assertIn("audioFrames.load()", SOURCE)
        self.assertIn("esp_reset_reason()", SOURCE)
        self.assertIn("drawDiagnostics(true)", SOURCE)
        # A complete redraw also clears the two-pixel strip between content
        # and footer when navigating from a dense page to a sparse page.
        diagnostics = SOURCE.split('void drawDiagnostics(bool partial = false)', 1)[1].split('void drawScreen()', 1)[0]
        self.assertIn('(void)partial;', diagnostics)
        self.assertIn('d.fillScreen(C_BG);', diagnostics)
        self.assertIn("screen == DIAGNOSTICS && lastPaintedScreen == DIAGNOSTICS", SOURCE)
    def test_wifi_uses_async_scan(self):
        self.assertRegex(SOURCE, r"WiFi\.scanNetworks\s*\(\s*true\b")
        self.assertIn("WiFi.scanComplete()", SOURCE)

    def test_failed_scan_is_handled(self):
        self.assertIn("WIFI_SCAN_FAILED", SOURCE)
        self.assertRegex(SOURCE, r"WiFi\.scanDelete\s*\(")

    def test_failed_connection_does_not_persist_credentials(self):
        # saveWifi() must happen only after WL_CONNECTED in serviceWifi().
        match = re.search(r"void\s+serviceWifi\s*\(\s*\)\s*\{", SOURCE)
        self.assertIsNotNone(match)
        service = SOURCE[match.start():SOURCE.find("void beginWifiJoin", match.end())]
        self.assertRegex(service, r"(?:WiFi\.status\s*\(\s*\)\s*==\s*WL_CONNECTED|wifiReady\.load\s*\(\s*\))")
        success = service.find("WL_CONNECTED") if "WL_CONNECTED" in service else service.find("wifiReady.load()")
        self.assertLess(success, service.index("saveWifi()"))
        self.assertIn("WIFI_TIMEOUT_MS", service)

    def test_wifi_screen_displays_actionable_failure(self):
        # The retry timeout should leave the user with an explanation, not just return to password entry.
        screens = re.search(r"void\s+drawWifiPassword\s*\([^)]*\)\s*\{([\s\S]*?)\n\}\s*void\s+drawWifiJoin", SOURCE)
        self.assertIsNotNone(screens, "Missing Wi-Fi password/join screens")
        self.assertRegex(
            screens.group(1),
            r"(?i)(?:failed|timeout|timed out|error|incorrect|retry|wrong password)",
            "Show a reason/status for Wi-Fi join failure to distinguish timeout from a password typo",
        )


if __name__ == "__main__":
    unittest.main()
