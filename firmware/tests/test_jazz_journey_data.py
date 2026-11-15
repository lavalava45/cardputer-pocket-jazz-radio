"""Data and wiring contracts for optional offline Jazz Journey data.

These are source tests; hardware SD timing and speaker behavior are pending.
"""
from pathlib import Path
import unittest
import sys

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
sys.path.insert(0, str(ROOT / "tools"))
import build_jazz_journey  # noqa


class JazzJourneyDataset(unittest.TestCase):
    def test_24_linked_stages_and_30_listening_cards(self):
        lines = (DATA / "jazz-journey.tsv").read_text("ascii").splitlines()
        tips = (DATA / "jazz-listen.tsv").read_text("ascii").splitlines()
        self.assertEqual(len(lines), 24)
        self.assertEqual(len(tips), 30)
        musicians = {
            line.split("\t", 1)[0]
            for line in (DATA / "jazz-encyclopedia.tsv").read_text("ascii").splitlines()
        }
        spark_topics = {
            line.split("\t")[2] for line in
            (DATA / "jazz-facts.tsv").read_text("ascii").splitlines()
            if line.startswith("FACT\t")
        }
        for i, line in enumerate(lines):
            f = line.split("\t")
            self.assertEqual(len(f), 15)
            self.assertEqual(int(f[0]), i)
            self.assertEqual(int(f[1]), i//6)
            self.assertIn(f[6], musicians)
            self.assertIn(f[13], spark_topics,
                          "Every course stage must open a real Jazz Spark FACT")
            self.assertGreater(len(f[5]), 45)
            self.assertGreater(len(f[7]), 30)
            self.assertIn(f[12], ("1", "2", "3"))
            self.assertTrue(f[14].startswith("https://"))
            self.assertLess(len(line), 800)
            self.assertFalse(any(ord(c) < 32 and c != "\t" for c in line))
        styles = [int(s.split("\t")[2]) for s in lines]
        self.assertEqual(styles, [0]*6 + [1]*6 +
                         [2,2,2,3,4,5,6,5,7,8,9,10])
        self.assertEqual(set(styles), set(range(11)))
        for i, line in enumerate(tips):
            f=line.split("\t")
            self.assertEqual(len(f),5)
            self.assertEqual(int(f[0]),i)
            self.assertLess(int(f[1]),24)
            self.assertTrue(f[4].startswith("https://"))
        self.assertLess((DATA / "jazz-journey.tsv").stat().st_size, 32*1024)
        self.assertLess((DATA / "jazz-listen.tsv").stat().st_size, 16*1024)

    def test_regeneration_deterministic_and_readonly(self):
        before=[(DATA / name).read_bytes() for name in
                ("jazz-encyclopedia.tsv", "jazz-facts.tsv", "artist-notes.tsv")]
        expected=build_jazz_journey.render()
        self.assertEqual(expected,build_jazz_journey.render())
        for name, text in expected.items():
            # The preexisting provenance MD has CRLF on Windows; read_text
            # compares its content without silently reformatting that file.
            self.assertEqual((DATA / name).read_text("ascii" if name.endswith(".tsv") else "utf-8"),text)
        self.assertEqual(before,[(DATA / name).read_bytes() for name in
                                 ("jazz-encyclopedia.tsv", "jazz-facts.tsv", "artist-notes.tsv")])

    def test_educational_source_not_audio_or_auto_play(self):
        header=(ROOT / "PocketJazzRadio/JazzJourney.h").read_text("utf-8")
        self.assertIn("struct JazzJourneyLesson", header)
        self.assertIn("struct JazzListeningTip", header)
        self.assertIn("JazzJourneyProgress", header)
        self.assertIn("kLessons = 24", header)
        self.assertIn("kTips = 30", header)
        self.assertIn("kStyles = 11", header)
        self.assertNotIn("WiFi.begin", header)
        self.assertNotIn("SD.open", header)
        self.assertNotIn("new ", header)


if __name__ == "__main__":
    unittest.main()
