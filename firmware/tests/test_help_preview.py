import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from importlib.util import module_from_spec, spec_from_file_location

ROOT = Path(__file__).resolve().parents[1]
SPEC = spec_from_file_location("help_preview", ROOT / "tools" / "render_help_preview.py")
MODULE = module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class HelpPreviewContract(unittest.TestCase):
    def test_preview_is_derived_from_the_firmware_table(self):
        source = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(
            encoding="utf-8"
        )
        pages = MODULE.extract_pages(source)
        self.assertEqual(len(pages), 20)
        self.assertEqual(len(pages[0]["rows"]), 4)
        self.assertEqual(len(pages[-1]["rows"]), 4)
        self.assertEqual(pages[0]["section"], "RADIO")

    def test_offline_canvas_has_native_geometry_and_keyboard_support(self):
        with TemporaryDirectory() as directory:
            result = MODULE.render(Path(directory) / "help-preview.html")
            doc = result.read_text(encoding="utf-8")
        self.assertIn("The 20 pages", doc)
        self.assertIn('width="240" height="135"', doc)
        self.assertIn('e.key==="ArrowUp"', doc)
        self.assertIn('e.key==="ArrowDown"', doc)
        self.assertIn('e.key==="Delete"', doc)
        self.assertIn('e.key==="Backspace"', doc)
        self.assertIn('"section":"RADIO"', doc)
        self.assertIn('"section":"SD AND SAFETY"', doc)
        self.assertIn("const stride=big?12:6", doc)
        self.assertNotIn("http://", doc)
        self.assertNotIn("https://", doc)


if __name__ == "__main__":
    unittest.main()
