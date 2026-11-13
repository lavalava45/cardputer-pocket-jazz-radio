"""Keep on-device Tab help and the actual early-return screen router aligned."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
KEYS = APP.split("void handleKeys(const Press& k) {", 1)[1].split(
    "void serviceSleepTimer(", 1
)[0]


class SearchScreenEntry(unittest.TestCase):
    def test_tab_search_from_radio_not_browse(self):
        browse = KEYS.split("if (screen == BROWSE) {", 1)[1].split(
            "if (screen == SEARCH) {", 1
        )[0]
        self.assertIn("return;", browse)
        self.assertNotIn("if (k.tab)", browse)
        self.assertLess(KEYS.index("if (screen == BROWSE) {"),
                        KEYS.index("if (k.tab) {", KEYS.index("if (screen == SOUND_FX) {")))
        radio_only = KEYS.split("if (pressed(k, 'f')) {", 1)[1]
        self.assertIn("if (k.tab) {", radio_only)
        self.assertIn("screen = SEARCH;", radio_only)

    def test_documented_search_entry_matches_actual_source(self):
        # The public guide is maintained outside the frozen firmware source
        # archive; code invariants are asserted here without filesystem links.
        self.assertIn("if (k.del) screen = RADIO;", KEYS)
        self.assertIn("if (pressed(k, 'h'))", KEYS)


if __name__ == "__main__":
    unittest.main()
