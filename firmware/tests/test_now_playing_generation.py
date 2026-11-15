"""Read-only tests for deterministic, curated flash hints.

The test runner must NEVER rewrite PocketJazzRadio/JazzNowPlaying.h to
silently 'fix' drift. Only the separately invoked generator CLI may do that.
"""
from pathlib import Path
from unittest import TestCase
from unittest.mock import patch
import sys
import tempfile
import shutil

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
import build_now_playing_hints as hints


class NowPlayingReadOnlyGeneration(TestCase):
    def test_render_twice_yields_same_header_without_touching_file(self):
        target = ROOT / "PocketJazzRadio/JazzNowPlaying.h"
        before = target.read_bytes()
        stat = target.stat()
        rendered = hints.render_header()
        self.assertEqual(rendered, hints.render_header())
        # Canonical JSON/C++ quoting is essential for exact artist aliases.
        self.assertIn(r'Johnny \"Hammond\" Smith', rendered)
        self.assertEqual(rendered.encode("ascii"), before)
        self.assertEqual(before, target.read_bytes())
        self.assertEqual(stat.st_mtime_ns, target.stat().st_mtime_ns)

    def test_corrupted_input_is_detected_without_rewriting_any_header(self):
        target = ROOT / "PocketJazzRadio/JazzNowPlaying.h"
        original = target.read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            data = tmp / "data"
            data.mkdir()
            for name in ("jazz-editorial-bios.tsv", "jazz-encyclopedia.tsv",
                         "jazz-researched-context.tsv"):
                shutil.copyfile(ROOT / "data" / name, data / name)
            previous = (data / "jazz-editorial-bios.tsv").read_text("ascii")
            self.assertIn("miles-davis", previous)
            (data / "jazz-editorial-bios.tsv").write_text(
                previous.replace(
                    "Trumpeter and bandleader who repeatedly reshaped modern jazz",
                    "Incorrect and unverifiable new biography",
                    1,
                ),
                encoding="ascii",
            )
            with patch.object(hints, "ROOT", tmp):
                with self.assertRaises(AssertionError):
                    hints.render_header()
            self.assertFalse((tmp / "PocketJazzRadio").exists())
        self.assertEqual(original, target.read_bytes())
