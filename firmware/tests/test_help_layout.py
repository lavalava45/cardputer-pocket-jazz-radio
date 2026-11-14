"""Source-level accessibility and navigation contracts for the 240x135 LCD."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
# A public source checkout does not vendor the full upstream M5Cardputer
# distribution. Use the separately hashed, licensed v1.1.1 keyboard fixture
# when the Arduino library is not installed alongside the checkout.
keyboard = ROOT / "arduino-sketchbook/libraries/M5Cardputer/src/utility/Keyboard/Keyboard.h"
if not keyboard.is_file():
    keyboard = ROOT / "tests/fixtures/M5Cardputer-1.1.1-Keyboard.h"
KEYBOARD_H = keyboard.read_text(encoding="utf-8")


class HelpLayoutTests(unittest.TestCase):
    def test_four_large_white_independent_rows_per_page(self):
        table = APP.split("static constexpr HelpPage pages[] = {", 1)[1].split(
            "static_assert(sizeof(pages)", 1
        )[0]
        pages = re.findall(r'\{"([^"]+)", \{(.*?)\}\}', table, flags=re.S)
        self.assertEqual(len(pages), 18)
        for section, raw_rows in pages:
            with self.subTest(section=section):
                rows = re.findall(r'"([^"]+)"', raw_rows)
                self.assertEqual(len(rows), 4)
                self.assertTrue(all(0 < len(row) <= 18 for row in rows), rows)
                self.assertLessEqual(len(section), 21)
        draw = APP.split("void drawHelp() {", 1)[1].split("void drawRecents()", 1)[0]
        self.assertIn("const char* rows[4]", draw)
        self.assertIn("29 + line * 20, page.rows[line], C_WHITE, 2", draw)
        self.assertIn('"UP/DOWN PAGES   DEL BACK", C_WHITE', draw)
        self.assertNotIn("C_GREEN", draw)
        self.assertNotIn("C_GOLD", draw)
        self.assertNotIn("C_MUTED", draw)
        self.assertLess(29 + 3 * 20 + 16, 110)

    def test_physical_keycap_arrows_and_single_back(self):
        self.assertIn("constexpr uint8_t HELP_PAGE_COUNT = 18;", APP)
        self.assertIn("sizeof(pages) / sizeof(pages[0]) == HELP_PAGE_COUNT", APP)
        read = APP.split("Press readKeys() {", 1)[1].split("bool pressed(", 1)[0]
        for key, glyph in (("arrowUp", ";"), ("arrowDown", "."), ("arrowLeft", ","), ("arrowRight", "/")):
            self.assertRegex(read, rf"const bool {key} = heldArrow\(k.word, '{re.escape(glyph)}'")
            self.assertIn("lastArrow" + key[5:], read)
        # The pinned keyboard library provides the punctuation keycaps, not KeysState.up.
        self.assertIn("std::vector<char> word;", KEYBOARD_H)
        self.assertNotIn("bool up", KEYBOARD_H)
        help_keys = APP.split("if (screen == HELP) {", 1)[1].split("if (screen == CATALOG_UPDATE)", 1)[0]
        self.assertIn("if (k.up)", help_keys)
        self.assertIn("else if (k.down)", help_keys)
        self.assertIn("(helpPage + HELP_PAGE_COUNT - 1) % HELP_PAGE_COUNT", help_keys)
        self.assertIn("(helpPage + 1) % HELP_PAGE_COUNT", help_keys)
        self.assertIn("if (k.del) screen = RADIO", help_keys)
        self.assertNotIn("k.enter", help_keys)
        self.assertNotIn("pressed(k, 'h')", help_keys)

    def test_search_spark_and_clock_have_one_navigation_convention(self):
        keys = APP.split("void handleKeys(const Press& k) {", 1)[1].split("void serviceSleepTimer", 1)[0]
        spark = keys.split("if (screen == SPARK) {", 1)[1].split("if (screen == TRACKS)", 1)[0]
        self.assertIn("if (k.up) showJazzSpark(-1)", spark)
        self.assertIn("if (k.down) showJazzSpark(1)", spark)
        self.assertNotIn("k.enter", spark)
        clock = keys.split("if (screen == CLOCK) {", 1)[1].split("if (screen == SOUND_FX)", 1)[0]
        self.assertIn("if (k.up) adjustClockOffset(30)", clock)
        self.assertIn("if (k.down) adjustClockOffset(-30)", clock)
        self.assertNotIn("adjustClockOffset(60)", clock)
        self.assertIn('if (k.tab)', keys)  # one main station search entry
        self.assertNotIn("if (k.tab || pressed(k, '/'))", keys)
        help_text = APP.split("void drawHelp() {", 1)[1].split("void drawRecents()", 1)[0]
        for row in ("F BROWSE STATIONS", "U NOTES SEARCH", "K LIKE TO SD",
                    "E SAVES TSV LOGS", "ICY PEEK SD OPT-IN", "NVS SAVES OFFSET",
                    "D OPEN FROM RADIO", "PRIVATE M3U SAFE"):
            self.assertIn('"' + row + '"', help_text)


if __name__ == "__main__":
    unittest.main()
