"""Offline unit tests for the opt-in (real HTTPS) MP3 station verifier."""
import importlib.util
import io
import json
import re
from pathlib import Path
import unittest


MODULE = Path(__file__).resolve().parents[1] / "tools/verify_station_streams.py"
SPEC = importlib.util.spec_from_file_location("verify_station_streams", MODULE)
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class Mp3StationProbeTests(unittest.TestCase):
    def test_headers_match_arduino_esp32_httpclient_and_secure_icy_source(self):
        # The production source never overrides the Arduino 3.3.9 default UA.
        source = (MODULE.parents[1] / "PocketJazzRadio/SecureICYSource.cpp").read_text("utf-8")
        self.assertIn('http_.addHeader("Icy-MetaData", "1")', source)
        self.assertIn('http_.setReuse(false)', source)
        self.assertNotIn('http_.setUserAgent(', source)
        self.assertEqual(VERIFY.DEVICE_HEADERS, {
            "User-Agent": "ESP32HTTPClient",
            "Icy-MetaData": "1",
            "Accept-Encoding": "identity;q=1,chunked;q=0.1,*;q=0",
            "Connection": "close",
            "Accept": None,
        })
        if VERIFY.requests is not None:
            session = VERIFY.requests.Session()
            prepared = session.prepare_request(VERIFY.requests.Request(
                "GET", "https://radio.example/audio", headers=VERIFY.DEVICE_HEADERS))
            self.assertNotIn("Accept", prepared.headers)
            self.assertEqual(prepared.headers["Icy-MetaData"], "1")

    def test_icy_interval_header_matches_firmware_limits(self):
        self.assertEqual(VERIFY.parse_icy_interval(None), 0)
        self.assertEqual(VERIFY.parse_icy_interval(""), 0)
        self.assertEqual(VERIFY.parse_icy_interval("0000"), 0)
        self.assertEqual(VERIFY.parse_icy_interval("00000008192"), 8192)
        self.assertEqual(VERIFY.parse_icy_interval("1048576"), 1048576)
        for bad in ("-1", "+1", " 8192", "8192 ", "1.0", "1048577", "1" * 10000):
            with self.subTest(bad=bad[:30]):
                with self.assertRaises(ValueError):
                    VERIFY.parse_icy_interval(bad)

    def test_strips_icy_metadata_even_when_network_chunks_split_length_and_title(self):
        frame = bytes.fromhex("fffb9064") + bytes(413)
        audio = (frame * 20)[:VERIFY.SAMPLE_BYTES]
        interval = 500
        wire = bytearray()
        metadata = b"StreamTitle='Test artist - Test song';"
        padded = metadata.ljust(((len(metadata) + 15) // 16) * 16, b"\x00")
        for at in range(0, len(audio), interval):
            if at:
                wire.append(len(padded) // 16)
                wire.extend(padded)
            wire.extend(audio[at:at + interval])
        raw = VERIFY.IcyAudioSample(interval)
        for byte in wire:
            raw.feed(bytes((byte,)))
        self.assertTrue(raw.complete)
        self.assertEqual(bytes(raw.audio), audio)
        self.assertEqual(VERIFY.mp3_signature(bytes(raw.audio))["sample_rate_hz"], 44100)
        self.assertIsNone(VERIFY.mp3_signature(bytes(wire[:VERIFY.SAMPLE_BYTES])))

    def test_https_probe_demuxes_icy_and_sends_exact_request_headers(self):
        frame = bytes.fromhex("fffb9064") + bytes(413)
        audio = (frame * 20)[:VERIFY.SAMPLE_BYTES]
        wire = bytearray()
        for at in range(0, len(audio), 700):
            if at:
                wire.extend(b"\x01" + b"tag-testing-1234")  # one 16-byte ICY block
            wire.extend(audio[at:at + 700])

        class Response:
            url = "https://radio.example/audio"
            status_code = 200
            history = []
            headers = {"Content-Type": "audio/mpeg", "icy-metaint": "700"}
            raw = io.BytesIO(bytes(wire))

            def __enter__(self): return self
            def __exit__(self, *_): pass

        class FakeRequests:
            calls = []

            @classmethod
            def get(cls, url, **kwargs):
                cls.calls.append((url, kwargs))
                return Response()

        original = VERIFY.requests
        try:
            VERIFY.requests = FakeRequests
            result = VERIFY.request_once("https://radio.example/audio")
        finally:
            VERIFY.requests = original
        self.assertEqual(result["status"], "PASS", result)
        self.assertEqual(result["icy_interval"], 700)
        self.assertEqual(result["bytes_examined"], VERIFY.SAMPLE_BYTES)
        self.assertGreater(result["raw_bytes_received"], VERIFY.SAMPLE_BYTES)
        self.assertEqual(FakeRequests.calls[0][1]["headers"], VERIFY.DEVICE_HEADERS)
        self.assertTrue(FakeRequests.calls[0][1]["verify"])
        self.assertTrue(FakeRequests.calls[0][1]["stream"])

    def test_three_aligned_mp3_frames_confirm_mpeg1_layer3(self):
        frame = bytes.fromhex("fffb9064") + bytes(413)
        result = VERIFY.mp3_signature(b"xyz" + frame * 3)
        self.assertEqual(result["sample_rate_hz"], 44100)
        self.assertEqual(result["bitrate_kbps_first_frame"], 128)
        self.assertEqual(result["first_frame_offset"], 3)

    def test_false_positive_mp3_prefix_does_not_pass(self):
        frame = bytes.fromhex("fffb9064") + bytes(413)
        self.assertIsNone(VERIFY.mp3_signature(frame + b"<html>Not MP3</html>"))
        self.assertIsNone(VERIFY.mp3_signature(b"ID3" + bytes(8192)))
        self.assertIsNone(VERIFY.mp3_signature(bytes.fromhex("fff19064") * 400))

    def test_candidate_playlists_have_unique_primaries_and_valid_backups(self):
        items = VERIFY.playlist_items(MODULE.parents[1] / "playlists")
        primary = [x for x in items if x["kind"] == "primary"]
        backup = [x for x in items if x["kind"] == "backup"]
        self.assertEqual(len(primary), 56)
        self.assertEqual(len(backup), 6)
        self.assertEqual(len(set(x["url"] for x in primary)), 56)
        self.assertTrue(all(x["url"].startswith("https://") for x in items))
        self.assertTrue(all(len(x["url"].encode()) <= 191 for x in items))

    def test_aac_response_and_https_downgrade_are_rejected(self):
        class Response:
            status_code = 200
            url = "https://example.com/stream"
            history = []
            headers = {"Content-Type": "audio/aac"}

            def __enter__(self): return self
            def __exit__(self, *_): pass

        class FakeRequests:
            @staticmethod
            def get(*args, **kwargs):
                return Response()

        original = VERIFY.requests
        try:
            VERIFY.requests = FakeRequests
            self.assertEqual(VERIFY.request_once("https://example.com/stream")["status"], "FAIL")
            self.assertEqual(VERIFY.request_once("http://example.com/stream")["status"], "FAIL")
            Response.headers = {"Content-Type": "audio/mpeg"}
            Response.url = "http://example.com/stream"
            self.assertEqual(VERIFY.request_once("https://example.com/stream")["status"], "FAIL")
        finally:
            VERIFY.requests = original

    def test_historical_44_station_network_report_remains_immutable(self):
        report_path = MODULE.parents[1] / "docs/STATION-STREAM-CHECK-20261110.json"
        report = json.loads(report_path.read_text("utf-8"))
        rows = report["streams"]
        checked_map = {(i["playlist"], i["station"], i["kind"], i["url"]) for i in rows}
        self.assertEqual(len(checked_map), 50)
        self.assertEqual(len(rows), 50)
        self.assertEqual(report["counts"], {"primary": 44, "backup": 6,
                                            "passed": 50, "failed": 0})
        for item in rows:
            self.assertEqual(item["status"], "PASS", item)
            self.assertEqual(item["http"], 200)
            self.assertIn(item["content_type"], VERIFY.CONTENT_TYPES)
            self.assertEqual(item["bytes_examined"], VERIFY.SAMPLE_BYTES)
            self.assertTrue(item["final_url"].startswith("https://"))
            self.assertIn(item["mp3"]["sample_rate_hz"], (44100, 48000))

    def test_current_56_catalog_matches_44_snapshot_minus_blues_plus_14_probed(self):
        root = MODULE.parents[1]
        prior = json.loads((root / "docs/STATION-STREAM-CHECK-20261110.json").read_text("utf-8"))
        old_primaries = {item["url"] for item in prior["streams"] if item["kind"] == "primary"}
        old_backups = {item["url"] for item in prior["streams"] if item["kind"] == "backup"}
        banned = {
            "https://listen.181fm.com/181-blues_128k.mp3",
            "https://jazzblues.ice.infomaniak.ch/jazzblues-high.mp3",
        }
        self.assertEqual(len(old_primaries), 44)
        self.assertTrue(banned.issubset(old_primaries))

        # This is the reviewed, timestamped new-PC-probe list, not the old 44
        # station report presented as if it verified the current catalog.
        md = (root / "docs/STATION-CATALOG-56-20261111.md").read_text("utf-8")
        new_urls = set(re.findall(r"\|\s*`(https://[^`]+)`\s*\|", md))
        self.assertEqual(len(new_urls), 14)
        self.assertTrue(all(url.startswith("https://") for url in new_urls))
        self.assertFalse(old_primaries & new_urls)

        current = VERIFY.playlist_items(root / "playlists")
        primary_urls = {item["url"] for item in current if item["kind"] == "primary"}
        backup_urls = {item["url"] for item in current if item["kind"] == "backup"}
        self.assertEqual(primary_urls, (old_primaries - banned) | new_urls)
        self.assertEqual(backup_urls, old_backups)
        self.assertEqual(len(current), 62)


if __name__ == "__main__":
    unittest.main()
