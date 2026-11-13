"""Executable contracts for the opt-in offline PC Like importer/review flow."""
from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from importlib.util import module_from_spec, spec_from_file_location

ROOT = Path(__file__).resolve().parents[1]
SPEC = spec_from_file_location("likes_bridge", ROOT / "tools" / "likes_bridge.py")
BRIDGE = module_from_spec(SPEC)
SPEC.loader.exec_module(BRIDGE)
CONFIRMED = "https://open.spotify.com/track/6rqhFgbbKwnb9MLmUQDhG6"


class LikesBridgeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / "liked.txt"
        self.state = self.root / "likes.json"
        self.source.write_bytes(
            "2026-11-10T12:34Z\tFIP Jazz\tJohn Coltrane - Naima\n"
            "time-unknown\tSomaFM\tBjörk & Friends <not verified>\n".encode()
        )

    def test_ingest_idempotent_and_same_title_repeats_are_distinct(self):
        new, total = BRIDGE.ingest(self.source, self.state)
        self.assertEqual((new, total), (2, 2))
        before = self.state.read_bytes()
        self.assertEqual(BRIDGE.ingest(self.source, self.state), (0, 2))
        self.assertEqual(self.state.read_bytes(), before)
        repeated = self.source.read_bytes().splitlines(keepends=True)[0]
        with self.source.open("ab") as f:
            f.write(repeated)
        self.assertEqual(BRIDGE.ingest(self.source, self.state), (1, 3))
        ids = [x["id"] for x in BRIDGE._load_state(self.state)["likes"]]
        self.assertEqual(len(set(ids)), 3)
        self.assertEqual(BRIDGE.ingest(self.source, self.state), (0, 3))

    def test_broken_final_row_rejected_without_state_write(self):
        BRIDGE.ingest(self.source, self.state)
        previous = self.state.read_bytes()
        with self.source.open("ab") as f:
            f.write(b"broken incomplete tail")
        with self.assertRaisesRegex(BRIDGE.BridgeError, "incomplete"):
            BRIDGE.ingest(self.source, self.state)
        self.assertEqual(self.state.read_bytes(), previous)

    def test_changed_prefix_and_truncation_fail_closed(self):
        BRIDGE.ingest(self.source, self.state)
        previous = self.state.read_bytes()
        self.source.write_bytes(self.source.read_bytes().replace(b"FIP Jazz", b"FXP Jazz"))
        with self.assertRaisesRegex(BRIDGE.BridgeError, "changed/truncated"):
            BRIDGE.ingest(self.source, self.state)
        self.assertEqual(self.state.read_bytes(), previous)
        self.source.write_bytes(b"")
        with self.assertRaisesRegex(BRIDGE.BridgeError, "changed/truncated"):
            BRIDGE.ingest(self.source, self.state)

    def test_malformed_tsv_invalid_utf8_and_oversize_are_rejected(self):
        for content in (
            b"time-unknown\tStation\tTitle",
            b"time-unknown\tStation\tToo\tmany\n",
            b"time-unknown\tStation\tBad\x00\n",
            b"time-unknown\tStation\tBad\xff\n",
            b"time-unknown\tStation\tLong" + b"a" * 100 + b"\n",
            b"b" * (BRIDGE.MAX_LOG_BYTES + 1),
        ):
            with self.subTest(content=content[:25]):
                self.source.write_bytes(content)
                with self.assertRaises(BRIDGE.BridgeError):
                    BRIDGE.ingest(self.source, self.state)
                self.assertFalse(self.state.exists())

    def test_confirm_rejects_forged_domains_tokens_and_non_track_links(self):
        BRIDGE.ingest(self.source, self.state)
        item = BRIDGE._load_state(self.state)["likes"][0]
        previous = self.state.read_bytes()
        for bad in (
            "http://open.spotify.com/track/" + "A" * 22,
            "https://open.spotify.com.evil.org/track/" + "A" * 22,
            "https://evil.org@open.spotify.com/track/" + "A" * 22,
            "https://open.spotify.com/playlist/" + "A" * 22,
            CONFIRMED + "?access_token=secret",
            "javascript:alert(1)",
            "https://open.spotify.com/track/INVALID",
        ):
            with self.subTest(bad=bad):
                with self.assertRaises(BRIDGE.BridgeError):
                    BRIDGE.review(self.state, item["id"], "confirm", bad)
                self.assertEqual(self.state.read_bytes(), previous)
        self.assertEqual(
            BRIDGE.review(self.state, item["id"], "confirm", CONFIRMED)["status"], "confirmed"
        )
        confirmed = BRIDGE._load_state(self.state)["likes"][0]
        self.assertEqual(confirmed["spotify_track_url"], CONFIRMED)
        self.assertEqual(BRIDGE.review(self.state, item["id"], "skip")["status"], "skipped")
        self.assertIsNone(BRIDGE._load_state(self.state)["likes"][0]["spotify_track_url"])

    def test_html_has_escaped_metadata_and_no_automatic_network(self):
        BRIDGE.ingest(self.source, self.state)
        output = self.root / "report.html"
        self.assertEqual(BRIDGE.export(self.state, output, False)[0], 2)
        body = output.read_text(encoding="utf-8")
        self.assertIn("Björk &amp; Friends &lt;not verified&gt;", body)
        self.assertIn("open.spotify.com/search/Bj%C3%B6rk", body)
        self.assertIn('name="referrer" content="no-referrer"', body)
        self.assertIn("pocketjazz-likes-report-v1", body)
        self.assertNotIn("<script", body)
        self.assertNotIn("fetch(", body)
        previous = output.read_bytes()
        with self.assertRaisesRegex(BRIDGE.BridgeError, "report exists"):
            BRIDGE.export(self.state, output, False)
        self.assertEqual(output.read_bytes(), previous)
        self.assertEqual(BRIDGE.export(self.state, output, True)[0], 2)

    def test_json_export_has_no_auth_fields_and_never_modifies_source(self):
        source_before = self.source.read_bytes()
        BRIDGE.ingest(self.source, self.state)
        output = self.root / "feed.json"
        count, _ = BRIDGE.export(self.state, output, False)
        self.assertEqual(count, 2)
        feed = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(feed["schema"], 1)
        self.assertEqual(feed["report_format"], BRIDGE.REPORT_MARKER)
        self.assertEqual(len(feed["likes"]), 2)
        self.assertNotIn("client_secret", str(feed))
        self.assertNotIn("access_token", str(feed))
        self.assertEqual(self.source.read_bytes(), source_before)

    def test_invalid_state_does_not_get_overwritten(self):
        self.state.write_text('{"schema":7,"likes":[]}', encoding="utf-8")
        original = self.state.read_bytes()
        with self.assertRaisesRegex(BRIDGE.BridgeError, "schema"):
            BRIDGE.ingest(self.source, self.state)
        self.assertEqual(self.state.read_bytes(), original)

    def test_short_valid_firmware_rows_can_exceed_800(self):
        data = b"time-unknown\tS\tA\n" * 1001
        self.assertLessEqual(len(data), BRIDGE.MAX_LOG_BYTES)
        self.source.write_bytes(data)
        imported, total = BRIDGE.ingest(self.source, self.state)
        self.assertEqual((imported, total), (1001, 1001))
        self.assertEqual(BRIDGE.ingest(self.source, self.state), (0, 1001))

    def test_existing_state_rejects_injected_href_without_modifying_it(self):
        BRIDGE.ingest(self.source, self.state)
        state = json.loads(self.state.read_text(encoding="utf-8"))
        state["likes"][0]["status"] = "confirmed"
        state["likes"][0]["spotify_track_url"] = "javascript:alert(1)"
        self.state.write_text(json.dumps(state), encoding="utf-8")
        original = self.state.read_bytes()
        with self.assertRaises(BRIDGE.BridgeError):
            BRIDGE.export(self.state, self.root / "report.html", False)
        self.assertEqual(self.state.read_bytes(), original)
        self.assertFalse((self.root / "report.html").exists())

    def test_unicode_line_separators_do_not_silently_split_record(self):
        for bad in ("abc\u0085def", "abc\u2028def", "abc\u2029def"):
            with self.subTest(bad=repr(bad)):
                self.source.write_text(
                    "time-unknown\tSomaFM\t" + bad + "\n", encoding="utf-8"
                )
                with self.assertRaises(BRIDGE.BridgeError):
                    BRIDGE.ingest(self.source, self.state)
                self.assertFalse(self.state.exists())

    def test_search_unknown_and_browser_query_encoding(self):
        self.assertEqual(BRIDGE.search_links("track-unknown"), (None, None))
        spot, web = BRIDGE.search_links("Miles Davis - So What? & #1")
        self.assertEqual(spot, "https://open.spotify.com/search/Miles%20Davis%20-%20So%20What%3F%20%26%20%231")
        self.assertTrue(web.endswith("What%3F%20%26%20%231"))

    def test_export_replace_refuses_arbitrary_existing_files_and_other_states(self):
        BRIDGE.ingest(self.source, self.state)
        other = self.root / "another-likes.json"
        BRIDGE.ingest(self.source, other)
        saved_other = other.read_bytes()
        with self.assertRaisesRegex(BRIDGE.BridgeError, "not a generated"):
            BRIDGE.export(self.state, other, True)
        self.assertEqual(other.read_bytes(), saved_other)
        arbitrary = self.root / "notes.html"
        arbitrary.write_text("<h1>personal notes</h1>", encoding="utf-8")
        before = arbitrary.read_bytes()
        with self.assertRaisesRegex(BRIDGE.BridgeError, "not a generated"):
            BRIDGE.export(self.state, arbitrary, True)
        self.assertEqual(arbitrary.read_bytes(), before)
        output = self.root / "report.json"
        BRIDGE.export(self.state, output, False)
        self.assertEqual(BRIDGE.export(self.state, output, True)[0], 2)
        self.assertEqual(json.loads(output.read_text())["report_format"],
                         BRIDGE.REPORT_MARKER)

    def test_state_lock_fails_closed_when_another_operation_is_active(self):
        BRIDGE.ingest(self.source, self.state)
        before = self.state.read_bytes()
        with BRIDGE._state_guard(self.state):
            with self.assertRaisesRegex(BRIDGE.BridgeError, "locked"):
                BRIDGE.ingest(self.source, self.state)
            item = BRIDGE._load_state(self.state)["likes"][0]
            with self.assertRaisesRegex(BRIDGE.BridgeError, "locked"):
                BRIDGE.review(self.state, item["id"], "confirm", CONFIRMED)
            with self.assertRaisesRegex(BRIDGE.BridgeError, "locked"):
                BRIDGE.export(self.state, self.root / "report.json", False)
        self.assertEqual(self.state.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
