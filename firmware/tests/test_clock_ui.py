"""Clock and consolidated diagnostics contracts. LCD still needs device testing."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
CONFIG = (ROOT / "data" / "clock.example.txt").read_text(encoding="utf-8")


class TimeAndSignalContracts(unittest.TestCase):
    def test_sd_config_is_optional_bounded_and_defaults_to_utc(self):
        self.assertEqual("TZ=UTC0", CONFIG.strip())
        for fragment in ('strlcpy(clockTz, "UTC0"', 'file.size() <= 256',
                         'validPosixTimeZone(candidate.c_str())',
                         'setenv("TZ", clockTz, 1)', 'tzset()'):
            self.assertIn(fragment, APP)
        self.assertIn('"/PocketJazz/clock.txt"', APP)
        self.assertNotIn("setenv(\"TZ\", savedPassword", APP)

    def test_local_clock_never_forges_epoch_as_valid_calendar(self):
        night = APP[APP.index("void drawNight(bool partial = false)"):APP.index("void drawMood()")]
        self.assertIn("time(nullptr) >= MIN_TLS_TIME", night)
        self.assertIn('date[0] ? date : "TIME NOT SET"', night)
        self.assertIn("localtime_r", night)
        self.assertIn("TIME NOT SET", night)
        self.assertNotIn("drawClock(bool partial = false)", APP)
        self.assertIn("if (!partial)", night)
        self.assertIn('drawNight(true)', APP)
        self.assertIn('esp_sntp_set_time_sync_notification_cb(onNtpSync)', APP)
        self.assertIn('ntpSynced.store(true)', APP)
        self.assertIn('ntpSynced.load() ? "NTP" : "UNSYNC"', night)
        self.assertIn('"UP/DN TZ 30m  0 RESET  DEL"', night)
        self.assertIn('strftime(date, sizeof(date), "%a %d %b"', night)
        self.assertIn('pocketjazz::fixedUtcOffsetRule', APP)
        self.assertIn('TZ effective=%d expected=%d match=%d', APP)
        self.assertIn('effectiveLocalUtcOffsetMinutes(anchor)', APP)
        self.assertIn('localtime_r(&at, &local)', APP)
        self.assertIn('gmtime_r(&at, &utc)', APP)
        self.assertLess(APP.index('tzset();'), APP.index('effectiveLocalUtcOffsetMinutes(anchor)'))
        self.assertIn('p.putInt("tzMin", clockOffsetMinutes)', APP)
        self.assertIn('clockUserOverride = p.getBool("tzSet", false)', APP)
        self.assertIn('p.getInt("tzMin", -100000) == clockOffsetMinutes', APP)
        self.assertIn('NVS offset=%d override=%d bytes=%u/%u readback=%d', APP)
        # Diagnostics now uses a user-facing label; a plausible clock is still
        # not the same thing as an actual NTP synchronization event.
        self.assertIn('time(nullptr) >= MIN_TLS_TIME ? "TIME SET" : "NOT SET"', APP)
        self.assertLess(APP.index('esp_sntp_set_time_sync_notification_cb(onNtpSync)'),
                        APP.index('configTzTime(clockTz, "pool.ntp.org"'),
                        'Set the callback before SNTP starts to avoid missing a fast sync')

    def test_sntp_start_preserves_saved_timezone_instead_of_resetting_to_utc(self):
        """Arduino-ESP32 configTime(0,0) resets TZ; configTzTime reapplies it."""
        self.assertIn('loadClockSettings(); // Apply saved NVS offset', APP)
        self.assertIn('configTzTime(clockTz, "pool.ntp.org", "time.google.com")', APP)
        self.assertNotIn('configTime(0, 0, "pool.ntp.org", "time.google.com")', APP)
        self.assertIn('localtime_r(&t, &clockTm)', APP)
        self.assertIn('clockUserOverride = p.getBool("tzSet", false)', APP)
        self.assertIn('pocketjazz::validPosixTimeZone(candidate.c_str())', APP)

    def test_signal_information_is_in_diagnostics_not_a_separate_screen(self):
        diag = APP[APP.index("void drawDiagnostics("):APP.index("void drawSettingsBackup()")]
        self.assertIn("Signal strength:", diag)
        self.assertIn("audioRingCapacity.load()", diag)
        self.assertIn("Battery voltage:", diag)
        self.assertIn("batteryMillivolts", diag)
        self.assertNotIn("drawSignal(", APP)
        self.assertIn("drawNight(true)", APP)
        self.assertIn("mix(now / 1000)", APP)
        self.assertIn("mix((uint32_t)(time(nullptr) / 60))", APP)

    def test_keyboard_contexts_preserve_s_safe_mode_and_c_tempo_clear(self):
        self.assertIn("safeMode = M5Cardputer.Keyboard.isKeyPressed('s')", APP)
        self.assertIn("if (pressed(k, 'c')) { manualTempo.reset(); return; }", APP)
        self.assertIn("if (pressed(k, 'c')) { screen = CLOCK; return; }", APP)
        self.assertNotIn("clockNightView", APP)
        self.assertIn("if (pressed(k, 'b')) { backupConfirmation = 0; screen = SETTINGS_BACKUP; return; }", APP)
        self.assertNotIn("if (pressed(k, 'b')) { screen = CLOCK", APP)
        self.assertNotIn("case NIGHT:", APP)
        self.assertIn("case CLOCK: drawNight(); break;", APP)
        self.assertIn("drawNight(true)", APP)
        self.assertNotIn("if (pressed(k, 's')) { screen = SIGNAL; return; }", APP)
        self.assertIn("if (k.down) moodAcousticCursor", APP)
        help_screen = APP.split("void drawHelp() {", 1)[1].split("void drawRecents()", 1)[0]
        self.assertIn('"FN+T DAILY ALARM"', help_screen)
        self.assertIn('"C CLOCK"', help_screen)
        self.assertIn('"S TUNING SOUND FX"', help_screen)
        self.assertNotIn('"T NIGHT IN CLOCK"', help_screen)
        self.assertNotIn('"S SIGNAL/BUFFER"', help_screen)
        self.assertIn("UP ADD 30 MIN", help_screen)
        self.assertIn("DOWN LESS 30 MIN", help_screen)


if __name__ == "__main__":
    unittest.main()
