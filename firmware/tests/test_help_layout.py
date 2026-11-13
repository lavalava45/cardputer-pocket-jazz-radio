"""Source-level accessibility and navigation contracts for the 240x135 LCD.

The keyboard fixture is the MIT-licensed M5Cardputer 1.1.1 upstream
src/utility/Keyboard/Keyboard.h, captured verbatim for public host CI.
The app targets this pinned version; no Arduino dependency is needed here.
"""
import hashlib
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
KEYBOARD_FIXTURE = Path(__file__).resolve().parent / "fixtures/M5Cardputer-1.1.1-Keyboard.h"
KEYBOARD_BYTES = KEYBOARD_FIXTURE.read_bytes()
KEYBOARD_H = KEYBOARD_BYTES.decode("utf-8")
KEYBOARD_SHA256 = "df6d46c842c9f5ef202321ce54285bde16f2666033279d0b9cdb4da68b06e24b"


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
        # A missing/mutated vendor fixture must FAIL the CI gate, never turn
        # this hardware-specific key mapping assertion into an empty skip.
        self.assertEqual(hashlib.sha256(KEYBOARD_BYTES).hexdigest(), KEYBOARD_SHA256)
        self.assertIn("SPDX-License-Identifier: MIT", KEYBOARD_H)
        self.assertIn("constexpr uint8_t HELP_PAGE_COUNT = 18;", APP)
        self.assertIn("sizeof(pages) / sizeof(pages[0]) == HELP_PAGE_COUNT", APP)
        read = APP.split("Press readKeys() {", 1)[1].split("bool pressed(", 1)[0]
        for key, glyph in (("arrowUp", ";"), ("arrowDown", "."), ("arrowLeft", ","), ("arrowRight", "/")):
            self.assertRegex(read, rf"const bool {key} = heldArrow\(k.word, '{re.escape(glyph)}'")
            self.assertIn("lastArrow" + key[5:], read)
        # The pinned keyboard library provides the punctuation keycaps, not KeysState.up.
        self.assertIn("std::vector<char> word;", KEYBOARD_H)
        self.assertNotIn("bool up", KEYBOARD_H)
        keymap = KEYBOARD_H.split("_key_value_map[4][14]", 1)[1].split("class Keyboard_Class", 1)[0]
        home_row = keymap.split("{{KEY_FN", 1)[1].split("{{KEY_LEFT_CTRL", 1)[0]
        bottom_row = keymap.split("{{KEY_LEFT_CTRL", 1)[1]
        self.assertIn("{';', ':'}", home_row)   # right of L
        for pair in ("{',', '<'}", "{'.', '>'}", "{'/', '?'}"):
            self.assertIn(pair, bottom_row)  # physical punctuation keycaps
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
