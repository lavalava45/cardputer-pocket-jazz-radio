"""Protect the compact, source-labelled firmware screenshots in public docs."""

import re
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCREENS = ("radio", "browse", "qr", "clock", "mood", "fx", "help", "diagnostics")
DOCUMENTS = (
    ROOT / "README.md",
    ROOT / "docs/FEATURE-TOUR.md",
    ROOT / "docs/EMULATOR.md",
    ROOT / "docs/USER-GUIDE.md",
)


class DocsGalleryTests(unittest.TestCase):
    def test_screenshot_dimensions_match_displayed_width(self):
        for screen in SCREENS:
            with self.subTest(screen=screen):
                path = ROOT / "docs/images" / f"emulator-v07z13-{screen}.png"
                data = path.read_bytes()
                self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
                self.assertEqual(data[12:16], b"IHDR")
                self.assertEqual(struct.unpack(">II", data[16:24]), (360, 203))

    def test_gallery_references_are_not_stretched(self):
        for document in DOCUMENTS:
            contents = document.read_text(encoding="utf-8")
            for tag in re.findall(r"<img\s+[^>]*>", contents, flags=re.I):
                if "emulator-v07z13-" in tag and "cover.png" not in tag:
                    self.assertIn('width="360"', tag, document)
                    self.assertIn('height="203"', tag, document)
                    self.assertNotIn('width="100%"', tag, document)
            self.assertNotIn('width="700"', contents, document)

    def test_current_readme_uses_real_emulator_not_old_mockups(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        for old in ("radio-preview.png", "browse-preview.png", "clock-preview.png", "help-preview.png"):
            self.assertNotIn(old, readme)
        self.assertIn("actual", readme.lower())
        self.assertIn("360 px wide", readme)
        self.assertIn("v0.7z14", readme)
        self.assertIn("v0.7z13", readme)
        self.assertIn("51", readme)

    def test_local_links_in_edited_docs_exist(self):
        for document in DOCUMENTS:
            contents = document.read_text(encoding="utf-8")
            references = re.findall(r"<img[^>]+src=\"([^\"]+)\"", contents, flags=re.I)
            references += re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", contents)
            for reference in references:
                url = reference.split("#", 1)[0].strip()
                if not url or "://" in url or url.startswith("#") or url.startswith("data:"):
                    continue
                with self.subTest(document=document.name, link=url):
                    self.assertTrue((document.parent / url).exists(), url)

    def test_photo_caption_and_runtime_provenance(self):
        provenance = (ROOT / "docs/EMULATOR.md").read_text(encoding="utf-8")
        self.assertIn("240 × 135", provenance)
        self.assertIn("11214c1a9a0498878242e1315af6f3fe8c42668aee22783a125001dd648c3654", provenance)
        self.assertIn("e4048df599d06019e2d6abf22ce92cebe805eb286317b5176ed2008576f97516", provenance)
        self.assertIn("headless", provenance)
        self.assertIn("not the newer public v0.7z14 download", provenance)


if __name__ == "__main__":
    unittest.main()
