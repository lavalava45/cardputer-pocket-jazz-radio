"""Offline data contracts for the verified MusicBrainz corpus and C++ parser.

No network, Arduino build, SD writes or release changes.
"""
import hashlib
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
SPEC = importlib.util.spec_from_file_location(
    "pocket_jazz_notes_offline", ROOT / "tools" / "build_jazz_notes.py")
notes = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(notes)


class JazzSearchDataContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.csv_rows = notes.load_input(DATA / "artists-input.csv")
        cls.tsv = (DATA / "artist-notes.tsv").read_bytes()
        cls.idx = (DATA / "artist-notes.idx").read_bytes()
        cls.records = [line.split("\t") for line in cls.tsv.decode("ascii").splitlines()]

    def test_verified_36_within_50_and_originals_preserved(self):
        self.assertEqual(len(self.csv_rows), 36)
        self.assertLessEqual(len(self.csv_rows), notes.MAX_INPUT_ROWS)
        self.assertGreaterEqual(len(self.csv_rows), 20)
        artists = {row["artist_name"]: row for row in self.csv_rows}
        for artist, album in (
            ("Miles Davis", "Kind of Blue"),
            ("John Coltrane", "Blue Train"),
            ("Bill Evans", "Waltz for Debby"),
        ):
            self.assertEqual(artists[artist]["album_title"], album)
        self.assertEqual(len({r["artist_mbid"] for r in self.csv_rows}), 36)
        self.assertEqual(
            len({notes.artist_key(r["artist_name"]) for r in self.csv_rows}), 36)

    def test_exact_export_matches_csv_and_provenance(self):
        self.assertEqual(len(self.records), len(self.csv_rows))
        by_key = {
            notes.artist_key(row["artist_name"]): row for row in self.csv_rows}
        self.assertEqual(set(row[0] for row in self.records), set(by_key))
        for record in self.records:
            with self.subTest(artist=record[0]):
                key, summary, album, artist_url, album_url, credit, license_text = record
                row = by_key[key]
                self.assertEqual(album, row["album_title"])
                self.assertEqual(
                    artist_url, "https://musicbrainz.org/artist/" + row["artist_mbid"])
                self.assertEqual(
                    album_url, "https://musicbrainz.org/release-group/" + row["album_mbid"])
                self.assertEqual(credit, notes.ATTRIBUTION)
                self.assertEqual(license_text, notes.LICENSE)
                self.assertTrue(summary)
                self.assertEqual(
                    notes.render([record]),
                    ("\t".join(record) + "\n").encode("ascii"))
        provenance = (DATA / "ARTIST-NOTES-PROVENANCE.md").read_text(
            encoding="utf-8")
        self.assertIn("Verification timestamp (UTC):", provenance)
        for row in self.csv_rows:
            self.assertIn(
                "https://musicbrainz.org/artist/" + row["artist_mbid"], provenance)
            self.assertIn(
                "https://musicbrainz.org/release-group/" + row["album_mbid"], provenance)

    def test_sha_index_order_offsets_and_canonical_limits(self):
        notes.validate_index(self.tsv, self.idx)
        self.assertLessEqual(len(self.tsv), 32768)
        self.assertLessEqual(len(self.idx), 4096)
        self.assertTrue(self.tsv.endswith(b"\n"))
        digest = hashlib.sha256(self.tsv).hexdigest()
        header = self.idx.decode("ascii").splitlines()[0].split("\t")
        self.assertEqual(
            header, ["#PJ-NOTES-IDX", "1", "fnv1a32",
                     digest, str(len(self.csv_rows))])
        indexes = [
            line.split("\t")
            for line in self.idx.decode("ascii").splitlines()[1:]]
        self.assertEqual(len(indexes), len(self.records))
        self.assertEqual(
            [(int(h, 16), key) for h, _, key in indexes],
            sorted((int(h, 16), key) for h, _, key in indexes))
        for h, offset, key in indexes:
            with self.subTest(key=key):
                i = int(offset)
                self.assertLess(i, len(self.tsv))
                self.assertTrue(i == 0 or self.tsv[i - 1:i] == b"\n")
                self.assertEqual(
                    self.tsv[i:].split(b"\n", 1)[0].split(b"\t", 1)[0],
                    key.encode("ascii"))
                self.assertEqual(int(h, 16), notes.fnv1a32(key))


if __name__ == "__main__":
    unittest.main()
