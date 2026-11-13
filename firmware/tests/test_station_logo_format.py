"""Deterministic format, conversion, corruption and atomic output checks."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('station_logo_converter', ROOT / 'tools' / 'convert_station_logo.py')
converter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(converter)


class StationLogoFormatTests(unittest.TestCase):
    def test_stable_station_hash_and_filename(self):
        self.assertEqual(converter.station_file_id(''), 'cbf29ce484222325')
        self.assertEqual(converter.station_file_id('abc'), 'e71fa2190541574b')
        self.assertNotEqual(converter.station_file_id('https://radio.example/a'),
                            converter.station_file_id('https://radio.example/b'))

    def test_header_crc_size_color_and_rgb565_endianness(self):
        rgb = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255]) + bytes(64 * 64 * 3 - 9)
        logo = converter.pack_image(rgb)
        self.assertEqual(len(logo), 8208)
        self.assertTrue(converter.validate_image(logo))
        self.assertEqual(logo[:4], b'PJLG')
        self.assertEqual(struct.unpack('<BBBBII', logo[4:16])[:5], (1, 64, 64, 1, 8192))
        self.assertEqual(logo[16:22], bytes.fromhex('00f8e0071f00'))

    def test_corruption_and_malformed_size_rejected(self):
        logo = converter.pack_image(bytes(64 * 64 * 3))
        for position in (0, 4, 5, 6, 7, 8, 9, 12, 15, 16, 8207):
            with self.subTest(position=position):
                data = bytearray(logo)
                data[position] ^= 1
                self.assertFalse(converter.validate_image(data))
        for invalid in (logo[:-1], logo + b'X', b'', logo[:16]):
            self.assertFalse(converter.validate_image(invalid))
        with self.assertRaises(ValueError):
            converter.pack_image(b'abc')

    def test_output_atomic_and_no_invalid_overwrite(self):
        logo = converter.pack_image(bytes(64 * 64 * 3))
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp) / 'my-station.pjlg'
            converter.write_atomic(dest, logo)
            self.assertEqual(dest.read_bytes(), logo)
            with self.assertRaises(ValueError):
                converter.write_atomic(dest, logo[:-1])
            self.assertEqual(dest.read_bytes(), logo)
            self.assertEqual(list(Path(tmp).glob('*.tmp')), [])
            with self.assertRaises(ValueError):
                converter.write_atomic(Path(tmp) / 'bad.bin', logo)

    def test_png_converter_validates_geometry_and_mode(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest('Pillow is optional and not installed')
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp) / 'good.png'
            Image.new('RGBA', (64, 64), (255, 0, 0, 255)).save(good)
            output = converter.convert_png(good)
            self.assertTrue(converter.validate_image(output))
            self.assertEqual(output[16:18], b'\x00\xf8')
            wrong = Path(tmp) / 'wrong.png'
            Image.new('RGB', (65, 64)).save(wrong)
            with self.assertRaises(ValueError):
                converter.convert_png(wrong)


if __name__ == '__main__':
    unittest.main()
