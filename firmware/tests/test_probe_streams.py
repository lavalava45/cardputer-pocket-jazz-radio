import unittest
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = spec_from_file_location("probe_streams", ROOT / "tools" / "probe_streams.py")
PROBE = module_from_spec(SPEC)
SPEC.loader.exec_module(PROBE)


class StreamProbeHostOnly(unittest.TestCase):
    def test_plain_mp3_metadata_absent(self):
        d = PROBE.IcyDemux(0)
        self.assertEqual(d.feed(b"\xff\xfb\x90\x64"), 4)
        self.assertEqual(d.feed(b"\x00\xff"), 2)
        self.assertEqual(d.audio_bytes, 6)
        self.assertEqual(d.metadata_blocks, 0)

    def test_icy_metadata_fragmented_at_every_possible_boundary(self):
        metadata = b"StreamTitle='X';"
        self.assertEqual(len(metadata), 16)
        raw = b"ABCD" + b"\x01" + metadata + b"WXYZ" + b"\x00" + b"EFGH"
        for chunk_size in range(1, len(raw) + 1):
            with self.subTest(size=chunk_size):
                d = PROBE.IcyDemux(4)
                for i in range(0, len(raw), chunk_size):
                    d.feed(raw[i:i + chunk_size])
                self.assertEqual(d.audio_bytes, 12)
                self.assertEqual(d.metadata_blocks, 2)
                self.assertEqual(d.metadata_bytes, 16)
                self.assertEqual(d.audio_until_metadata, 0)
                self.assertTrue(d.expect_length)

    def test_empty_and_invalid_metadata_intervals(self):
        self.assertEqual(PROBE.bounded_interval(None), 0)
        self.assertEqual(PROBE.bounded_interval(" 16000 "), 16000)
        for bad in ("0", "-1", "1.5", "not-a-number", "1000001"):
            with self.subTest(raw=bad):
                with self.assertRaises(ValueError):
                    PROBE.bounded_interval(bad)
        with self.assertRaises(ValueError):
            PROBE.IcyDemux(-1)

    def test_only_five_known_https_urls_accepted(self):
        self.assertEqual(len(PROBE.ENDPOINTS), 5)
        with self.assertRaises(ValueError):
            PROBE.probe_one("private", "http://192.168.0.1/", 1)
        with self.assertRaises(ValueError):
            PROBE.probe_one("fake", "https://example.org/not-our-station", 1)
        with self.assertRaises(ValueError):
            PROBE.probe_one(PROBE.ENDPOINTS[0][0], PROBE.ENDPOINTS[0][1], 0.1)


if __name__ == "__main__":
    unittest.main()
