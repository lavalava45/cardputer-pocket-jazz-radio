"""Integration contracts for the VU-only radio panel; no logo RAM or SD reads."""
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


class StationLogoUIContract(unittest.TestCase):
    def test_8kb_logo_cache_removed_from_main_app(self):
        self.assertNotIn('cachedLogoPixels', APP)
        self.assertNotIn('cachedLogoHash', APP)
        self.assertNotIn('refreshLogoCache', APP)
        self.assertNotIn('pocketjazz::logo::load(', APP)
        self.assertNotIn('#include "StationLogoFormat.h"', APP)
        audio = APP.split('void audioWorker(void*)', 1)[1].split('struct Press {', 1)[0]
        self.assertNotIn('drawVu(', audio)

    def test_vu_is_always_visible_without_wasting_the_side_panel(self):
        self.assertNotIn('bool vuMode', APP)
        # V now marks a *manual hearing task* inside Jazz Journey; it still
        # must not hide the always-on radio VU meter.
        controls = APP.split('void handleKeys(const Press& k) {', 1)[1].split(
            'if (screen == JAZZ_LESSON) {', 1
        )[0]
        self.assertNotIn("if (pressed(k, 'v')) {", controls)
        self.assertIn('"VU ALWAYS ON"', APP)
        player = APP.split('void drawPlayer(bool partial', 1)[1].split('void drawNight(', 1)[0]
        self.assertIn('drawWrappedRadioText(station, 28, C_WHITE, 2)', player)
        self.assertIn('drawVu(shownVu);', player)
        self.assertIn('"VOL %u%%"', player)
        self.assertNotIn('"VU OFF"', player)
        self.assertNotIn('fillRoundRect(9, 29, 75, 76', player)
        self.assertNotIn('iconRadio(', player)
        self.assertNotIn('SD.open(', player)
        vu = APP.split('void drawVu(uint8_t level)', 1)[1].split('void drawPlayer(', 1)[0]
        self.assertIn('fillRect(30, 94, 62, 15', vu)

    def test_single_clock_partial_updates_remain_bounded(self):
        night = APP.split('void drawNight(bool partial', 1)[1].split('void drawMood(', 1)[0]
        self.assertIn('if (!partial)', night)
        self.assertIn('fillRect(9, 26, 225, 90', night)
        self.assertIn('drawNight(true)', APP)
        self.assertIn('"UP/DN TZ 30m   0 RESET   DEL BACK"', night)
        self.assertNotIn('text(177, 31, ntpSynced.load()', night)
        self.assertNotIn('text(181, 48, shortName(zone', night)
        self.assertNotIn('void drawClock(', APP)
        self.assertNotIn('void drawSignal(', APP)


if __name__ == '__main__':
    unittest.main()
