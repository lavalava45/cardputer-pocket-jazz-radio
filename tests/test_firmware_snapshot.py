"""Ensure the published source snapshot stays byte-identical to the release."""
from pathlib import Path
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'scripts/verify_firmware_snapshot.py'
spec = importlib.util.spec_from_file_location('frozen_snapshot', MODULE)
snapshot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(snapshot)


class FrozenFirmwareSource(unittest.TestCase):
    def test_source_manifest_sha256(self):
        self.assertGreater(snapshot.verify(), 200)

    def test_current_firmware_is_v0_7z10(self):
        header = (ROOT / 'firmware/PocketJazzRadio/BuildVersion.h').read_text('utf-8')
        self.assertIn('0.7z10', header)


if __name__ == '__main__':
    unittest.main()
