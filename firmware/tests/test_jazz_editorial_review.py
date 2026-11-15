"""Keep the 2026-11-15 heterogeneous editorial spot-check tied to real data."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class EditorialReviewContract(unittest.TestCase):
    def test_exactly_twenty_individual_cases_and_no_orphaned_keys(self):
        path = ROOT / "docs/JAZZ-NOTES-EDITORIAL-REVIEW-20261115.md"
        text = path.read_text("utf-8")
        keys = re.findall(r"^\| `([a-z0-9-]+)` \|", text, re.M)
        # The report uses inline-code keys, never fuzzy artist matching.
        self.assertEqual(len(keys), 20)
        self.assertEqual(len(set(keys)), 20)
        catalog = {r.split("\t")[0] for r in
                   (ROOT / "data/jazz-encyclopedia.tsv").read_text("ascii").splitlines()}
        self.assertTrue(set(keys) <= catalog)

    def test_cross_genre_and_ensemble_cases_have_source_capsules(self):
        researched = {r.split("\t")[0] for r in
                      (ROOT / "data/jazz-researched-context.tsv").read_text("ascii").splitlines()}
        protected = {r.split("\t")[0] for r in
                     (ROOT / "data/jazz-editorial-bios.tsv").read_text("ascii").splitlines()}
        for key in ("michelle-dockery", "phil-collins", "yoko-kanno",
                    "al-green", "alan-greenspan", "viggo-mortensen",
                    "the-oscar-peterson-trio", "duke-ellington-and-his-orchestra",
                    "giorgi-vintilescu"):
            self.assertIn(key, researched)
            self.assertNotIn(key, protected)
        for key in ("miles-davis", "charlie-parker", "sun-ra"):
            self.assertIn(key, protected)


if __name__ == "__main__":
    unittest.main()
