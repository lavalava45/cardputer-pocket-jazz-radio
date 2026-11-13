"""New daily radio alarm: end-to-end source contracts without a false hardware PASS."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
POLICY = (ROOT / "PocketJazzRadio/DailyRadioAlarm.h").read_text(encoding="utf-8")
HELP = APP.split("void drawHelp() {", 1)[1].split("void drawRecents()", 1)[0]
KEYS = APP.split("void handleKeys(const Press& k) {", 1)[1].split(
    "void serviceDailyRadioAlarm(uint32_t now)", 1
)[0]
ALARM = APP.split("void serviceDailyRadioAlarm(uint32_t now) {", 1)[1].split(
    "void serviceSleepTimer(uint32_t now)", 1
)[0]


class DailyRadioAlarmIntegration(unittest.TestCase):
    def test_fnt_opens_distinct_user_screen_without_breaking_sleep_timer(self):
        self.assertIn("if (k.fn && pressed(k, 't'))", KEYS)
        self.assertIn("screen = DAILY_ALARM", KEYS)
        self.assertIn("if (pressed(k, 't')) {", KEYS)
        self.assertLess(KEYS.index("if (k.fn && pressed(k, 't'))"),
                        KEYS.rindex("if (pressed(k, 't')) {"))
        self.assertIn("if (screen == DAILY_ALARM)", KEYS)
        self.assertIn("if (k.up) dailyAlarmEdit.hour", KEYS)
        self.assertIn("if (k.right) dailyAlarmEdit.minute", KEYS)
        self.assertIn("char('1' + slot)", KEYS)
        self.assertIn("saveDailyRadioAlarmConfig()", KEYS)
        self.assertIn("if (dailyAlarmSounding && k.enter)", KEYS)
        self.assertIn('showNotice("Alarm stopped")', KEYS)
        self.assertIn('"FN+T DAILY ALARM"', HELP)
        self.assertIn("case DAILY_ALARM: drawDailyAlarm()", APP)
        self.assertIn("case DAILY_ALARM:", APP.split("uint32_t uiSignature(", 1)[1])
        self.assertIn("serviceDailyRadioAlarm(now)", APP)

    def test_scheduler_requires_confirmed_time_and_persisted_day(self):
        self.assertIn("!ntpSynced.load(std::memory_order_acquire)", ALARM)
        self.assertIn("if (utc < MIN_TLS_TIME) return", ALARM)
        self.assertIn("localtime_r(&utc, &local)", ALARM)
        self.assertIn("DailyRadioAlarm::due(", ALARM)
        self.assertIn("claimDailyRadioAlarmDay(today)", ALARM)
        self.assertLess(ALARM.index("claimDailyRadioAlarmDay(today)"),
                        ALARM.index("dailyAlarmSounding = true"))
        self.assertIn("if (!dailyAlarmConfig.enabled || safeMode", ALARM)
        self.assertIn("dailyAlarmLastFiredDay", APP)
        self.assertIn('p.putInt("alarmDay", dayKey)', APP)
        self.assertIn('p.getInt("alarmDay", -1) == dayKey', APP)
        self.assertIn("if (futureNvsSchema || dailyAlarmFutureFormat)", APP)
        self.assertIn('p.getBytesLength("alarmCfg")', APP)
        self.assertIn("dailyAlarmFutureFormat = true", APP)
        self.assertIn("bytes[0] == 'J' && bytes[1] == 'A' && bytes[2] > 1", APP)
        self.assertIn("civilDayKey(int year, int dayOfYear)", POLICY)
        self.assertIn("today > lastFiredDay", POLICY)
        self.assertIn("hour == config.hour && minute == config.minute", POLICY)

    def test_alarm_uses_existing_audio_owner_no_sd_or_unauthenticated_fallback(self):
        self.assertIn("dailyAlarmOutputVolume", APP)
        self.assertIn("cmd.volume = !cmd.play ? 0 : dailyAlarmSounding", APP)
        self.assertIn("DailyRadioAlarm::safeTarget(volume)", ALARM)
        self.assertIn("DailyRadioAlarm::fadeVolume(", ALARM)
        self.assertIn("dailyAlarmFramesAtRequest", ALARM)
        self.assertIn("dailyAlarmRamping = true", ALARM)
        self.assertIn("DailyRadioAlarm::kStartTimeoutMs", ALARM)
        self.assertIn('showNotice("Alarm: stream failed")', ALARM)
        self.assertIn("sendAudio(); // volume-only command", ALARM)
        self.assertIn("if (offlineMode || offlineLeaving || offlineSourceOpen.load()", ALARM)
        self.assertIn("sleepMinutes = 0", ALARM)
        self.assertNotIn("M5.Speaker.setVolume(", ALARM)
        self.assertNotIn("SD.open(", ALARM)
        self.assertNotIn("WiFiClient", ALARM)
        self.assertNotIn("setInsecure", ALARM)
        self.assertEqual(APP.count("static AudioOutputM5Speaker output(&M5.Speaker, 0)"), 1)
        self.assertNotIn("alarmFallbackEnabled", APP)

    def test_save_is_versioned_bounded_and_no_sleep_wake_claim(self):
        self.assertIn("static constexpr size_t kConfigBytes = 8", POLICY)
        self.assertIn("bytes[7] != checksum(bytes)", POLICY)
        self.assertIn("if (!valid(decoded)) return false", POLICY)
        self.assertIn("static constexpr uint32_t kFadeMs = 60000", POLICY)
        self.assertIn("static constexpr uint32_t kStartTimeoutMs = 20000", POLICY)
        self.assertIn("if (elapsedMs >= kFadeMs) return target", POLICY)
        self.assertIn('p.putBytes("alarmCfg", bytes, sizeof(bytes))', APP)
        self.assertIn("memcmp(bytes, back, sizeof(bytes)) == 0", APP)
        self.assertIn("dailyAlarmConfig = dailyAlarmEdit", APP)
        self.assertIn("cfg.external_rtc = false", APP)
        self.assertNotIn("esp_deep_sleep_start", APP)


if __name__ == "__main__":
    unittest.main()
