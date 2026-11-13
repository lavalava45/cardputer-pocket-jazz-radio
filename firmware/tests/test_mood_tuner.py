"""v0.3 Mood Tuner source contracts (offline, no firmware build).

These check the implementation paths, not merely the existence of UI labels.
Failures are expected while v0.3 is being integrated.
"""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp"


def body(source: str, function: str) -> str:
    match = re.search(r"\b(?:void|bool|int)\s+" + re.escape(function) + r"\s*\([^;{}]*\)\s*\{", source)
    if not match:
        raise AssertionError(f"Missing implementation of {function}()")
    start, level = match.end(), 1
    for index in range(start, len(source)):
        if source[index] == "{":
            level += 1
        elif source[index] == "}":
            level -= 1
            if level == 0:
                return source[start:index]
    raise AssertionError(f"Unclosed body for {function}()")


class MoodTunerContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_m3u_mood_tag_is_parsed_and_consumed_per_station(self):
        loader = body(self.source, "loadPlaylists")
        parser = body(self.source, "parseMoodValue")
        add = body(self.source, "addStation")
        self.assertIn('line.startsWith("#PJ-MOOD:")', loader)
        self.assertRegex(loader, r'parseMoodValue\s*\(\s*tags\s*,\s*"energy"\s*\)')
        self.assertRegex(loader, r'parseMoodValue\s*\(\s*tags\s*,\s*"acoustic"\s*\)')
        self.assertRegex(loader, r'addStation\s*\(\s*pendingName\s*,\s*line\s*,\s*pendingAlt\s*,\s*pendingEnergy\s*,\s*pendingAcoustic\s*\)')
        self.assertRegex(loader, r'pendingEnergy\s*=\s*pendingAcoustic\s*=\s*-1', "Tags must not leak to next station")
        self.assertIn("tags[at] < '0'", parser)
        self.assertIn("tags[at] > '4'", parser)
        self.assertIn("tags[at + 1] != ','", parser)
        self.assertRegex(add, r'energy\s*>=\s*0\s*&&\s*energy\s*<=\s*4')
        self.assertRegex(add, r'acoustic\s*>=\s*0\s*&&\s*acoustic\s*<=\s*4')

    def test_no_tag_has_explicit_invalid_sentinel_and_no_match_fallback(self):
        add = body(self.source, "addStation")
        finder = body(self.source, "closestMoodStation")
        handler = body(self.source, "handleKeys")
        self.assertRegex(add, r'moodEnergy\s*=\s*-1')
        self.assertRegex(add, r'moodAcoustic\s*=\s*-1')
        self.assertRegex(finder, r'moodEnergy\s*<\s*0\s*\|\|\s*stations\[i\]\.moodAcoustic\s*<\s*0')
        self.assertRegex(finder, r'\breturn\s+best\s*;')
        self.assertIn('No mood tags in M3U', handler)
        self.assertRegex(handler, r'idx\s*<\s*0\s*\)\s*showNotice')

    def test_mood_grid_has_25_cells_and_visible_selection(self):
        self.assertRegex(self.source, r'\bMOOD\b')
        ui = body(self.source, "drawMood")
        self.assertRegex(ui, r'for\s*\(\s*int\s+(?:acoustic|row|y)\s*=\s*0\s*;\s*(?:acoustic|row|y)\s*(?:<\s*5|<=\s*4)', "Draw all five grid rows")
        self.assertRegex(ui, r'for\s*\(\s*int\s+(?:energy|col|x)\s*=\s*0\s*;\s*(?:energy|col|x)\s*(?:<\s*5|<=\s*4)', "Draw all five grid columns")
        self.assertRegex(ui, r'moodEnergyCursor|moodAcousticCursor', "Highlight the selected cell")
        dispatcher = body(self.source, "drawScreen")
        self.assertRegex(dispatcher, r'case\s+MOOD\s*:\s*drawMood\s*\(')

    def test_cursor_moves_without_automatic_station_switching(self):
        handler = body(self.source, "handleKeys")
        match = re.search(r'if\s*\(\s*screen\s*==\s*MOOD\s*\)\s*\{', handler)
        self.assertIsNotNone(match, "Missing Mood screen keyboard handler")
        start, depth = match.end(), 1
        stop = start
        for stop in range(start, len(handler)):
            depth += (handler[stop] == "{") - (handler[stop] == "}")
            if depth == 0:
                break
        mood = handler[start:stop]
        self.assertRegex(mood, r'moodEnergyCursor\s*=\s*max\s*\(\s*0')
        self.assertRegex(mood, r'moodEnergyCursor\s*=\s*min\s*\(\s*4')
        self.assertRegex(mood, r'moodAcousticCursor\s*=\s*max\s*\(\s*0')
        self.assertRegex(mood, r'moodAcousticCursor\s*=\s*min\s*\(\s*4')
        enter = re.search(r'if\s*\(\s*k\.enter\s*\)\s*\{', mood)
        self.assertIsNotNone(enter, "Explicit Enter confirmation is required")
        self.assertNotIn("selectByUrl(", mood[:enter.start()], "Arrow movement must not tune stations")
        self.assertRegex(mood[enter.end():], r'closestMoodStation\s*\(\s*\)[\s\S]*?selectByUrl\s*\(')

    def test_like_action_is_explicit_sd_append_with_sanitized_text(self):
        handler = body(self.source, "handleKeys")
        like = body(self.source, "likeCurrentTrack")
        self.assertRegex(handler, r"pressed\s*\(\s*k\s*,\s*'k'\s*\)\s*\)\s*\{\s*likeCurrentTrack\s*\(")
        # v0.7a uses the common, preflight-bounded SD append routine rather
        # than opening the log inside this handler. Verify the real callee.
        self.assertIn('appendUiExport("/PocketJazz/liked.txt",', like)
        self.assertIn("!sdAvailable", like)
        self.assertIn('row.c_str(), row.length(), 65536', like)
        append = self.source.split('ExportWriteStatus appendUiExport(', 1)[1].split('void showExportStatus(', 1)[0]
        self.assertIn('SD.open(path, FILE_APPEND)', append)
        self.assertIn('canAppend(existing, length, maxFileBytes)', append)
        self.assertIn('canAppend(file.size(), length, maxFileBytes)', append)
        self.assertIn('if (written != length) return EXPORT_FAILED;', append)
        self.assertIn('cleanTsvField(stations[idx].name, safeName, sizeof(safeName))', like)
        self.assertIn('cleanTsvField(currentTitle[0] ? currentTitle : "track-unknown",', like)
        self.assertIn('file.flush()', append)
        self.assertIn('File verify = SD.open(path, FILE_READ)', append)
        self.assertIn('memcmp(check, data + checked, count)', append)
        self.assertIn('Like saved to SD', like)


if __name__ == "__main__":
    unittest.main()
