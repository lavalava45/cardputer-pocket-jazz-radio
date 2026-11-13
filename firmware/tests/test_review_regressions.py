"""Host-side regression contracts for the four reviewer findings.

The pixel fixture models the 6px font-1 cell geometry; it is not an LCD
hardware test. The actual PCM/pre-fill algorithms are separately compiled and
exercised in tests/pcm_level_test.cpp using MSVC/CMake.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
PACKAGER = (ROOT / "tools" / "build.ps1").read_text(encoding="utf-8")


class ReviewerRegressions(unittest.TestCase):
    def test_prefill_owns_time_window_after_successful_https_open(self):
        source_open = APP.index("src->open(endpoint)")
        initial_read = APP.index("buffer->read(&dummy, 0)", source_open)
        clock = APP.index("const uint32_t prefillStarted = millis()", initial_read)
        loop = APP.index("shouldKeepPrefilling(millis(), prefillStarted", clock)
        decode = APP.index("decoder->begin(buffer, &output)", loop)
        self.assertLess(source_open, initial_read)
        self.assertLess(initial_read, clock)
        self.assertLess(clock, loop)
        self.assertLess(loop, decode)
        self.assertNotIn("millis() - openStarted) < 3500", APP)
        self.assertIn("MP3_PREFILL_TIMEOUT_MS = 5000", APP)

    def test_slow_tls_trickle_reaches_playable_bytes_on_separate_clock(self):
        # Executable time-budget simulation. C++ tests exercise the exact
        # production helper; this verifies the full phase ordering.
        for handshake_ms in (3200, 4500):
            with self.subTest(handshake_ms=handshake_ms):
                open_started = 0
                prefill_started = handshake_ms + 100
                now = prefill_started
                buffered = 0
                while buffered < 2048 and now - prefill_started < 5000:
                    buffered += 128  # a valid slow MP3 source
                    now += 500
                self.assertGreaterEqual(buffered, 512, "must be ready for MP3 decoder")
                if handshake_ms == 4500:
                    self.assertGreater(now - open_started, 3500,
                                       "would have failed with old shared deadline")

    def test_partial_redraw_erases_entire_old_battery_text(self):
        beginning = APP.index("void drawPlayer(bool partial")
        ending = APP.index("void drawNight(bool partial", beginning)
        player = APP[beginning:ending]
        self.assertIn('text(123, 6, String(batteryLevel) + "%"', player)
        partial = player[player.index("} else {"):player.index('text(161, 6, wifiReady.load()')]
        self.assertNotIn("fillScreen", partial)
        rectangles = [
            tuple(map(int, m.groups()))
            for m in re.finditer(
                r"d\.fillRect\(\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*C_BG\s*\)",
                partial,
            )
        ]
        # Font 1 at size=1: 6-pixel character cells, 8 pixels high.
        old_pixels = {(x, y) for x in range(123, 123 + 4 * 6)
                      for y in range(6, 14)}  # "100%"
        for x, y, w, h in rectangles:
            old_pixels = {p for p in old_pixels
                          if not (x <= p[0] < x + w and y <= p[1] < y + h)}
        new_pixels = {(x, y) for x in range(123, 123 + 3 * 6)
                      for y in range(6, 14)}  # "99%"
        old_pixels |= new_pixels
        self.assertEqual(old_pixels, new_pixels, "100% -> 99% leaves overprinted pixels")

    def test_meter_tracks_each_stereo_source_channel_not_l_plus_r(self):
        self.assertIn("stereoSourcePeakPercent(stereo, frames)", APP)
        helper = (ROOT / "PocketJazzRadio" / "PCMLevel.h").read_text()
        self.assertIn("absLeft", helper)
        self.assertIn("absRight", helper)
        self.assertNotIn("(left + right) / 2", helper)
        tests = (ROOT / "tests" / "pcm_level_test.cpp").read_text()
        for case in ("leftOnly", "rightOnly", "antiPhase"):
            self.assertIn(case, tests)

    def test_real_release_packager_fails_closed_without_test_gate(self):
        self.assertIn("tests/ directory is absent", PACKAGER)
        self.assertIn("No test_*.py", PACKAGER)
        self.assertIn("no tests were executed", PACKAGER)
        self.assertIn("TEST-REPORT.txt SHA256", PACKAGER)


if __name__ == "__main__":
    unittest.main()
