"""Public onboarding must work without source compilation or private SD data."""
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
QUICK = ROOT / "docs/QUICK-START.md"
STORY = ROOT / "docs/WHY-POCKET-JAZZ.md"


class OnboardingDocs(unittest.TestCase):
    def test_quick_start_is_three_explicit_steps(self):
        contents = QUICK.read_text(encoding="utf-8")
        steps = re.findall(r"(?m)^([1-9])\. \*\*(Download|Copy|Launch)\*\*", contents)
        self.assertEqual(steps, [("1", "Download"), ("2", "Copy"), ("3", "Launch")])
        self.assertIn("PocketJazzRadio-v0.8.0-app-only.bin", contents)
        self.assertIn("releases/download/v0.8.0/", contents)
        self.assertIn("M5Launcher 3 → SD", contents)
        self.assertIn("2.4 GHz Wi-Fi", contents)

    def test_optional_data_and_personal_safety_explicit(self):
        contents = QUICK.read_text(encoding="utf-8")
        for filename in (
            "jazz-encyclopedia.tsv",
            "artist-notes.tsv",
            "artist-notes.idx",
            "jazz-facts.tsv",
            "04-my-stations.m3u",
        ):
            self.assertIn(filename, contents)
        self.assertIn("Never overwrite your own file", contents)
        self.assertIn("do *not* need", contents)
        self.assertIn("stop radio/offline playback", contents)
        self.assertIn("some stations can buffer or become unavailable", contents)
        self.assertIn("the project tracks these cases openly", contents)

    def test_story_is_cautious_and_accessible(self):
        contents = STORY.read_text(encoding="utf-8")
        for phrase in (
            "51 hand-picked",
            "Jazz Notes",
            "Jazz Spark",
            "Mood Tuner",
            "BUFFERING",
            "v0.7z16",
            "BUFFERING remains tracked as an open issue",
            "Suggested title",
            "actual v0.8.0 publication",
        ):
            self.assertIn(phrase, contents)
        self.assertIn("v0.7z13 emulator", contents)
        self.assertIn("Do not copy the old draft as current release information.", contents)

    def test_navigation_and_local_links(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("docs/QUICK-START.md", readme)
        self.assertIn("docs/WHY-POCKET-JAZZ.md", readme)
        for document in (QUICK, STORY):
            text = document.read_text(encoding="utf-8")
            refs = re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", text)
            refs += re.findall(r'<img\s+[^>]*src="([^"]+)"', text)
            for ref in refs:
                local = ref.split("#", 1)[0]
                if not local or local.startswith(("https://", "http://", "mailto:")):
                    continue
                with self.subTest(document=document.name, link=local):
                    self.assertTrue((document.parent / local).is_file(), local)


if __name__ == "__main__":
    unittest.main()
