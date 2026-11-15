"""Read-only evidence contracts linking curriculum, Notes, Spark and ICY.

A title from an unknown broadcaster does not become a known artist by
similarity. If a lesson refers to a musician, it must have a sourced, useful
biography and an explicit matching Now Playing caption, not a generic label.
"""
from pathlib import Path
import json
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_now_playing_hints import CAPTIONS, RESEARCH_CAPTIONS


class JazzLearningContentAlignment(unittest.TestCase):
    def test_all_24_lessons_open_sourced_notes_and_spark_without_fuzzy_guesses(self):
        rows = {
            fields[0]: fields for fields in (
                line.split("\t") for line in
                (ROOT / "data/jazz-encyclopedia.tsv").read_text("ascii").splitlines()
            )
        }
        protected = {
            line.split("\t", 1)[0] for line in
            (ROOT / "data/jazz-editorial-bios.tsv").read_text("ascii").splitlines()
        }
        researched = {
            line.split("\t", 1)[0] for line in
            (ROOT / "data/jazz-researched-context.tsv").read_text("ascii").splitlines()
        }
        facts = {
            fields[2] for fields in (
                line.split("\t") for line in
                (ROOT / "data/jazz-facts.tsv").read_text("ascii").splitlines()
            ) if fields[0] == "FACT"
        }
        lessons = [
            line.split("\t") for line in
            (ROOT / "data/jazz-journey.tsv").read_text("ascii").splitlines()
        ]
        self.assertEqual(len(lessons), 24)
        unique_artists = {l[6] for l in lessons}
        self.assertGreaterEqual(len(unique_artists), 15)
        self.assertTrue(unique_artists <= protected | researched)
        self.assertTrue(unique_artists <= set(CAPTIONS) | set(RESEARCH_CAPTIONS))
        for lesson in lessons:
            artist, topic = lesson[6], lesson[13]
            self.assertIn(artist, rows)
            self.assertIn(topic, facts)
            key, name, summary, album, source = rows[artist]
            self.assertGreaterEqual(len(summary), 80, artist)
            self.assertTrue(source.startswith("https://"), artist)
            # Never derive ICY matches from a stage title or a partial name.
            self.assertNotEqual(lesson[3].lower(), name.lower())
            self.assertIn(int(lesson[12]), (1, 2, 3))

    def test_every_explained_caption_has_distinct_source_biography(self):
        researched = {
            line.split("\t")[0]: line.split("\t")[1:] for line in
            (ROOT / "data/jazz-researched-context.tsv").read_text("ascii").splitlines()
        }
        protected = {
            line.split("\t")[0]: line.split("\t")[1:] for line in
            (ROOT / "data/jazz-editorial-bios.tsv").read_text("ascii").splitlines()
        }
        self.assertFalse(set(researched) & set(protected))
        self.assertEqual(set(RESEARCH_CAPTIONS), set(researched))
        self.assertEqual(set(CAPTIONS), set(protected))
        source = (ROOT / "PocketJazzRadio/JazzNowPlaying.h").read_text("ascii")
        self.assertIn("kCuratedCount = 595", source)
        labels = [
            (json.loads(name), json.loads(caption))
            for name, caption in re.findall(
                r'\{("(?:\\.|[^"\\])*"), ("(?:\\.|[^"\\])*")\}', source)
        ]
        self.assertEqual(len(labels), 595)
        self.assertEqual(len({name.lower() for name, _ in labels}), 595)
        self.assertTrue(all(10 <= len(text) <= 36 for _, text in labels))

    def test_passport_heard_cannot_be_marked_by_opening_lessons_or_stream(self):
        app = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
        journey = app.split("if (screen == JAZZ_LESSON) {", 1)[1].split(
            "if (screen == JAZZ_QUIZ)", 1)[0]
        self.assertIn("pressed(k, 'v')", journey)
        self.assertIn("journeyProgress.markHeard(journeyLessonId)", journey)
        self.assertNotIn("journeyProgress.markLearned(journeyLessonId)", journey)
        radio = app.split("void audioWorker(void*) {", 1)[1].split("void loadPeekConfig()", 1)[0]
        for action in ("markHeard(", "markLearned(", "saveJazzJourney(", "queueJazzDiary("):
            self.assertNotIn(action, radio)


if __name__ == "__main__":
    unittest.main()
