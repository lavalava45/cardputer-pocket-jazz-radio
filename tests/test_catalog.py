"""Offline, non-network tests for the public curated catalog."""
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from scripts import catalog_manifest as catalog
from scripts import verify_streams


class StationCatalogTests(unittest.TestCase):
    def test_manifest_matches_every_committed_byte(self):
        data = json.loads((catalog.ROOT / "manifest.json").read_text("utf-8"))
        self.assertEqual(data, catalog.expected_manifest())
        self.assertEqual(data["catalog_version"], catalog.CATALOG_VERSION)
        self.assertEqual(data["stations"], 56)
        self.assertEqual(data["alternate_urls"], 6)
        self.assertEqual(data["playlists"], 5)
        self.assertEqual(data["pc_verification"]["passed"], 62)
        self.assertEqual(data["pc_verification"]["failed"], 0)

    def test_no_personal_playlist_or_private_material_in_distribution(self):
        filenames = sorted(path.name for path in (catalog.ROOT / "catalog").glob("*.m3u"))
        self.assertEqual(filenames, list(catalog.FILES))
        self.assertNotIn("04-my-stations.m3u", filenames)
        self.assertNotIn("04-my-stations.m3u", (catalog.ROOT / "manifest.json").read_text("utf-8"))

    def test_m3u_is_bounded_for_cardputer_and_alt_urls_are_sparse(self):
        stations = sum((catalog.parse_playlist(catalog.ROOT / "catalog" / name)
                        for name in catalog.FILES), [])
        self.assertEqual(len(stations), 56)
        self.assertEqual(sum(bool(s["alternate"]) for s in stations), 6)
        self.assertLessEqual(len(stations), 64)
        self.assertEqual(len({s["url"] for s in stations}), 56)
        self.assertTrue(all(s["url"].startswith("https://") for s in stations))

    def test_rejects_untrusted_url_and_bad_m3u_structures(self):
        for bad in ("http://example.org/radio.mp3", "https://user:pass@example.org/x",
                    "https://example.org/with space", "ftp://example.org/radio",
                    "https://example.org/" + "x" * 200, "https:///no-host"):
            with self.subTest(bad=bad[:40]):
                with self.assertRaises(ValueError):
                    catalog.validate_url(bad)
        with TemporaryDirectory() as directory:
            sample = Path(directory) / "bad.m3u"
            for content in (
                "https://radio.example/stream\n",
                "#EXTM3U\nhttps://radio.example/stream\n",
                "#EXTM3U\n#EXTINF:-1,Station\n",
                "#EXTM3U\n#PJ-ALT:https://radio.example/alt\n",
                "#EXTM3U\n#EXTINF:-1,Station\n#EXTINF:-1,Other\n",
            ):
                sample.write_text(content, encoding="utf-8")
                with self.subTest(content=content[:48]):
                    with self.assertRaises(ValueError):
                        catalog.parse_playlist(sample)

    def test_checksum_changes_are_detected(self):
        with TemporaryDirectory() as directory:
            fake = Path(directory)
            for name in catalog.FILES:
                target = fake / "catalog" / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((catalog.ROOT / "catalog" / name).read_bytes())
            report = fake / catalog.REPORT
            report.parent.mkdir(parents=True, exist_ok=True)
            report.write_bytes((catalog.ROOT / catalog.REPORT).read_bytes())
            with patch.object(catalog, "ROOT", fake):
                clean = catalog.expected_manifest()
                target = fake / "catalog" / catalog.FILES[0]
                target.write_bytes(target.read_bytes() + b"\n# edited\n")
                dirty = catalog.expected_manifest()
                self.assertNotEqual(clean["files"][0]["sha256"],
                                    dirty["files"][0]["sha256"])

    def test_pc_verification_parser_strips_icy_metadata(self):
        extractor = verify_streams.IcyAudioSample(4)
        wire = b"abcd" + b"\x01" + b"ignored-metadata" + b"efgh"
        for b in wire:
            extractor.feed(bytes((b,)))
        self.assertEqual(bytes(extractor.audio), b"abcdefgh")
        self.assertEqual(verify_streams.DEVICE_HEADERS["Icy-MetaData"], "1")
        self.assertEqual(verify_streams.DEVICE_HEADERS["User-Agent"], "ESP32HTTPClient")


if __name__ == "__main__":
    unittest.main()
