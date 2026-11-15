"""Source-only contract for the eight themed diagnostics pages.

No firmware compilation or physical-device claim is implied.
"""
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
UI = APP.split("void drawDiagnostics(bool partial = false) {", 1)[1].split(
    "void drawSettingsBackup() {", 1
)[0]


class DiagnosticsEightPageLayout(unittest.TestCase):
    def test_exact_page_count_titles_and_navigation(self):
        self.assertIn("constexpr uint8_t DIAG_PAGE_COUNT = 8;", APP)
        self.assertIn("diagPage = (diagPage + DIAG_PAGE_COUNT - 1) % DIAG_PAGE_COUNT", APP)
        self.assertIn("diagPage = (diagPage + 1) % DIAG_PAGE_COUNT", APP)
        titles = [
            "WI-FI STATUS", "WI-FI DISCONNECTS", "HTTPS CONNECTION",
            "RADIO STREAM", "AUDIO PROCESSING", "SOUND TIMING",
            "FREE MEMORY", "SYSTEM STATUS",
        ]
        for title in titles:
            self.assertIn(f'"{title}"', UI)
        self.assertEqual(len(re.findall(r"if\s*\(diagPage == [0-7]\)", UI)), 8)

    def test_all_existing_signal_groups_remain_visible(self):
        groups = [
            ("Wi-Fi", ["wifiReady.load()", "wifiHasIp()", "WiFi.RSSI()", "ntpSynced.load()"]),
            ("stability", ["wifiGotIpEvents.load()", "wifiDisconnectEvents.load()",
                           "wifiDisconnectReason.load()"]),
            ("https", ["audioHttpsPhase.load()", "audioHttpsHop.load()",
                       "audioHttpsDnsMs.load()", "audioHttpsSecureMs.load()",
                       "audioHttpsRequestMs.load()", "audioTlsErrorCode.load()"]),
            ("stream", ["audioState.load()", "audioReopens.load()", "audioFailureStage.load()",
                        "audioSourceStatusCode.load()", "audioBufferedBytes.load()",
                        "audioBufferMinimum.load()", "audioBufferMinAtMs.load()",
                        "audioLastOpenDurationMs.load()", "audioUnderruns.load()"]),
            ("pipeline", ["audioSourcePullMaxUs.load()", "audioDecodeLoopMaxUs.load()",
                          "audioPcmFlushMaxUs.load()", "audioFrames.load()",
                          "decodedPcmRateHz.load()"]),
            ("playback", ["pcm.pcmGapEpisodes", "pcm.pcmGapMaxMs",
                          "pcm.stopLastMs", "pcm.stopP95UpperMs",
                          "pcm.switchLastMs", "pcm.switchP95UpperMs"]),
            ("memory", ["ESP.getFreeHeap()", "heap_caps_get_largest_free_block",
                        "heap_caps_get_minimum_free_size", "audioStackWatermark.load()",
                        "audioHttpsFreeBefore.load()", "audioHttpsLargestBefore.load()",
                        "audioHttpsFreeAfter.load()", "audioHttpsLargestAfter.load()"]),
            ("system", ["now / 1000UL", "esp_reset_reason()",
                        "batteryMillivolts", "uiLoopMaxGapMs.load()"]),
        ]
        for group, expressions in groups:
            for expr in expressions:
                with self.subTest(group=group, expression=expr):
                    self.assertIn(expr, UI)

    def test_fixed_white_data_font_size_and_no_debug_side_effects(self):
        self.assertIn("static constexpr int kDiagFontScale = 1;", APP)
        self.assertIn("text(8, y, line, C_WHITE, kDiagFontScale);", APP)
        self.assertIn("d.setTextFont(1);", UI)
        self.assertIn("d.setTextSize(kDiagFontScale);", UI)
        self.assertNotIn("diagRowScale(", APP)
        self.assertNotIn("textWidth(line)", UI)
        self.assertIn('boundMs == UINT32_MAX ? String(">5120ms/?")', APP)
        self.assertIn("String(titles[diagPage])", UI)
        self.assertIn("C_GOLD", UI)
        self.assertIn("C_WHITE", UI)
        self.assertNotIn("C_RED", UI)
        self.assertNotIn("C_GREEN", UI)
        self.assertNotIn("C_MUTED", UI)
        for unsafe in ("HTTPClient", "WiFi.begin(", "Serial.", "setInsecure(",
                       "sendAudio(", "audioQueue"):
            self.assertNotIn(unsafe, UI)

    def test_all_eight_pages_use_readable_labels_and_fixed_row_scale(self):
        for caption in (
            "Wi-Fi connection", "Signal strength", "Internet clock",
            "Wi-Fi disconnections", "Last disconnect at",
            "Last HTTPS step", "Secure connect", "TLS result code",
            "Radio status", "Open attempts", "Failure: ",
            "Network read max", "MP3 decode max", "Speaker write max",
            "Speaker queue gaps", "Next sound delay", "Speaker queue rejects",
            "Largest free block", "TLS after free/max", "TLS pair = free",
            "Powered on for", "Audio update age", "Screen max pause",
            "Connect attempt", "HTTPS redirects", "NEXT ATTEMPT IN",
        ):
            with self.subTest(caption=caption):
                self.assertIn(caption, UI)
        for cryptic in ('"UI:"', '" A:"', '" H:"', '" D:"', '"MIN AT:"', '"TLS raw:"'):
            self.assertNotIn(cryptic, UI)
        self.assertIn("diagFailName(audioFailureStage.load())", UI)
        self.assertIn("diagRebootName(esp_reset_reason())", UI)
        self.assertIn("diagNextAttemptSeconds(now, audioLive.retryAt)", UI)
        self.assertNotIn("String(now - audioLive.since)", UI)
        self.assertEqual(len(re.findall(r"diagRow\(", UI)), 51)

    def test_row_geometry_keeps_font_and_footer_separate(self):
        # Font 1 at size 1 is a fixed 6x8 bitmap glyph. Pages can use
        # different numbers of rows but not different font sizes.
        sections = re.split(r"} else if \(diagPage == [1-7]\) \{", UI)
        self.assertEqual(8, len(sections))
        for page, section in enumerate(sections):
            y_values = sorted(set(map(int, re.findall(r"diagRow\((\d+),", section))))
            with self.subTest(page=page + 1):
                self.assertTrue(4 <= len(y_values) <= 8)
                self.assertGreaterEqual(y_values[0], 23)
                self.assertLessEqual(y_values[-1] + 8, 116)
                self.assertTrue(all(b - a >= 12 for a, b in zip(y_values, y_values[1:])))

    def test_real_world_failure_strings_fit_240px_without_font_switching(self):
        # Representative inputs from the v0.7z4 photos, including -32512,
        # 32768-byte MP3 allocation and large 32-bit retry/uptime counters.
        samples = [
            "TLS result code: -32512 (ERROR)",
            "TLS before free/max: 133992/34804",
            "TLS after free/max: 91080/31732",
            "Largest free block: 34804 B",
            "MP3 32768/32768 min:32768 B",
            "Failure: 5 NO ROOM FOR RING",
            "Failure: 7 NO MP3 DATA",
            "Wi-Fi disconnections: 4294967295",
            "Audio update age: 4294967295ms",
            "Open:4294967295ms; min at:4294967s",
            "NEXT ATTEMPT IN: 8s",
        ]
        for line in samples:
            with self.subTest(line=line):
                self.assertLessEqual(len(line) * 6, 224)

    def test_keep_existing_manual_actions_and_incremental_redraw(self):
        self.assertIn("drawDiagnostics(true)", APP)
        # Every diagnostics refresh clears the COMPLETE 240x135 display.
        # The former split (0..113, 116..134) left rows 114 and 115 dirty:
        # bottoms of glyphs from page 4/8 survived on sparse page 1/8.
        before_rows = UI.split("static constexpr const char* titles", 1)[0]
        self.assertIn("d.fillScreen(C_BG);", before_rows)
        self.assertLess(before_rows.index("d.fillScreen(C_BG);"),
                        UI.index('text(8, 5, String(titles[diagPage])'))
        self.assertNotIn("d.fillRect(0, 0, 240, 114, C_BG)", UI)
        self.assertNotIn("d.fillRect(0, 116, 240, 19, C_BG)", UI)
        keys = APP.split("if (screen == DIAGNOSTICS)", 1)[1].split(
            "if (screen == SETTINGS_BACKUP)", 1
        )[0]
        self.assertIn("exportIncidentReport()", keys)
        self.assertNotIn("exportDiagnostics()", keys)
        self.assertIn("diagAudioWindow.reset(", keys)
        self.assertIn("audioPlayout.resetCounters()", keys)
        self.assertIn("if (k.del) screen = RADIO", keys)


if __name__ == "__main__":
    unittest.main()
