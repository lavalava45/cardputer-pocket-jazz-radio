"""Source integration guard for cooperative live libmad retry scheduling."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "arduino-sketchbook/libraries/ESP8266Audio/src"
SOURCE = (LIB / "AudioGeneratorMP3.cpp").read_text(encoding="utf-8")
HEADER = (LIB / "AudioGeneratorMP3.h").read_text(encoding="utf-8")
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


class LiveMp3DecodeBudgetTest(unittest.TestCase):
    def test_only_live_audio_opts_in(self):
        self.assertIn("liveDecodeBudget.setEnabled(timeoutMs != 0)", HEADER)
        self.assertIn("decoder->SetLiveStallTimeoutMs(4000)", APP)
        self.assertIn("bool enabled_ = false", (LIB / "AudioLiveDecodeBudget.h").read_text())

    def test_both_unbounded_loops_have_yield_paths(self):
        self.assertIn("liveDecodeBudget.begin(millis())", SOURCE)
        self.assertGreaterEqual(SOURCE.count("!liveDecodeBudget.tryStep(millis())"), 2)
        self.assertIn("pendingSyncCarry = unused", SOURCE)
        self.assertIn("resumedPendingSync", SOURCE)
        self.assertIn("workBudgetDeferred = true", SOURCE)
        self.assertIn("if (!resumedFromWorkYield && !liveStallPolicy.waiting()", SOURCE)
        self.assertIn("frameWatchdog.expired(millis())", SOURCE)
        self.assertIn("liveStallPolicy.shouldWait", SOURCE)


if __name__ == "__main__":
    unittest.main()
