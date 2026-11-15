"""Ensure radio hint and offline Notes lookup share a conservative ICY parser."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
HELPER = (ROOT / "PocketJazzRadio/JazzIcyArtist.h").read_text("utf-8")


class StrictIcyAttribution(unittest.TestCase):
    def test_shared_parser_prevents_different_preview_and_lookup_rules(self):
        load = APP.split("void loadArtistNotes() {", 1)[1].split(
            "bool readNotesFile(", 1)[0]
        split = APP.split("void splitRadioMetadata(", 1)[1].split(
            "void drawVu(", 1)[0]
        self.assertIn("strictIcyArtistTrack(currentTitle, artistBytes, song)", load)
        self.assertIn("strictIcyArtistTrack(metadata, artistBytes, song)", split)
        self.assertLess(load.index("strictIcyArtistTrack(currentTitle"),
                        load.index("SD.open("))
        self.assertIn('strstr(delimiter + 3, " - ")', HELPER)

    def test_ambiguous_titles_fall_back_to_raw_display_without_artist_hint(self):
        split = APP.split("void splitRadioMetadata(", 1)[1].split(
            "void drawVu(", 1)[0]
        self.assertIn("artist[0] = title[0] = 0", split)
        self.assertIn("copyUtf8Bounded(title, titleSize, metadata", split)
        draw = APP.split("void drawPlayer(", 1)[1].split("void drawNotes(", 1)[0]
        self.assertIn("artist[0] ?", draw)
        self.assertIn("JazzNowPlaying::hint(artist)", draw)


if __name__ == "__main__":
    unittest.main()
