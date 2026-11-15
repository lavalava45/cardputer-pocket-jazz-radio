import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from diary_to_html import render_diary


class DiaryHtmlContract(unittest.TestCase):
    def test_safe_encoded_export_preserves_metadata(self):
        html = render_diary('2026-11-14T17:30:00Z\tStation One\tJohn Coltrane - A Love Supreme\tExplore later\n')
        self.assertIn('My Jazz Diary', html)
        self.assertIn('John Coltrane - A Love Supreme', html)
        self.assertIn('musicbrainz.org/search?', html)
        self.assertIn('Explore later', html)
        self.assertNotIn('<script', html)

    def test_markup_and_links_are_escaped(self):
        html = render_diary('uptime:50s\t<evil>\t<img src=x onerror=alert(1)>\tBeautiful solo\n')
        self.assertNotIn('<evil>', html)
        self.assertNotIn('<img', html)
        self.assertIn('&lt;evil&gt;', html)
        self.assertIn('&lt;img', html)
        self.assertNotIn('<script', html)

    def test_bad_rows_and_private_tags_rejected(self):
        for raw in ('2026\tfoo\tbad\tUnknown\n', 'two\tfields\n'):
            with self.assertRaises(ValueError):
                render_diary(raw)


if __name__ == '__main__':
    unittest.main()