"""MusicBrainz evidence policy: exact IDs and explicit performance links only."""
import importlib.util
import copy
import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_jazz_music_evidence import (parse_mb_artist, p434_statements,
                                       is_musical_recording_title, reciprocal_wikidata_qids,
                                       parse_event_venues, parse_recording_coperformers, validate, UUID,
                                       coperformer_priority_keys)
from enrich_jazz_bios import facts_to_bio, PROPS

ARTIST = "561d854a-6a28-4aa7-8c99-323e6ce46c2a"
OTHER = "298fb0c1-5a1f-43c1-854d-663762caad6e"
GROUP = "f137837b-fa55-48d1-8c85-f13961b3ac77"
RECORDING = "8e8a594f-2175-38c7-a871-abb68ec363e7"
EVENT = "e9ee55f4-17a7-47f9-b445-ad332a82ea78"


class ProvenMusicTests(unittest.TestCase):
    def test_prefer_display_prioritizes_only_visible_machine_enriched_keys(self):
        records = {"hank-mobley": {}, "miles-davis": {},
                   "stan-kenton": {}, "bob-french": {}}
        ordered = coperformer_priority_keys(records, prefer_display=True)
        # These source-overridden summaries cannot gain visible MB text.
        # Al Lucas and Bruce Ditmas now have reviewed source-bound profiles;
        # use a still-unresearched musician for machine-display priority.
        self.assertEqual(ordered[0], "bob-french")
        self.assertEqual(set(ordered[1:]),
                         {"hank-mobley", "miles-davis", "stan-kenton"})
        unpreferred = coperformer_priority_keys(records, prefer_display=False)
        self.assertEqual(set(unpreferred), set(ordered))
        self.assertEqual(len(ordered), len(set(ordered)))

    def test_wikidata_p434_unique_and_ambiguous_identifiers(self):
        def claim(mbid, rank="normal"):
            return {"id": "Q1$proof", "rank": rank,
                    "mainsnak": {"datavalue": {"value": mbid}}}
        entity = {"claims": {"P434": [claim(ARTIST), claim(ARTIST)]}}
        self.assertEqual({mb for mb, _ in p434_statements(entity)}, {ARTIST})
        entity["claims"]["P434"].append(claim(OTHER))
        self.assertEqual(len({mb for mb, _ in p434_statements(entity)}), 2)
        entity["claims"]["P434"].append(claim("somebody's name"))
        self.assertEqual(len(p434_statements(entity)), 2)  # duplicate and invalid excluded
        self.assertIsNone(UUID.fullmatch("Miles Davis"))

    def test_artist_credit_does_not_prove_performance(self):
        data = {"id": ARTIST, "relations": [
            {"type": "artist credit", "target-type": "release-group",
             "release-group": {"id": RECORDING, "title": "Example Album"}},
            {"type": "main performer", "target-type": "event", "direction": "forward",
             "event": {"id": EVENT, "name": "Berlin 1973"}},
            {"type": "instrument", "target-type": "recording", "direction": "forward",
             "recording": {"id": RECORDING, "title": "Sample Recording"}},
            {"type": "member of band", "target-type": "artist", "direction": "forward",
             "artist": {"id": GROUP, "name": "The Jazz Quartet"}},
            {"type": "collaboration", "target-type": "artist", "direction": "forward",
             "artist": {"id": OTHER, "name": "Davis and Coltrane Project"}},
            {"type": "married", "target-type": "artist", "direction": "forward",
             "artist": {"id": OTHER, "name": "Relative"}},
        ]}
        output = parse_mb_artist(data, ARTIST)
        self.assertEqual(len(output["recordings"]), 1)
        self.assertEqual(output["recordings"][0]["source_url"],
                         f"https://musicbrainz.org/recording/{RECORDING}")
        self.assertEqual(len(output["performance_events"]), 1)
        self.assertEqual(output["performance_venues"], [])
        self.assertEqual([x["name"] for x in output["ensembles"]], ["The Jazz Quartet"])
        self.assertEqual(output["collaborators"], [])
        self.assertEqual([x["name"] for x in output["collaboration_projects"]],
                         ["Davis and Coltrane Project"])
        self.assertEqual(output["release_groups"], [])

    def test_proven_music_summary_uses_recording_not_unverified_venue(self):
        record = {"facts": {p: [] for p in PROPS}}
        music = {"status": "checked_mb",
                 "recordings": [{"relation_type": "instrument", "title": "Sample Recording"}],
                 "ensembles": [{"name": "The Jazz Quartet"}],
                 "performance_venues": []}
        actual = facts_to_bio("Jazz artist", record, music=music)
        self.assertIn("Recorded on: Sample Recording", actual)
        self.assertIn("Member of The Jazz Quartet", actual)
        self.assertNotIn("Played in Berlin", actual)
        self.assertEqual(facts_to_bio("Jazz artist", record, music={"status": "pending_mb"}),
                         "Jazz artist")

    def test_180_character_budget_prioritizes_real_music_over_generic_fields(self):
        facts = {p: [] for p in PROPS}
        facts["P569"] = [{"year": 1934}]
        facts["P570"] = [{"year": 2007}]
        facts["P1303"] = [{"label": "tenor saxophone"}, {"label": "clarinet"}]
        facts["P19"] = [{"label": "New York City"}]
        facts["P27"] = [{"label": "United States of America"}]
        music = {"status": "checked_mb",
                 "recordings": [{"relation_type": "instrument", "title": "A Particular Blue Train Recording"}],
                 "ensembles": [{"name": "Confirmed Jazz Quartet"}],
                 "collaborators": [{"recording_mbid": RECORDING, "name": "Confirmed Saxophonist"}],
                 "performance_venues": []}
        original = "American jazz instrumentalist and music arranger with an extensive performing career"
        bio = facts_to_bio(original, {"facts": facts}, limit=180, music=music)
        previous_order = facts_to_bio(original, {"facts": facts}, limit=180,
                                      music=music, prioritize_music=False)
        self.assertLessEqual(len(bio), 180)
        self.assertIn("Recorded on: A Particular Blue Train Recording", bio)
        self.assertNotIn("Recorded on: A Particular Blue Train Recording", previous_order)
        self.assertIn("Member of Confirmed Jazz Quartet", bio)
        self.assertNotIn("Citizenship:", bio)
        self.assertNotIn("Born in ", bio)

    def test_stage_directions_do_not_become_biography_recordings(self):
        self.assertFalse(is_musical_recording_title("(Blairman enters)"))
        self.assertFalse(is_musical_recording_title("Audience applause"))
        self.assertFalse(is_musical_recording_title("Introduction of the band"))
        self.assertFalse(is_musical_recording_title('"V"'))
        self.assertFalse(is_musical_recording_title("A propos de logique (declamation)"))
        self.assertFalse(is_musical_recording_title("A poet (recitation)"))
        self.assertTrue(is_musical_recording_title("A Poem for Jazz"))
        self.assertTrue(is_musical_recording_title("Blue Train"))
        source = {"id": ARTIST, "relations": [
            {"type": "instrument", "target-type": "recording", "direction": "forward",
             "recording": {"id": OTHER, "title": "(Blairman enters)"}},
            {"type": "instrument", "target-type": "recording", "direction": "forward",
             "recording": {"id": RECORDING, "title": "Blue Train"}},
        ]}
        result = parse_mb_artist(source, ARTIST)
        self.assertEqual(result["raw_recording_relationship_count"], 2)
        self.assertEqual(result["excluded_nonmusical_title_count"], 1)
        self.assertEqual([r["title"] for r in result["recordings"]], ["Blue Train"])

    def test_reciprocal_wikidata_links_are_exact_qids(self):
        fake = {"relations": [
            {"type": "wikidata", "target-type": "url",
             "url": {"resource": "https://www.wikidata.org/wiki/Q12345"}},
            {"type": "wikipedia", "target-type": "url",
             "url": {"resource": "https://en.wikipedia.org/wiki/Q999"}},
            {"type": "wikidata", "target-type": "url",
             "url": {"resource": "https://www.wikidata.org/wiki/Q34567"}},
        ]}
        self.assertEqual(reciprocal_wikidata_qids(fake), ["Q12345", "Q34567"])

    def test_named_coperformer_requires_both_explicit_performance_credits(self):
        rel = lambda mid, name, typ: {"target-type": "artist", "type": typ,
                                      "artist": {"id": mid, "name": name}}
        source = {"id": RECORDING, "relations": [
            rel(ARTIST, "Miles Davis", "instrument"),
            rel(OTHER, "John Coltrane", "instrument"),
            rel(GROUP, "The Jazz Quartet", "artist credit"),
        ]}
        linked = parse_recording_coperformers(source, ARTIST, RECORDING, "Sample Recording")
        self.assertEqual([r["name"] for r in linked], ["John Coltrane"])
        self.assertEqual(linked[0]["recording_mbid"], RECORDING)
        source["relations"] = source["relations"][1:]
        self.assertEqual(parse_recording_coperformers(source, ARTIST, RECORDING, "Sample Recording"), [])

    def test_event_title_never_substitutes_for_explicit_venue_relation(self):
        event = {"mbid": EVENT, "name": "Live at the Blue Note", "relation_type": "main performer"}
        self.assertEqual(parse_event_venues({"id": EVENT, "relations": []}, event), [])
        rel = {"type": "held at", "direction": "forward", "place":
               {"id": GROUP, "name": "Berliner Philharmonie"}}
        found = parse_event_venues({"id": EVENT, "relations": [rel]}, event)
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]["source_url"], f"https://musicbrainz.org/event/{EVENT}")
        self.assertEqual(found[0]["place_mbid"], GROUP)

    def test_snapshot_has_only_pinned_source_identities(self):
        snapshot = ROOT / "data/jazz-music-evidence.json"
        if not snapshot.exists():
            self.skipTest("snapshot not yet fetched")
        entities = json.loads(snapshot.read_text("utf-8"))
        validate(entities)
        for key, row in entities.items():
            self.assertRegex(row["qid"], r"^Q[1-9][0-9]*$")
            self.assertEqual(row["wikidata_url"], f'https://www.wikidata.org/wiki/{row["qid"]}')
            if row["mbid"]:
                self.assertRegex(row["mbid"], UUID)
                self.assertEqual(row["musicbrainz_url"], f'https://musicbrainz.org/artist/{row["mbid"]}')
                self.assertTrue(row["wikidata_statement"].upper().startswith(row["qid"] + "$"))
            else:
                self.assertNotEqual(row["status"], "checked_mb")
            if row["status"] == "checked_mb":
                if row.get("identity_link_strength") == "reciprocal":
                    self.assertEqual(row.get("mb_reciprocal_qids"), [row["qid"]])
                elif row.get("identity_link_strength") == "wikidata_p434_only":
                    self.assertEqual(row.get("mb_reciprocal_qids"), [])
                for recording in row["recordings"]:
                    self.assertIn(recording["relation_type"], ("instrument", "vocal", "performer"))
                    self.assertEqual(recording["source_url"], f'https://musicbrainz.org/recording/{recording["mbid"]}')
                for ensemble in row["ensembles"]:
                    self.assertEqual(ensemble["relation_type"], "member of band")
                for coworker in row.get("collaborators", []):
                    self.assertIn(coworker["relation_type"], ("instrument", "vocal", "performer"))
                    self.assertIn(coworker["recording_mbid"],
                                  {r["mbid"] for r in row["recordings"] + row.get("raw_recording_examples", [])})
                    self.assertNotEqual(coworker["mbid"], row["mbid"])
                    self.assertEqual(coworker["source_url"],
                                     f'https://musicbrainz.org/recording/{coworker["recording_mbid"]}')
                for venue in row.get("performance_venues", []):
                    self.assertIn(venue["event_mbid"],
                                  {r["mbid"] for r in row.get("performance_events", [])})
                    self.assertIn(venue["event_relation_type"], ("main performer", "support act"))
                    self.assertEqual(venue["source_url"],
                                     f'https://musicbrainz.org/event/{venue["event_mbid"]}')
                    self.assertEqual(venue["place_url"],
                                     f'https://musicbrainz.org/place/{venue["place_mbid"]}')
                for album in row.get("release_groups", []):
                    self.assertFalse(album["performance_proof"])
                    self.assertEqual(album["selection"], "human_curated_musicbrainz_release_group")
            elif row["status"] == "identity_conflict":
                self.assertTrue(row.get("mb_reciprocal_qids"))
                self.assertFalse(row.get("recordings"))
                self.assertFalse(row.get("ensembles"))

    def test_validator_rejects_forged_ensemble_and_venue_sources(self):
        snapshot = json.loads((ROOT / "data/jazz-music-evidence.json").read_text("utf-8"))
        for field, url in (("ensembles", "https://musicbrainz.org/artist/invalid"),
                           ("performance_venues", "https://musicbrainz.org/event/invalid"),
                           ("collaborators", "https://musicbrainz.org/recording/invalid")):
            found = next(((key, row) for key, row in snapshot.items()
                          if row.get("status") == "checked_mb" and row.get(field)), None)
            self.assertIsNotNone(found, f"Expected established proof type {field}")
            key, row = found
            good = copy.deepcopy(row)
            validate({key: good}, check_snapshot_bytes=False)
            bad = copy.deepcopy(good)
            bad[field][0]["source_url"] = url
            with self.assertRaises(ValueError):
                validate({key: bad}, check_snapshot_bytes=False)


if __name__ == "__main__":
    unittest.main()
