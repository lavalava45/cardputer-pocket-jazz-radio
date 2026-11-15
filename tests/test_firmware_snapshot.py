"""Ensure the published source snapshot stays byte-identical to the release."""
from pathlib import Path
import importlib.util
import hashlib
from tempfile import TemporaryDirectory
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'scripts/verify_firmware_snapshot.py'
spec = importlib.util.spec_from_file_location('frozen_snapshot', MODULE)
snapshot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(snapshot)


class FrozenFirmwareSource(unittest.TestCase):
    def test_source_manifest_sha256(self):
        self.assertGreater(snapshot.verify(), 200)

    def test_current_firmware_is_v0_8_0(self):
        header = (ROOT / 'firmware/PocketJazzRadio/BuildVersion.h').read_text('utf-8')
        self.assertIn('#define POCKETJAZZ_FIRMWARE_VERSION "v0.8.0"', header)

    def test_unknown_source_must_be_hashed_even_if_bytecode_exists(self):
        # The exact integrity gate remains strict for all non-generated files.
        # Its generated-bytecode exemption is NOT a general preview exclusion.
        saved_source, saved_manifest = snapshot.SOURCE, snapshot.MANIFEST
        try:
            with TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / 'firmware'
                source.mkdir()
                data = b'#include <cstdint>\n'
                (source / 'example.cpp').write_bytes(data)
                manifest = source / 'SHA256-SOURCES.txt'
                manifest.write_text(
                    hashlib.sha256(data).hexdigest() + '  example.cpp\n', encoding='utf-8'
                )
                snapshot.SOURCE, snapshot.MANIFEST = source, manifest
                self.assertEqual(snapshot.verify(), 1)
                cache = source / 'tests' / '__pycache__'
                cache.mkdir(parents=True)
                (cache / 'test.cpython-311.pyc').write_bytes(b'generated')
                self.assertEqual(snapshot.verify(), 1)
                generated_preview = source / 'preview'
                generated_preview.mkdir()
                (generated_preview / 'help-v07k.html').write_text('<!doctype html>')
                with self.assertRaisesRegex(RuntimeError, 'Unlisted source files'):
                    snapshot.verify()
        finally:
            snapshot.SOURCE, snapshot.MANIFEST = saved_source, saved_manifest


if __name__ == '__main__':
    unittest.main()
