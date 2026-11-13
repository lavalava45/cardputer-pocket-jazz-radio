"""Safety/negative checks for the opt-in external measurement utility."""
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "tools/probe_station_throughput.py"
SOURCE = PROBE.read_text(encoding="utf-8")


class StationProbeContract(unittest.TestCase):
    def test_bounded_https_only_and_does_not_save_audio(self):
        self.assertIn('if urlsplit(newurl).scheme != "https":', SOURCE)
        self.assertIn("CAP_BYTES = 2_000_000", SOURCE)
        self.assertIn("response.read(min(4096", SOURCE)
        self.assertIn('args.output.open("x", encoding="utf-8")', SOURCE)
        self.assertNotIn("setInsecure", SOURCE)
        self.assertNotIn("verify=False", SOURCE)
        self.assertNotIn(".mp3\", \"wb", SOURCE)

    def test_invalid_duration_exits_without_accessing_network(self):
        for bad in ("1", "46", "-5"):
            with self.subTest(seconds=bad):
                result = subprocess.run(
                    [sys.executable, "-B", str(PROBE), "--seconds", bad,
                     "--station", "Jazz24"],
                    cwd=ROOT, capture_output=True, text=True, timeout=7
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("--seconds must be between 2 and 45", result.stderr)


if __name__ == "__main__":
    unittest.main()
