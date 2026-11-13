"""Offline, deterministic contracts for the v0.3 MusicBrainz note generator.

No network, firmware compilation, hardware access, or release mutation.
Use `python tools/build_jazz_notes.py --verify-only` separately for live API checks.
"""

import copy
import csv
import io
import importlib.util
import hashlib
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "tools" / "build_jazz_notes.py"
SPEC = importlib.util.spec_from_file_location("pocketjazz_build_notes", MODULE)
notes = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(notes)

ARTIST_ID = "561d854a-6a28-4aa7-8c99-323e6ce46c2a"
ALBUM_ID = "8e8a594f-2175-38c7-a871-abb68ec363e7"


def valid_data():
    row = dict(zip(notes.FIELDS, (ARTIST_ID, "Miles Davis", ALBUM_ID, "Kind of Blue")))
    artist = {
        "id": ARTIST_ID,
        "name": "Miles Davis",
        "type": "Person",
        "disambiguation": "jazz trumpeter, bandleader, songwriter",
        "life-span": {"begin": "1926-05-26", "end": "1991-09-28"},
        "relations": [],
    }
    album = {
        "id": ALBUM_ID,
        "title": "Kind of Blue",
        "primary-type": "Album",
        "artist-credit": [{"artist": {"id": ARTIST_ID, "name": "Miles Davis"}}],
    }
    return row, artist, album


def fetch_from(artist, album):
    def fetch(entity, mbid, includes):
        if entity == "artist":
            assert (mbid, includes) == (ARTIST_ID, "instrument-rels")
            return copy.deepcopy(artist)
        assert (entity, mbid, includes) == ("release-group", ALBUM_ID, "artist-credits")
        return copy.deepcopy(album)
    return fetch


class TextAndSerializationTests(unittest.TestCase):
    def test_nfc_normalization_and_byte_limits(self):
        self.assertEqual(notes.clean("  Cafe\u0301  ", max_bytes=6, label="title"), "Café")
        with self.assertRaises(notes.InvalidRecord):
            notes.clean("Café", max_bytes=4, label="title")
        self.assertEqual(notes.artist_key("Émile François"), "emile-francois")
        self.assertEqual(notes.verified_year("1926-05-26"), "1926")
        self.assertIsNone(notes.verified_year("yesterday"))

    def test_controls_and_formula_injection_are_rejected_even_at_edges(self):
        values = ("A\tB", "A\r\nB", "A\n", "\rA", " A\t ", "A\x00B", "\ufeffA",
                  "\u202eA", "A\u2028B", "=HYPERLINK(1)", "+1+1", "-1+1", "@SUM(A1)")
        for value in values:
            with self.subTest(repr=repr(value)), self.assertRaises(notes.InvalidRecord):
                notes.clean(value, max_bytes=120, label="field")

    def test_render_requires_exactly_seven_valid_normalized_fields(self):
        row, artist, album = valid_data()
        fields = notes.verify(row, fetch_from(artist, album))
        rendered = notes.render([fields])
        self.assertEqual(rendered.count(b"\t"), 6)
        self.assertEqual(rendered.count(b"\n"), 1)
        self.assertNotIn(b"\r", rendered)
        self.assertFalse(rendered.startswith(b"\xef\xbb\xbf"))
        for bad in (fields[:6], fields + ("extra",),
                    ("bad\r\nkey",) + fields[1:],
                    ("=bad",) + fields[1:],
                    ("a" * 49,) + fields[1:]):
            with self.subTest(bad=bad), self.assertRaises(notes.InvalidRecord):
                notes.render([bad])

    def test_ascii_output_rejects_lossy_non_ascii_source(self):
        row, artist, album = valid_data()
        artist["disambiguation"] = "trompétiste"
        with self.assertRaises(notes.InvalidRecord):
            notes.verify(row, fetch_from(artist, album))
        fields = list(notes.verify(row, fetch_from(valid_data()[1], album)))
        fields[1] = "trompétiste; born 1926"
        with self.assertRaises(notes.InvalidRecord):
            notes.render([fields])

    def test_fnv1a_reference_vectors_and_invalid_key(self):
        self.assertEqual(notes.fnv1a32("a"), 0xE40C292C)
        self.assertEqual(notes.fnv1a32("foobar"), 0xBF9CF968)
        for key in ("", "Miles Davis", "A", "é", "abc\tdef", "--abc"):
            with self.subTest(key=key), self.assertRaises(notes.InvalidRecord):
                notes.fnv1a32(key)

    def test_index_real_byte_offsets_and_sorted_hashes(self):
        row, artist, album = valid_data()
        fields = notes.verify(row, fetch_from(artist, album))
        other = ("abc",) + fields[1:]
        tsv = notes.render([fields, other])
        idx = notes.render_index(tsv)
        notes.validate_index(tsv, idx)
        lines = idx.decode("ascii").splitlines()
        self.assertEqual(lines[0].split("\t"),
                         [notes.INDEX_MAGIC, "1", "fnv1a32", hashlib.sha256(tsv).hexdigest(), "2"])
        offsets = {line.split("\t")[2]: int(line.split("\t")[1]) for line in lines[1:]}
        self.assertEqual(offsets["miles-davis"], 0)
        self.assertEqual(offsets["abc"], len(notes.render([fields])))
        self.assertEqual([int(line.split("\t")[0], 16) for line in lines[1:]],
                         sorted(int(line.split("\t")[0], 16) for line in lines[1:]))

    def test_index_rejects_stale_digest_corrupt_hash_offset_and_colliding_key(self):
        row, artist, album = valid_data()
        tsv = notes.render([notes.verify(row, fetch_from(artist, album))])
        idx = notes.render_index(tsv)
        samples = [
            (tsv + b"garbage", idx),
            (tsv, idx.replace(b"fnv1a32", b"wrongfnv")),
            (tsv, idx.replace(b"miles-davis", b"miles-jones")),
            (tsv, idx.replace(b"\t0\tmiles-davis", b"\t1\tmiles-davis")),
            (tsv, idx.replace(f'{notes.fnv1a32("miles-davis"):08x}'.encode(), b"00000000")),
        ]
        for payload, index in samples:
            with self.subTest(index=index), self.assertRaises(notes.InvalidRecord):
                notes.validate_index(payload, index)

    def test_index_missing_is_safe_tsv_scan_fallback(self):
        # A missing companion file is explicitly not a fatal TSV condition.
        row, artist, album = valid_data()
        tsv = notes.render([notes.verify(row, fetch_from(artist, album))])
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "artist-notes.tsv"
            target.write_bytes(tsv)
            self.assertFalse(target.with_suffix(".idx").exists())
            result = [r.split(b"\t", 1)[0] for r in target.read_bytes().splitlines()]
            self.assertIn(b"miles-davis", result)

    def test_pair_is_staged_and_index_matches_current_tsv(self):
        row, artist, album = valid_data()
        tsv = notes.render([notes.verify(row, fetch_from(artist, album))])
        idx = notes.render_index(tsv)
        with tempfile.TemporaryDirectory() as tmp:
            tsv_path = Path(tmp) / "artist-notes.tsv"
            idx_path = Path(tmp) / "artist-notes.idx"
            notes.save_pair(tsv_path, idx_path, tsv, idx)
            self.assertEqual(tsv_path.read_bytes(), tsv)
            self.assertEqual(idx_path.read_bytes(), idx)
            self.assertFalse(list(Path(tmp).glob("*.tmp")))
            with self.assertRaises(notes.InvalidRecord):
                notes.save_pair(tsv_path, idx_path, tsv + b"\n", idx)
            self.assertEqual((tsv_path.read_bytes(), idx_path.read_bytes()), (tsv, idx))

    def test_crlf_csv_accepts_record_boundaries_not_embedded_field_newlines(self):
        row, artist, album = valid_data()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "artists.csv"
            header = ",".join(notes.FIELDS)
            line = ",".join(row[name] for name in notes.FIELDS)
            path.write_bytes((header + "\r\n" + line + "\r\n").encode("utf-8"))
            self.assertEqual(notes.load_input(path), [row])

            malicious = header + "\r\n" + f'{ARTIST_ID},"Miles\r\nDavis",{ALBUM_ID},Kind of Blue\r\n'
            path.write_bytes(malicious.encode("utf-8"))
            input_rows = notes.load_input(path)
            self.assertEqual(len(input_rows), 1)
            self.assertEqual(notes.generate(input_rows, fetch_from(artist, album))[0], [])

    def test_csv_wrong_headers_extra_field_and_row_cap(self):
        row, _, _ = valid_data()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "artists.csv"
            path.write_text("artist_name,album_title\nMiles Davis,Kind of Blue\n", encoding="utf-8")
            with self.assertRaises(notes.InvalidRecord):
                notes.load_input(path)
            content = ",".join(notes.FIELDS) + "\n" + ",".join(row.values()) + ",extra\n"
            path.write_text(content, encoding="utf-8")
            with self.assertRaises(notes.InvalidRecord):
                notes.load_input(path)
            line = ",".join(row[name] for name in notes.FIELDS)
            path.write_text(",".join(notes.FIELDS) + "\n" + (line + "\n") * (notes.MAX_INPUT_ROWS + 1), encoding="utf-8")
            with self.assertRaises(notes.InvalidRecord):
                notes.load_input(path)


class SourceAuthenticityTests(unittest.TestCase):
    def test_verified_record_only_uses_core_fields_not_user_tags(self):
        row, artist, album = valid_data()
        artist["tags"] = [{"name": "made up astrophysicist"}]
        artist["annotations"] = "LLM-generated biography"
        fields = notes.verify(row, fetch_from(artist, album))
        self.assertEqual(fields[0], "miles-davis")
        self.assertEqual(fields[2], "Kind of Blue")
        self.assertIn("jazz trumpeter, bandleader, songwriter", fields[1])
        self.assertNotIn("astrophysicist", "\t".join(fields))
        self.assertNotIn("biography", "\t".join(fields))
        self.assertEqual(fields[5], notes.ATTRIBUTION)
        self.assertEqual(fields[6], notes.LICENSE)

    def test_falsified_identity_album_and_artist_credit_are_rejected(self):
        row, artist, album = valid_data()
        alterations = (
            ({**artist, "id": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"}, album),
            ({**artist, "name": "Not Miles Davis"}, album),
            (artist, {**album, "id": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"}),
            (artist, {**album, "title": "Invented Album"}),
            (artist, {**album, "primary-type": "Single"}),
            (artist, {**album, "artist-credit": [{"artist": {"id": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"}}]}),
            (artist, {**album, "artist-credit": []}),
        )
        for bad_artist, bad_album in alterations:
            with self.subTest(artist=bad_artist, album=bad_album), self.assertRaises(notes.InvalidRecord):
                notes.verify(row, fetch_from(bad_artist, bad_album))

    def test_invalid_input_never_emits_and_missing_source_facts_are_explicit(self):
        row, artist, album = valid_data()
        artist.pop("disambiguation")
        artist["life-span"] = {}
        fields = notes.verify(row, fetch_from(artist, album))
        self.assertIn("instrument not recorded", fields[1])
        self.assertIn("birth unverified", fields[1])
        for changed_row in (
            {**row, "artist_mbid": "../../fake"},
            {**row, "artist_name": "Miles Davis\n"},
            {**row, "album_title": "Kind\tBlue"},
        ):
            with self.subTest(row=changed_row):
                result, errors = notes.generate([changed_row], fetch_from(artist, album))
                self.assertFalse(result)
                self.assertEqual(len(errors), 1)

    def test_duplicates_do_not_add_unverified_records(self):
        row, artist, album = valid_data()
        good, rejected = notes.generate([row, row], fetch_from(artist, album))
        self.assertEqual(len(good), 1)
        self.assertEqual(len(rejected), 1)
        self.assertIn("duplicate", rejected[0])


class CommittedDataTests(unittest.TestCase):
    """Current checked-in SD candidate is independently validated offline."""

    def test_curated_mbids_and_album_ids_match_export(self):
        rows = notes.load_input(notes.DEFAULT_INPUT)
        # v0.7b requires at least 20 individually official-MusicBrainz-
        # verified cards, but preserves all three original curated artists.
        # Check exact CSV/TSV/IDX mapping, not a fabricated fixed count.
        self.assertGreaterEqual(len(rows), 20)
        self.assertLessEqual(len(rows), notes.MAX_INPUT_ROWS)
        prior = {
            ("Miles Davis", "561d854a-6a28-4aa7-8c99-323e6ce46c2a", "Kind of Blue", "8e8a594f-2175-38c7-a871-abb68ec363e7"),
            ("John Coltrane", "b625448e-bf4a-41c3-a421-72ad46cdb831", "Blue Train", "c63baaaa-10c0-4b86-ad7e-1f39d7e2eccc"),
            ("Bill Evans", "8247a3f2-3a8e-4256-b322-6c57b03a4e36", "Waltz for Debby", "3706fa92-d486-414f-892a-7b21d0f6037f"),
        }
        actual = {(r["artist_name"], r["artist_mbid"], r["album_title"], r["album_mbid"]) for r in rows}
        self.assertTrue(prior.issubset(actual), "Three original verified cards must be preserved")
        self.assertEqual(len({r["artist_mbid"] for r in rows}), len(rows))
        self.assertEqual(len({notes.artist_key(r["artist_name"]) for r in rows}), len(rows))
        provenance = (ROOT / "data" / "ARTIST-NOTES-PROVENANCE.md").read_text(encoding="utf-8")
        self.assertIn("36 artist/release-group pairs", provenance)
        raw = notes.DEFAULT_OUTPUT.read_bytes()
        self.assertTrue(raw.endswith(b"\n"))
        self.assertNotIn(b"\r", raw)
        self.assertFalse(raw.startswith(b"\xef\xbb\xbf"))
        lines = raw.decode("utf-8", errors="strict").splitlines()
        self.assertEqual(len(lines), len(rows))
        mbids_by_key = {notes.artist_key(r["artist_name"]): r for r in rows}
        found = set()
        for line in lines:
            fields = tuple(line.split("\t"))
            self.assertEqual(len(fields), 7, "Each SD row must have seven fields")
            self.assertEqual(notes.render([fields]).decode("utf-8").rstrip("\n"), line)
            key, summary, album, artist_url, album_url, attribution, license_name = fields
            self.assertIn(key, mbids_by_key)
            self.assertNotIn(key, found)
            found.add(key)
            source = mbids_by_key[key]
            self.assertEqual(album, source["album_title"])
            self.assertEqual(artist_url, f'https://musicbrainz.org/artist/{source["artist_mbid"]}')
            self.assertEqual(album_url, f'https://musicbrainz.org/release-group/{source["album_mbid"]}')
            self.assertEqual(attribution, notes.ATTRIBUTION)
            self.assertEqual(license_name, notes.LICENSE)
            self.assertNotIn("\ufeff", summary)
        self.assertEqual(found, set(mbids_by_key))
        self.assertTrue(raw.isascii(), "SD notes are ASCII-subset UTF-8")
        index = (ROOT / "data" / "artist-notes.idx").read_bytes()
        notes.validate_index(raw, index)
        self.assertTrue(index.isascii())
        # Validate actual seek positions on disk in bytes, not Python characters.
        with notes.DEFAULT_OUTPUT.open("rb") as stream:
            for line in index.decode("ascii").splitlines()[1:]:
                hashed, offset, key = line.split("\t")
                self.assertEqual(int(hashed, 16), notes.fnv1a32(key))
                stream.seek(int(offset))
                record = stream.readline().rstrip(b"\n")
                self.assertEqual(record.split(b"\t", 1)[0], key.encode("ascii"))


if __name__ == "__main__":
    unittest.main()
