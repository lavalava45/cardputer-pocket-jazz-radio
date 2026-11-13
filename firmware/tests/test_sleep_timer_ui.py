"""Source-only sleep-timer and context-sensitive T key UI regression."""
import re
import unittest
from pathlib import Path


APP = (Path(__file__).resolve().parents[1] / "PocketJazzRadio" /
       "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


class SleepTimerInterfaceTest(unittest.TestCase):
    def test_timer_steps_have_explicit_values_and_individual_notices(self):
        keys = APP.split("void handleKeys(const Press& k) {", 1)[1].split(
            "void serviceSleepTimer", 1
        )[0]
        self.assertIn("sleepMinutes == 0 ? 30", keys)
        self.assertIn("sleepMinutes == 30 ? 45", keys)
        self.assertIn("sleepMinutes == 45 ? 60 : 0", keys)
        self.assertIn("sleepStartedAt = millis();", keys)
        self.assertIn('"SLEEP: %u MIN"', keys)
        self.assertIn('"SLEEP: OFF"', keys)
        self.assertNotIn('showNotice("Sleep timer set")', keys)
        fn_alarm = keys.index("if (k.fn && pressed(k, 't'))")
        radio_timer = keys.index("if (pressed(k, 't'))", fn_alarm)
        self.assertLess(fn_alarm, radio_timer)

    def test_help_explains_t_and_single_clock_with_timezone_controls(self):
        ui = APP.split("void drawHelp() {", 1)[1].split("void drawRecents()", 1)[0]
        self.assertIn('"T SLEEP 30/45/60"', ui)
        self.assertIn('"C CLOCK"', ui)
        self.assertIn('"S TUNING SOUND FX"', ui)
        self.assertNotIn('"T NIGHT IN CLOCK"', ui)
        clock = APP.split("if (screen == CLOCK) {", 1)[1].split(
            "if (pressed(k, 'h'))", 1
        )[0]
        self.assertIn("adjustClockOffset(30)", clock)
        self.assertIn("adjustClockOffset(-30)", clock)
        self.assertNotIn("clockNightView", clock)

    def test_timer_stops_without_changing_alarm_or_clock_behavior(self):
        timer = APP.split("void serviceSleepTimer(", 1)[1].split(
            "void serviceOfflineCompletion", 1
        )[0]
        self.assertIn("if (!sleepMinutes) return", timer)
        self.assertIn("sleepMinutes * 60000UL", timer)
        self.assertIn('showNotice("Sleep timer ended")', timer)
        self.assertIn("wantPlay = false", timer)
        self.assertIn("sendAudio()", timer)


if __name__ == "__main__":
    unittest.main()
