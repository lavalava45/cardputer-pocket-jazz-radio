"""Accuracy-oriented, read-only coverage contract for the SD encyclopedia."""
import sys
import json
from pathlib import Path
import tempfile
import shutil
from unittest import TestCase

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import audit_jazz_bio_quality as quality


class JazzBiographyQuality(TestCase):
    def test_real_catalog_never_calls_dates_or_citizenship_musical_context(self):
        report = quality.audit()
        self.assertEqual(report["all_artist_cards"], 1498)
        self.assertEqual(report["preserved_editorial_biographies"], 63)
        self.assertGreaterEqual(report["descriptions_changed_from_immutable_seed"], 1428)
        self.assertEqual(report["source_researched_musical_biographies"], 532)
        self.assertEqual(report["source_reviewed_musical_biographies_total"], 595)
        self.assertEqual(report["claims"]["P569"], 1421)
        self.assertEqual(report["claims"]["P1303"], 1368)
        self.assertEqual(report["over_180_chars"], 0)
        self.assertEqual(report["under_60_chars"], 0)
        self.assertEqual(report["p800_nonmusical_display_exclusions"], 7)
        self.assertEqual(report["index"]["rows"], 1498)
        self.assertIn("musical_facts_actually_displayed", report)
        self.assertIn("source_backed_life_years_actually_displayed", report)
        # These are measured from *pinned* explicit relationships, never
        # assumed zero or derived from citizenship / location / birth year.
        music = report["music_relationships_from_pinned_musicbrainz"]
        self.assertEqual(report["new_evidenced_recordings"],
                         music["with_recorded_performance"])
        self.assertEqual(report["new_evidenced_collaborations"],
                         music["with_verified_collaboration"])
        self.assertEqual(report["new_evidenced_performance_venues"],
                         music["with_explicit_performance_venue"])
        self.assertLess(report["music_significance_verified_by_editorial_review"],
                        report["all_artist_cards"])

    def test_bad_index_offset_detected_without_mutating_real_data(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "data").mkdir()
            for name in ("jazz-encyclopedia.tsv", "jazz-encyclopedia.idx",
                         "jazz-encyclopedia-seed.tsv", "jazz-editorial-bios.tsv",
                         "jazz-structured-facts.json"):
                shutil.copyfile(ROOT / "data" / name, root / "data" / name)
            path = root / "data/jazz-encyclopedia.idx"
            text = path.read_text("ascii")
            parts = text.splitlines(keepends=True)
            entry = parts[1].split("\t")
            parts[1] = entry[0] + "\t9999\n"
            path.write_text("".join(parts), encoding="ascii", newline="\n")
            with self.assertRaisesRegex(ValueError, "Index offsets/keys|checksum"):
                quality.audit(root)

    def test_unsupported_editorial_change_fails_closed_without_rewriting(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "data").mkdir()
            for name in ("jazz-encyclopedia.tsv", "jazz-encyclopedia.idx",
                         "jazz-encyclopedia-seed.tsv", "jazz-editorial-bios.tsv",
                         "jazz-structured-facts.json"):
                shutil.copyfile(ROOT / "data" / name, root / "data" / name)
            path = root / "data/jazz-editorial-bios.tsv"
            text = path.read_text("ascii")
            path.write_text(text.replace("Trumpeter and bandleader", "Fictitious jazz person", 1),
                            encoding="ascii")
            with self.assertRaisesRegex(ValueError, "protected editorial text changed"):
                quality.audit(root)

    def test_researched_citations_and_summaries_cannot_drift_from_final_tsv(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "data").mkdir()
            for name in ("jazz-encyclopedia.tsv", "jazz-encyclopedia.idx",
                         "jazz-encyclopedia-seed.tsv", "jazz-editorial-bios.tsv",
                         "jazz-structured-facts.json", "jazz-researched-context.tsv",
                         "jazz-music-evidence.json"):
                shutil.copyfile(ROOT / "data" / name, root / "data" / name)
            path = root / "data/jazz-researched-context.tsv"
            source = path.read_text("ascii")
            self.assertIn("Mannenberg", source)
            # Preserve the exact byte length: otherwise this test accidentally
            # exercises the 180-char guard instead of source/TSV drift.
            path.write_text(source.replace("Mannenberg", "Inventedzz", 1),
                            encoding="ascii")
            with self.assertRaisesRegex(ValueError,
                                        "researched biography not integrated verbatim"):
                quality.audit(root)

    def test_musicbrainz_recording_requires_actual_artist_performance_role(self):
        """An artist-credit or group membership is not a player credit."""
        root = ROOT
        evidence = json.loads((root/"data/jazz-music-evidence.json").read_text("utf-8"))
        key = next(k for k, v in evidence.items() if v.get("recordings"))
        row = evidence[key]
        self.assertIn(row["recordings"][0]["relation_type"],
                      ("instrument", "vocal", "performer"))
        with tempfile.TemporaryDirectory() as tmp:
            target=Path(tmp)/"evidence.json"
            row["recordings"][0]["relation_type"]="writer"
            target.write_text(json.dumps(evidence),encoding="utf-8")
            seed={r["key"]:r for r in quality.read_catalog(
                root/"data/jazz-encyclopedia-seed.tsv")}
            with self.assertRaisesRegex(ValueError, "recording performance proof"):
                quality.verify_music_evidence(target, seed)

    def test_named_collaborator_must_share_a_specified_verified_recording(self):
        evidence=json.loads((ROOT/"data/jazz-music-evidence.json").read_text("utf-8"))
        key=next(k for k,v in evidence.items() if v.get("recordings"))
        artist=evidence[key]
        artist["collaborators"]=[{
            "mbid": "11111111-1111-1111-1111-111111111111",
            "recording_mbid": "22222222-2222-2222-2222-222222222222",
            "name": "Unrelated artist",
        }]
        with tempfile.TemporaryDirectory() as tmp:
            target=Path(tmp)/"evidence.json"
            target.write_text(json.dumps(evidence),encoding="utf-8")
            seed={r["key"]:r for r in quality.read_catalog(
                ROOT/"data/jazz-encyclopedia-seed.tsv")}
            with self.assertRaisesRegex(ValueError, "collaborator lacks shared recording proof"):
                quality.verify_music_evidence(target, seed)

    def test_same_sized_source_swap_fails_index_crc_before_offset_scan(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            (root/"data").mkdir()
            a=root/"data/jazz-encyclopedia.tsv"
            b=root/"data/jazz-encyclopedia.idx"
            shutil.copyfile(ROOT/"data/jazz-encyclopedia.tsv",a)
            shutil.copyfile(ROOT/"data/jazz-encyclopedia.idx",b)
            raw=bytearray(a.read_bytes())
            needle=b"Jazz"
            at=raw.find(needle)
            self.assertGreater(at,0)
            raw[at]=ord("j")
            a.write_bytes(raw)
            with self.assertRaisesRegex(ValueError, "checksum"):
                quality.verify_index(a,b)
