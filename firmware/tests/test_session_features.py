"""Firmware integration contracts; C++ helpers are executed in CTest."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
HISTORY = (ROOT / "PocketJazzRadio" / "TrackHistory.h").read_text(encoding="utf-8")
PREFS = (ROOT / "PocketJazzRadio" / "PreferenceGuards.h").read_text(encoding="utf-8")


class SessionFeatures(unittest.TestCase):
    def test_icy_history_is_bounded_and_no_background_polling(self):
        self.assertIn("TrackHistory<5> trackHistory", APP)
        self.assertIn("trackHistory.add(stations[idx].name, incoming.title, millis())", APP)
        self.assertIn("incoming.stationId != stationIndex()", APP)
        self.assertIn("audioState.load() == PLAYING", APP)
        self.assertIn("if (screen == TRACKS)", APP)
        self.assertIn("pressed(k, 'y')", APP)
        self.assertIn("case TRACKS: drawTracks()", APP)
        self.assertRegex(HISTORY, r"data_\[Capacity\]")
        self.assertIn("std::strcmp(previous->title, entry.title) == 0", HISTORY)
        draw = APP.split("void drawTracks()", 1)[1].split("void drawShare()", 1)[0]
        self.assertNotRegex(draw, r"\b(?:WiFiClient|HTTPClient|GET|POST|SD\.open)\b")

    def test_future_nvs_schema_is_read_only_in_all_writer_functions(self):
        self.assertIn("getUChar(\"schema\", 0)", APP)
        self.assertIn("canWritePreferenceSchema(storedSchema)", APP)
        self.assertIn("p.putUChar(\"schema\", pocketjazz::kCurrentPreferenceSchema)", APP)
        self.assertIn("if (futureNvsSchema)", APP)
        for marker in ("void savePrefs()", "void savePreset(int slot)",
                       "void saveRecent()"):
            section = APP.split(marker, 1)[1].split("\n}", 1)[0]
            self.assertIn("futureNvsSchema", section, marker)
        wifi = APP.split("bool saveWifi()", 1)[1].split("\n}", 1)[0]
        self.assertIn("commitWifiProfiles(candidate, index)", wifi)
        commit = APP.split("bool commitWifiProfiles(", 1)[1].split("\n}", 1)[0]
        self.assertIn("if (futureNvsSchema)", commit)
        self.assertIn("autoPlay = false", APP)
        restore = APP.split("void restorePrefs()", 1)[1].split("bool commitWifiProfiles(", 1)[0]
        for original, dest in (("rawLastUrl", "restoredStationUrl"),
                               ("rawPresetUrl", "presets[i].url"),
                               ("rawRecentUrl", "candidate.url")):
            self.assertIn(f"{original}.c_str(), {original}.length()", restore)
            self.assertIn(f"copyValidatedSavedStreamUrl({dest}", restore)
        self.assertIn("originalBytes >= N", PREFS)
        self.assertIn("std::strlen(original) != originalBytes", PREFS)
        self.assertIn("validSavedStreamUrl(original, N)", PREFS)
        self.assertLess(restore.index('canWritePreferenceSchema(storedSchema)'),
                        restore.index('p.getString("lastUrl"'))
        self.assertIn("stored <= kCurrentPreferenceSchema", PREFS)


if __name__ == "__main__":
    unittest.main()
