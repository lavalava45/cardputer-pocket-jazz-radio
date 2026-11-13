"""Prevent generated installation instructions from describing retired UI."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILDER = (ROOT / "tools/build.ps1").read_text(encoding="utf-8")


class InstallReadmeKeymap(unittest.TestCase):
    def test_generated_readme_matches_current_controls(self):
        readme = BUILDER.split("$installText = @'", 1)[-1] if "$installText = @'" in BUILDER else BUILDER
        self.assertIn("V toggles the VU display", readme)
        self.assertIn("S opens TUNING SOUND FX", readme)
        self.assertIn("RADIO Left/Right tunes", readme)
        self.assertIn("single Blue Note clock", readme)
        self.assertNotIn("Optional 64x64 station logos", readme)
        self.assertNotIn("(A/D select, P playlist", readme)
        self.assertNotIn("T toggles normal/Blue Note", readme)


if __name__ == "__main__":
    unittest.main()
