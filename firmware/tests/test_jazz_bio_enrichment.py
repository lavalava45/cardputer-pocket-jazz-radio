"""Offline, source-backed artist enrichment and exact byte-offset index."""
import hashlib
import json
import subprocess
import zlib
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from enrich_jazz_bios import facts_to_bio, PROPS


class JazzBioEnrichmentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = [r.split("\t") for r in (ROOT / "data/jazz-encyclopedia.tsv").read_text("ascii").splitlines()]
        cls.snapshot = json.loads((ROOT / "data/jazz-structured-facts.json").read_text("utf-8"))

    def test_every_snapshot_entry_has_matching_source_and_evidence(self):
        # Researched biographies deliberately replace the generated card's
        # display citation. The immutable seed retains the QID source proof.
        records = {r[0]: r for r in [row.split("\t") for row in
                   (ROOT / "data/jazz-encyclopedia-seed.tsv").read_text("ascii").splitlines()]}
        for key, record in self.snapshot.items():
            self.assertEqual(records[key][4], "https://www.wikidata.org/wiki/" + record["qid"])
            self.assertEqual(set(record["facts"]), set(PROPS))
            for prop, values in record["facts"].items():
                for value in values:
                    self.assertTrue(value["statement"].upper().startswith(record["qid"] + "$"))
                    self.assertIn("year" if prop in ("P569", "P570") else "qid", value)

    def test_protected_editorial_bios_remain_byte_identical(self):
        catalog = {r[0]: r for r in self.rows}
        for line in (ROOT / "data/jazz-editorial-bios.tsv").read_text("ascii").splitlines():
            key, summary, url = line.split("\t")
            self.assertEqual((catalog[key][2], catalog[key][4]), (summary, url))

    def test_immutable_seed_rebuilds_without_duplicate_machine_suffixes(self):
        seed = (ROOT / "data/jazz-encyclopedia-seed.tsv").read_bytes()
        self.assertEqual(hashlib.sha256(seed).hexdigest(),
                         "0a288ccbdff85b32c100137ff53f22b0aacfbe52fd15bb0165bdf4f5a6df2dc6")
        original = {row[0]: row for row in [r.split("\t") for r in seed.decode("ascii").splitlines()]}
        generated = {row[0]: row for row in self.rows}
        self.assertEqual(set(original), set(generated))
        for key, record in self.snapshot.items():
            if key in original and key in generated and generated[key][4].startswith("https://www.wikidata.org"):
                bio = generated[key][2]
                self.assertLessEqual(bio.count("Instruments:"), 1)
                self.assertFalse("Instrument:" in bio and "Instruments:" in bio)

    def test_claims_only_no_unverified_fields_and_bounded_ascii(self):
        for record in self.snapshot.values():
            bio = facts_to_bio("Jazz musician", record)
            self.assertTrue(bio.isascii())
            self.assertLessEqual(len(bio), 180)
            self.assertNotIn("Collaborated with", bio)
            self.assertNotIn("Album:", bio)
        empty = {"facts": {prop: [] for prop in PROPS}}
        self.assertEqual(facts_to_bio("Jazz musician", empty), "Jazz musician")
        dates = {"facts": {**empty["facts"],
                           "P569": [{"year": 1920}], "P570": [{"year": 1990}]}}
        self.assertIn("died 1990", facts_to_bio("American musician (born 1920)", dates))
        first = facts_to_bio("American musician (born 1920)", dates)
        self.assertEqual(facts_to_bio(first, dates), first)
        populated = {"facts": {**empty["facts"],
            "P800": [{"qid": "Q1", "label": "Verified notable composition"}],
            "P19": [{"qid": "Q2", "label": "New York City"}],
            "P27": [{"qid": "Q3", "label": "United States"}]}}
        bio = facts_to_bio("A" * 100, populated, 145)
        self.assertIn("Notable work: Verified notable composition", bio)
        self.assertNotIn("Citizenship:", bio)

    def test_generic_p800_films_and_books_do_not_become_jazz_recommendations(self):
        by_key = {row[0]: row[2] for row in self.rows}
        excluded = {
            "barbra-streisand": "Yentl",
            "boris-vian": "Froth on the Daydream",
            "humphrey-carpenter": "J. R. R. Tolkien",
            "lili-haydn": "DriverX",
            "michael-snow": "Wavelength",
            "michelle-dockery": "Downton Abbey",
            "viggo-mortensen": "Far from Men",
        }
        for key, work in excluded.items():
            with self.subTest(key=key):
                self.assertIn(work, str(self.snapshot[key]["facts"]["P800"]))
                self.assertNotIn("Notable work: " + work, by_key[key])
                rerendered = facts_to_bio(
                    "Jazz musician", self.snapshot[key], artist_key=key)
                self.assertNotIn("Notable work:", rerendered)
        # A true musical work is not removed just because it came from P800.
        retained = facts_to_bio(
            "Jazz musician", self.snapshot["henry-mancini"],
            artist_key="henry-mancini")
        self.assertIn("Notable work: Moon River", retained)

    def test_index_points_to_exact_ascii_lines(self):
        raw = (ROOT / "data/jazz-encyclopedia.tsv").read_bytes()
        idx = (ROOT / "data/jazz-encyclopedia.idx").read_bytes()
        self.assertTrue(raw.endswith(b"\n"))
        self.assertTrue(idx.endswith(b"\n"))
        lines = idx.decode("ascii").splitlines()
        crc = f"{zlib.crc32(raw) & 0xffffffff:08x}"
        index_rows = idx.split(b"\n", 1)[1]
        index_crc = f"{zlib.crc32(index_rows) & 0xffffffff:08x}"
        self.assertEqual(lines[0], f"#PJ-JAZZ-IDX\t2\t{len(raw)}\t{len(self.rows)}\t{crc}\t{index_crc}")
        self.assertNotEqual(f"{zlib.crc32(raw[:-1] + b'X') & 0xffffffff:08x}", crc,
                            "Same-size TSV corruption must invalidate the index header")
        self.assertNotEqual(f"{zlib.crc32(index_rows[:-1] + b'X') & 0xffffffff:08x}", index_crc,
                            "Same-size index-row corruption must invalidate the index header")
        entries = [line.split("\t") for line in lines[1:]]
        self.assertEqual(len(entries), len(self.rows))
        self.assertEqual([entry[0] for entry in entries], sorted(entry[0] for entry in entries))
        previous = -1
        for (key, offset), row in zip(entries, self.rows):
            offset = int(offset)
            self.assertGreater(offset, previous)
            self.assertEqual(raw[offset:raw.index(b"\n", offset)].decode("ascii"), "\t".join(row))
            self.assertEqual(key, row[0])
            previous = offset
        provenance = (ROOT / "data/JAZZ-ENCYCLOPEDIA-PROVENANCE.md").read_text("utf-8")
        self.assertIn(hashlib.sha256(raw).hexdigest(), provenance)
        self.assertIn(hashlib.sha256(idx).hexdigest(), provenance)

    def test_rebuild_check_is_offline_and_does_not_mutate_artifacts(self):
        outputs = [ROOT / "data" / name for name in (
            "jazz-encyclopedia.tsv", "jazz-encyclopedia.idx", "jazz-facts.tsv",
            "JAZZ-ENCYCLOPEDIA-PROVENANCE.md")]
        before = [hashlib.sha256(path.read_bytes()).digest() for path in outputs]
        for _ in range(2):
            result = subprocess.run([sys.executable, str(ROOT / "tools/build_jazz_encyclopedia.py"), "--check"],
                                    capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertEqual(before, [hashlib.sha256(path.read_bytes()).digest() for path in outputs])

    def test_report_measures_one_pass_from_seed_not_current_output(self):
        result = subprocess.run([sys.executable, str(ROOT / "tools/enrich_jazz_bios.py")],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        stats = json.loads(result.stdout)
        self.assertGreaterEqual(stats["changed_once_from_immutable_seed"], 1400)
        self.assertLessEqual(stats["changed_once_from_immutable_seed"], len(self.snapshot))
        self.assertEqual(stats["wikidata_eligible"], len(self.snapshot))
        self.assertEqual(stats["editorial_preserved"], 63)
        self.assertEqual(stats["claim_presence"]["P1303"], 1368)
        self.assertEqual(stats["quality"]["duplicate_instrument_segments"], 0)
        self.assertEqual(stats["sourced_repertoire"]["musicbrainz_album_recommendations"], 36)
        music_path = ROOT / "data/jazz-music-evidence.json"
        music = json.loads(music_path.read_text("utf-8")) if music_path.exists() else {}
        for key, field in (("explicit_musicbrainz_performance_recordings", "recordings"),
                           ("explicit_musicbrainz_member_of_band", "ensembles"),
                           ("named_coperformer_pairs_verified", "collaborators"),
                           ("verified_performance_venues", "performance_venues")):
            self.assertEqual(stats["sourced_repertoire"][key],
                             sum(len(v.get(field, [])) for v in music.values()))


if __name__ == "__main__":
    unittest.main()
