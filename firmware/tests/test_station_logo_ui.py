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

    def test_vu_default_and_visibility_only_toggle(self):
        self.assertIn('bool vuMode = true;', APP)
        self.assertIn('if (pressed(k, \'v\')) {', APP)
        self.assertIn('vuMode = !vuMode;', APP)
        self.assertIn('"V VU SHOW/HIDE"', APP)
        player = APP.split('void drawPlayer(bool partial', 1)[1].split('void drawNight(', 1)[0]
        self.assertIn('const bool redrawPanel = !partial', player)
        self.assertIn('if (redrawPanel)', player)
        self.assertIn('if (vuMode)', player)
        self.assertIn('drawVu(shownVu);', player)
        self.assertIn('"VU OFF"', player)
        self.assertNotIn('iconRadio(', player)
        self.assertNotIn('SD.open(', player)
        key = APP.split("if (pressed(k, 'v')) {", 1)[1].split('return;', 1)[0]
        self.assertNotIn('sendAudio(', key)
        self.assertNotIn('wantPlay', key)
        self.assertNotIn('muted', key)

    def test_single_clock_partial_updates_remain_bounded(self):
        night = APP.split('void drawNight(bool partial', 1)[1].split('void drawMood(', 1)[0]
        self.assertIn('if (!partial)', night)
        self.assertIn('fillRect(10, 26, 224, 88', night)
        self.assertIn('drawNight(true)', APP)
        self.assertIn('"UP/DN TZ 30m  0 RESET  DEL"', night)
        self.assertNotIn('void drawClock(', APP)
        self.assertNotIn('void drawSignal(', APP)


if __name__ == '__main__':
    unittest.main()
