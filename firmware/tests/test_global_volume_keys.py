"""Source-contract checks for universal volume buttons on Cardputer ADV.

The global handler must run before screen-specific returns. Literal punctuation
must still work in Wi-Fi credentials and station/artist search fields.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
KEYS = APP.split("void handleKeys(const Press& k) {", 1)[1].split(
    "void serviceDailyRadioAlarm(", 1
)[0]


class GlobalVolumeKeysTest(unittest.TestCase):
    def test_volume_handler_is_before_all_screen_specific_returns(self):
        global_start = KEYS.index("const bool volumeDown = pressed(k, '-') || pressed(k, '_');")
        global_end = KEYS.index("const int nextVolume = constrain(volume + step, 0, 185);")
        for screen in (
            "DAILY_ALARM", "WIFI_LIST", "WIFI_PASSWORD", "WIFI_JOIN",
            "HELP", "DIAGNOSTICS", "SETTINGS_BACKUP", "OFFLINE_LIBRARY",
            "RECENTS", "BROWSE", "SEARCH", "MOOD", "NOTES", "NOTES_SEARCH",
            "SPARK", "TRACKS", "SHARE", "TEMPO", "CLOCK", "SOUND_FX",
        ):
            with self.subTest(screen=screen):
                self.assertLess(global_end, KEYS.index(f"if (screen == {screen})"))
        self.assertLess(global_start, KEYS.index("if (pressed(k, 'h'))"))

    def test_volume_changes_use_existing_audio_owner_and_persistence(self):
        handler = KEYS.split("const bool volumeDown", 1)[1].split(
            "if (screen == DAILY_ALARM)", 1
        )[0]
        for fragment in (
            "pressed(k, '=') || pressed(k, '+')",
            "volumeDown != volumeUp",  # no ambiguous simultaneous keys
            "const int step = volumeUp ? 8 : -8;",
            "const int nextVolume = constrain(volume + step, 0, 185);",
            "if (nextVolume != volume)",
            "volume = nextVolume;",
            "markPrefs();",
            "sendAudio(); // volume-only mailbox update; no change of stream intent",
        ):
            self.assertIn(fragment, handler)
        self.assertEqual(KEYS.count("volume = nextVolume;"), 1)
        self.assertEqual(KEYS.count("Alarm level adjusted"), 1)
        self.assertNotIn("M5.Speaker.setVolume", handler)
        self.assertNotIn("M5.Speaker.playRaw", handler)

    def test_text_entry_preserves_literal_punctuation_but_fn_controls_volume(self):
        handler = KEYS.split("const bool volumeDown", 1)[1].split(
            "if (screen == DAILY_ALARM)", 1
        )[0]
        self.assertIn("screen == WIFI_PASSWORD || screen == SEARCH", handler)
        self.assertIn("(screen == NOTES_SEARCH && !notesSearchRan)", handler)
        self.assertIn("(!typingText || k.fn)", handler)
        wifi_input = KEYS.split("if (screen == WIFI_PASSWORD) {", 1)[1].split(
            "if (screen == WIFI_JOIN)", 1
        )[0]
        self.assertIn("for (const char c : k.chars)", wifi_input)
        self.assertIn("wifiPassword += c", wifi_input)
        station_input = KEYS.split("if (screen == SEARCH) {", 1)[1].split(
            "if (screen == MOOD)", 1
        )[0]
        self.assertIn("stationSearch += c", station_input)

    def test_alarm_keeps_its_independent_target_and_global_stop(self):
        self.assertLess(KEYS.index("if (dailyAlarmSounding && k.enter)"),
                        KEYS.index("const bool volumeDown"))
        global_handler = KEYS.split("const bool volumeDown", 1)[1].split(
            "if (screen == DAILY_ALARM)", 1
        )[0]
        self.assertIn("if (dailyAlarmSounding)", global_handler)
        self.assertIn("(int)dailyAlarmTargetVolume + step, 0, 80", global_handler)
        self.assertIn("if (dailyAlarmRamping)", global_handler)
        self.assertIn("dailyAlarmOutputVolume = pocketjazz::DailyRadioAlarm::fadeVolume", global_handler)

    def test_no_duplicated_offline_or_radio_volume_handling(self):
        offline = KEYS.split("if (screen == OFFLINE_LIBRARY) {", 1)[1].split(
            "if (screen == RECENTS)", 1
        )[0]
        self.assertNotIn("volume = max(0, volume - 8)", offline)
        self.assertNotIn("volume = min(185, volume + 8)", offline)
        after_radio = KEYS.split("if (pressed(k, 'p') && listCount > 1)", 1)[1]
        self.assertNotIn("dailyAlarmTargetVolume =", after_radio)
        self.assertNotIn("volume = max(0, volume - 8)", after_radio)
        self.assertNotIn("volume = min(185, volume + 8)", after_radio)

    def test_first_press_adjusts_volume_even_if_display_is_dimmed(self):
        loop = APP.split("void loop() {", 1)[1]
        wake = loop.split("if (screenDimmed) {", 1)[1].split(
            "} else {\n      handleKeys(input);", 1
        )[0]
        self.assertIn("screenDimmed = false;", wake)
        self.assertIn("M5Cardputer.Display.setBrightness(135)", wake)
        self.assertIn("const bool isVolumeKey = pressed(input, '-')", wake)
        self.assertIn("(!typingText || input.fn)", wake)
        self.assertIn("if (isVolumeKey && (!typingText || input.fn)) handleKeys(input);", wake)
        self.assertEqual(wake.count("handleKeys(input)"), 1)

    def test_help_explicitly_documents_global_volume_shortcuts(self):
        help_source = APP.split("void drawHelp() {", 1)[1].split(
            "void drawRecents()", 1
        )[0]
        self.assertIn('"- / = VOL ANYWHERE"', help_source)


if __name__ == "__main__":
    unittest.main()
