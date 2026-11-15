"""Real dossier provenance, earlier-source protection, Spark/Journey reachability."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
sys.path.insert(0, str(ROOT / "tools"))
from verify_jazz_editorial_dossiers import audit, load_verified_editorial_spark
from audit_jazz_bio_quality import verify_index
from build_now_playing_hints import RESEARCH_CAPTIONS, render_header


class EditorialDossierContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dossiers = json.loads((DATA / "jazz-editorial-dossiers.json").read_text("ascii"))
        cls.researched = {
            r.split("\t")[0]: r.split("\t")[1:] for r in
            (DATA / "jazz-researched-context.tsv").read_text("ascii").splitlines()
        }
        cls.catalog = {
            r.split("\t")[0]: r.split("\t")[1:] for r in
            (DATA / "jazz-encyclopedia.tsv").read_text("ascii").splitlines()
        }

    def test_large_independent_research_round_and_older_protection(self):
        metrics = audit()
        self.assertEqual(metrics["original_editorials_unchanged"], 63)
        self.assertEqual(metrics["earlier_researched_whole_lines_unchanged"], 87)
        self.assertEqual(metrics["new_individual_dossiers"], 445)
        self.assertEqual(metrics["previous_editorial_wave_unchanged"], 161)
        self.assertEqual(metrics["previous_individual_dossiers_unchanged"], 74)
        self.assertEqual(metrics["previous_spark_facts_unchanged"], 14)
        self.assertGreaterEqual(metrics["concrete_attributed_claims"], 897)
        self.assertEqual(metrics["editorial_spark_claims_bound_by_id"], 150)
        self.assertEqual(metrics["research_capsules_on_SD_source"], 532)
        self.assertEqual(metrics["other_unreviewed_catalog_entries"], 903)
        self.assertEqual(metrics["second_editorial_wave_unchanged"], 199)
        self.assertEqual(metrics["second_individual_dossiers_unchanged"], 112)
        self.assertEqual(metrics["second_spark_facts_unchanged"], 46)
        self.assertEqual(metrics["third_editorial_wave_unchanged"], 213)
        self.assertEqual(metrics["third_individual_dossiers_unchanged"], 126)
        self.assertEqual(metrics["third_spark_facts_unchanged"], 56)
        self.assertEqual(metrics["third_final_editorial_wave_unchanged"], 225)
        self.assertEqual(metrics["third_final_individual_dossiers_unchanged"], 138)
        self.assertEqual(metrics["third_final_spark_facts_unchanged"], 68)
        self.assertEqual(metrics["fourth_editorial_wave_unchanged"], 256)
        self.assertEqual(metrics["fourth_individual_dossiers_unchanged"], 169)
        self.assertEqual(metrics["fourth_spark_facts_unchanged"], 93)
        self.assertEqual(metrics["fifth_editorial_wave_unchanged"], 281)
        self.assertEqual(metrics["fifth_individual_dossiers_unchanged"], 194)
        self.assertEqual(metrics["fifth_spark_facts_unchanged"], 112)
        self.assertEqual(metrics["sixth_editorial_wave_unchanged"], 313)
        self.assertEqual(metrics["sixth_individual_dossiers_unchanged"], 226)
        self.assertEqual(metrics["sixth_spark_facts_unchanged"], 120)
        self.assertEqual(metrics["seventh_editorial_wave_unchanged"], 352)
        self.assertEqual(metrics["seventh_individual_dossiers_unchanged"], 265)
        self.assertEqual(metrics["seventh_spark_facts_unchanged"], 125)
        self.assertEqual(metrics["eighth_editorial_wave_unchanged"], 392)
        self.assertEqual(metrics["eighth_individual_dossiers_unchanged"], 305)
        self.assertEqual(metrics["eighth_spark_facts_unchanged"], 133)
        self.assertEqual(metrics["ninth_editorial_wave_unchanged"], 433)
        self.assertEqual(metrics["ninth_individual_dossiers_unchanged"], 346)
        self.assertEqual(metrics["ninth_spark_facts_unchanged"], 140)
        self.assertEqual(metrics["tenth_editorial_wave_unchanged"], 468)
        self.assertEqual(metrics["tenth_individual_dossiers_unchanged"], 381)
        self.assertEqual(metrics["tenth_spark_facts_unchanged"], 147)
        self.assertEqual(metrics["eleventh_editorial_wave_unchanged"], 506)
        self.assertEqual(metrics["eleventh_individual_dossiers_unchanged"], 419)
        self.assertEqual(metrics["eleventh_spark_facts_unchanged"], 150)
        self.assertEqual(metrics["eleventh_audited_editorial_wave_unchanged"], 507)
        self.assertEqual(metrics["eleventh_audited_dossiers_unchanged"], 420)
        self.assertEqual(metrics["eleventh_audited_spark_unchanged"], 150)
        self.assertEqual(metrics["eleventh_source_qa_editorial_wave_unchanged"], 510)
        self.assertEqual(metrics["eleventh_source_qa_dossiers_unchanged"], 423)
        self.assertEqual(metrics["eleventh_source_qa_spark_unchanged"], 150)
        self.assertEqual(metrics["twelfth_editorial_wave_unchanged"], 518)
        self.assertEqual(metrics["twelfth_dossiers_unchanged"], 431)
        self.assertEqual(metrics["twelfth_spark_unchanged"], 150)
        self.assertEqual(metrics["twelfth_research02_editorial_wave_unchanged"], 521)
        self.assertEqual(metrics["twelfth_research02_dossiers_unchanged"], 434)
        self.assertEqual(metrics["twelfth_research02_spark_unchanged"], 150)
        self.assertEqual(metrics["thirteenth_editorial_wave_unchanged"], 532)
        self.assertEqual(metrics["thirteenth_dossiers_unchanged"], 445)
        self.assertEqual(metrics["thirteenth_spark_unchanged"], 150)
        self.assertEqual(set(self.dossiers), set(self.researched) - {
            line.split("\t")[0] for line in
            (DATA / "jazz-researched-baseline-20261115.sha256.tsv").read_text("ascii").splitlines()})

    def test_every_source_researched_music_note_actually_on_SD_and_now_playing(self):
        self.assertEqual(set(RESEARCH_CAPTIONS), set(self.researched))
        self.assertEqual(len(RESEARCH_CAPTIONS), 532)
        self.assertEqual(len(self.catalog), 1498)
        verify_index(DATA / "jazz-encyclopedia.tsv", DATA / "jazz-encyclopedia.idx")
        for key in self.dossiers:
            with self.subTest(artist=key):
                summary, citation = self.researched[key]
                name, displayed, _album, displayed_source = self.catalog[key]
                self.assertEqual((displayed, displayed_source), (summary, citation))
                self.assertEqual(self.dossiers[key]["primary_source_url"], citation)
                self.assertTrue(80 <= len(summary) <= 180)
                self.assertTrue(10 <= len(RESEARCH_CAPTIONS[key]) <= 36)
                self.assertNotEqual(name, "")
        self.assertEqual(render_header().encode("ascii"),
                         (ROOT / "PocketJazzRadio/JazzNowPlaying.h").read_bytes())

    def test_spark_editorials_use_exact_artist_dossier_and_journey_topic(self):
        spark = [r.split("\t") for r in
                 (DATA / "jazz-editorial-spark.tsv").read_text("ascii").splitlines()]
        self.assertEqual(len(spark), 150)
        journey_topics = {r.split("\t")[-2] for r in
                          (DATA / "jazz-journey.tsv").read_text("ascii").splitlines()}
        generated = {(r[1], r[2], r[3]) for r in
                     [x.split("\t") for x in (DATA / "jazz-facts.tsv").read_text("ascii").splitlines()]
                     if r[0] == "FACT"}
        self.assertEqual(len({r[0] for r in spark}), len(spark))
        for key, evidence_id, statement, topic, url in spark:
            self.assertIn(key, self.dossiers)
            self.assertIn(topic, journey_topics)
            matching = [e for e in self.dossiers[key]["evidence"]
                        if e["id"] == evidence_id]
            self.assertEqual(len(matching), 1)
            self.assertEqual(matching[0]["claim"], statement)
            self.assertEqual(matching[0]["source_url"], url)
            self.assertIn((statement, topic, url), generated)

    def _copy_sources(self, temp):
        target = Path(temp) / "data"
        target.mkdir()
        for name in ("jazz-researched-context.tsv", "jazz-editorial-bios.tsv",
                     "jazz-encyclopedia-seed.tsv",
                     "jazz-researched-baseline-20261115.sha256.tsv",
                     "jazz-editorial-dossiers.json",
                     "jazz-editorial-spark.tsv", "jazz-journey.tsv",
                     "jazz-researched-baseline-20261115-wave1.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave1.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave2.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave2.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave2.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave3.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave3.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave3.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave3-final.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave3-final.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave3-final.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave4.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave4.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave4.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave5.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave5.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave5.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave6.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave6.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave6.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave7.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave7.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave7.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave8.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave8.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave8.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave9.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave9.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave9.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave10.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave10.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave10.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave11.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave11.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave11.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave11-audited.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave11-audited.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave11-audited.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave11-source-qa.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave11-source-qa.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave11-source-qa.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave12-research01.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave12-research01.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave12-research01.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave12-research02.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave12-research02.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave12-research02.sha256.tsv",
                     "jazz-researched-baseline-20261115-wave13.sha256.tsv",
                     "jazz-editorial-dossiers-baseline-20261115-wave13.sha256.tsv",
                     "jazz-editorial-spark-baseline-20261115-wave13.sha256.tsv"):
            shutil.copyfile(DATA / name, target / name)
        return target

    def _tamper_spark(self, data, field, value, artist=None):
        path = data / "jazz-editorial-spark.tsv"
        rows = [line.split("\t") for line in path.read_text("ascii").splitlines()]
        index = next((i for i, row in enumerate(rows) if row[0] == artist), 0)
        rows[index][field] = value
        path.write_bytes(("".join("\t".join(row) + "\n" for row in rows)).encode("ascii"))
        return json.loads((data / "jazz-editorial-dossiers.json").read_text("ascii"))

    def test_spark_text_substitution_with_valid_url_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            docs = self._tamper_spark(
                data, 2, "Jazz cellist Abdul Wadud founded the New Orleans Jazz Orchestra in 1977.")
            with self.assertRaisesRegex(ValueError, "claim text mismatch"):
                load_verified_editorial_spark(data, docs)
            with self.assertRaisesRegex(ValueError, "claim text mismatch"):
                audit(data)

    def test_unknown_evidence_id_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            docs = self._tamper_spark(data, 1, "ev99")
            with self.assertRaisesRegex(ValueError, "evidence ID not found"):
                load_verified_editorial_spark(data, docs)
            with self.assertRaisesRegex(ValueError, "evidence ID not found"):
                audit(data)

    def test_correct_claim_and_id_with_wrong_dossier_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            source_for_another_claim = self.dossiers["abdul-wadud"]["evidence"][1]["source_url"]
            docs = self._tamper_spark(data, 4, source_for_another_claim, "abdul-wadud")
            with self.assertRaisesRegex(ValueError, "evidence source mismatch"):
                load_verified_editorial_spark(data, docs)
            with self.assertRaisesRegex(ValueError, "evidence source mismatch"):
                audit(data)

    def test_dossier_evidence_ids_cannot_be_duplicated(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(path.read_text("ascii"))
            docs["bob-berg"]["evidence"][1]["id"] = "ev01"
            path.write_bytes((json.dumps(docs,sort_keys=True)+"\n").encode("ascii"))
            with self.assertRaisesRegex(ValueError, "evidence|previously reviewed"):
                audit(data)

    def test_two_distinct_claims_for_same_artist_are_not_two_spark_slots(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            path = data / "jazz-editorial-spark.tsv"
            rows = path.read_text("ascii").splitlines()
            different_claim = self.dossiers["abdul-wadud"]["evidence"][1]
            rows.append("\t".join(("abdul-wadud", different_claim["id"],
                                   different_claim["claim"], "Free jazz",
                                   different_claim["source_url"])))
            path.write_bytes(("\n".join(rows) + "\n").encode("ascii"))
            with self.assertRaisesRegex(ValueError, "duplicate artist"):
                load_verified_editorial_spark(
                    data, json.loads((data / "jazz-editorial-dossiers.json").read_text("ascii")))
            with self.assertRaisesRegex(ValueError, "duplicate artist"):
                audit(data)

    def test_cross_artist_dossier_claim_swap_is_not_a_valid_proof(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            other = self.dossiers["bob-berg"]["evidence"][0]
            path = data / "jazz-editorial-spark.tsv"
            rows = [line.split("\t") for line in path.read_text("ascii").splitlines()]
            entry = next(x for x in rows if x[0] == "abdul-wadud")
            entry[1], entry[2], entry[4] = other["id"], other["claim"], other["source_url"]
            path.write_bytes(("".join("\t".join(x) + "\n" for x in rows)).encode("ascii"))
            with self.assertRaisesRegex(ValueError, "claim text mismatch"):
                audit(data)

    def test_pinned_spark_cannot_change_in_lockstep_with_dossier(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            docs_path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(docs_path.read_text("ascii"))
            entry = docs["abdul-wadud"]["evidence"][0]
            new_sentence = "Abdul Wadud produced an imaginary jazz album in 1977."
            entry["claim"] = new_sentence
            docs_path.write_bytes((json.dumps(docs, sort_keys=True) + "\n").encode("ascii"))
            spark_path = data / "jazz-editorial-spark.tsv"
            spark = [line.split("\t") for line in spark_path.read_text("ascii").splitlines()]
            next(x for x in spark if x[0] == "abdul-wadud")[2] = new_sentence
            spark_path.write_bytes(("".join("\t".join(x) + "\n" for x in spark)).encode("ascii"))
            with self.assertRaisesRegex(ValueError, "previously reviewed editorial dossier changed"):
                audit(data)

    def test_fourth_wave_dossier_and_spark_lockstep_forgery_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            dossier_path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(dossier_path.read_text("ascii"))
            evidence = docs["alfred-ellis"]["evidence"][1]
            evidence["claim"] = "Pee Wee Ellis allegedly composed an imaginary 1969 album."
            dossier_path.write_text(json.dumps(docs, sort_keys=True), encoding="ascii")
            spark_path = data / "jazz-editorial-spark.tsv"
            lines = [line.split("\t") for line in spark_path.read_text("ascii").splitlines()]
            next(line for line in lines if line[0] == "alfred-ellis")[2] = evidence["claim"]
            spark_path.write_text("\n".join("\t".join(line) for line in lines) + "\n",
                                  encoding="ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed .* dossier changed"):
                audit(data)

    def test_fourth_wave_source_and_key_drift_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            spark_path = data / "jazz-editorial-spark.tsv"
            lines = [line.split("\t") for line in spark_path.read_text("ascii").splitlines()]
            row = next(line for line in lines if line[0] == "alfred-ellis")
            row[4] = self.dossiers["melba-liston"]["primary_source_url"]
            spark_path.write_text("\n".join("\t".join(line) for line in lines) + "\n",
                                  encoding="ascii")
            with self.assertRaisesRegex(ValueError, "evidence source mismatch"):
                audit(data)

    def test_fourth_wave_encyclopedia_and_hints_are_integrated_not_just_json(self):
        for key in ("alfred-ellis", "cachao", "melba-liston", "buschi-niebergall",
                    "coco-schumann", "jimmy-lyons", "celso-fonseca", "anke-helfrich"):
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertGreaterEqual(len(self.dossiers[key]["evidence"]), 2)

    def test_fifth_wave_index_and_exact_hints_are_not_only_staged_json(self):
        for key in ("carmen-lundy", "gerald-wilson", "giuseppi-logan",
                    "lionel-loueke", "dorothy-ashby", "akiko-yano"):
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertGreaterEqual(len(self.dossiers[key]["evidence"]), 2)

    def test_fifth_wave_lockstep_evidence_and_spark_forgery_fails_pin(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(path.read_text("ascii"))
            docs["giuseppi-logan"]["evidence"][1]["claim"] = "Fictitious release."
            path.write_text(json.dumps(docs), encoding="ascii")
            spark_path = data / "jazz-editorial-spark.tsv"
            rows = [r.split("\t") for r in spark_path.read_text("ascii").splitlines()]
            next(r for r in rows if r[0] == "giuseppi-logan")[2] = "Fictitious release."
            spark_path.write_text("\n".join("\t".join(r) for r in rows) + "\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed .* dossier changed"):
                audit(data)

    def test_sixth_wave_offline_index_hints_and_source_evidence(self):
        for key in ("charly-antolini", "snooky-young", "margie-hyams",
                    "gianni-gebbia", "glauco-venier", "guillermo-klein",
                    "sibongile-khumalo", "franklin-kiermyer", "nathalie-loriers"):
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])
                self.assertGreaterEqual(len(self.dossiers[key]["evidence"]), 2)

    def test_sixth_wave_lockstep_spark_forgery_fails_pinned_dossier(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(path.read_text("ascii"))
            docs["snooky-young"]["evidence"][0]["claim"] = "Imaginary swing album in 1941."
            path.write_text(json.dumps(docs, sort_keys=True), encoding="ascii")
            spark_path = data / "jazz-editorial-spark.tsv"
            rows = [r.split("\t") for r in spark_path.read_text("ascii").splitlines()]
            next(r for r in rows if r[0] == "snooky-young")[2] = "Imaginary swing album in 1941."
            spark_path.write_text("\n".join("\t".join(r) for r in rows) + "\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed .* dossier changed"):
                audit(data)

    def test_seventh_wave_is_present_in_offline_catalog_and_exact_hints(self):
        for key in ("attila-zoller", "boyd-raeburn", "carl-kress",
                    "chico-o-farrill", "harold-lopez-nussa", "john-carter",
                    "lotte-anker", "mongezi-feza", "wolfgang-puschnig"):
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertGreaterEqual(len(self.dossiers[key]["evidence"]), 2)

    def test_seventh_wave_coordinated_dossier_and_spark_forgery_is_rejected(self):
        # Editing both the claim and matching Spark text still cannot bypass
        # the separately pinned source-reviewed dossier object checksum.
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            dossier_path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(dossier_path.read_text("ascii"))
            docs["boyd-raeburn"]["evidence"][0]["claim"] = "Invented bebop concert at a fictional venue."
            dossier_path.write_text(json.dumps(docs, sort_keys=True), encoding="ascii")
            facts_path = data / "jazz-editorial-spark.tsv"
            rows = [r.split("\t") for r in facts_path.read_text("ascii").splitlines()]
            next(r for r in rows if r[0] == "boyd-raeburn")[2] = "Invented bebop concert at a fictional venue."
            facts_path.write_text("\n".join("\t".join(r) for r in rows) + "\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed .* dossier changed"):
                audit(data)

    def test_seventh_wave_broken_evidence_id_and_source_are_rejected(self):
        for field, value, error in (("id", "ev99", "evidence ID not found"),
                                    ("source", "https://example.invalid/fake", "evidence source mismatch")):
            with self.subTest(field=field):
                with tempfile.TemporaryDirectory() as temp:
                    data = self._copy_sources(temp)
                    path = data / "jazz-editorial-spark.tsv"
                    rows = [r.split("\t") for r in path.read_text("ascii").splitlines()]
                    row = next(r for r in rows if r[0] == "boyd-raeburn")
                    row[1 if field == "id" else 4] = value
                    path.write_text("\n".join("\t".join(r) for r in rows) + "\n", encoding="ascii")
                    with self.assertRaisesRegex(ValueError, error):
                        audit(data)

    def test_eighth_wave_reaches_generated_catalog_and_exact_now_playing(self):
        for key in ("aaron-sachs", "alice-babs", "bill-barber",
                    "bola-sete", "gene-gifford", "omer-simeon",
                    "karl-seglem", "aziza-mustafa-zadeh", "nicole-mitchell",
                    "paul-rutherford", "michael-bisio", "norma-winstone"):
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertGreaterEqual(len(self.dossiers[key]["evidence"]), 2)

    def test_eighth_wave_coordinated_spark_and_dossier_mutation_fails_pin(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            dossiers_path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(dossiers_path.read_text("ascii"))
            docs["aaron-sachs"]["evidence"][0]["claim"] = "Invented clarinet session with fake personnel."
            dossiers_path.write_text(json.dumps(docs, sort_keys=True), encoding="ascii")
            spark_path = data / "jazz-editorial-spark.tsv"
            rows = [r.split("\t") for r in spark_path.read_text("ascii").splitlines()]
            next(r for r in rows if r[0] == "aaron-sachs")[2] = "Invented clarinet session with fake personnel."
            spark_path.write_text("\n".join("\t".join(r) for r in rows) + "\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed .* dossier changed"):
                audit(data)

    def test_eighth_wave_wrong_id_source_and_claim_are_rejected(self):
        for index, replacement, error in (
                (1, "ev99", "evidence ID not found"),
                (2, "Wrongly attributed personnel and fake session.", "claim text mismatch"),
                (4, "https://unverified.example/another-session", "evidence source mismatch")):
            with self.subTest(column=index):
                with tempfile.TemporaryDirectory() as temp:
                    data = self._copy_sources(temp)
                    path = data / "jazz-editorial-spark.tsv"
                    rows = [r.split("\t") for r in path.read_text("ascii").splitlines()]
                    next(r for r in rows if r[0] == "aaron-sachs")[index] = replacement
                    path.write_text("\n".join("\t".join(r) for r in rows) + "\n", encoding="ascii")
                    with self.assertRaisesRegex(ValueError, error):
                        audit(data)

    def test_ninth_wave_is_reachable_in_generated_offline_catalog_and_hints(self):
        for key in ("cat-anderson", "conte-candoli", "chris-flory",
                    "carl-saunders", "frank-socolow", "ray-nance",
                    "miya-masaoka", "omer-avital", "motohiko-hino",
                    "stanley-cowell", "michael-white", "sivuca",
                    "morten-gunnar-larsen"):
            with self.subTest(artist=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertGreaterEqual(len(self.dossiers[key]["evidence"]), 2)

    def test_ninth_wave_coordinated_claim_and_spark_mutation_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            dossier_file = data / "jazz-editorial-dossiers.json"
            dossiers = json.loads(dossier_file.read_text("ascii"))
            dossiers["cat-anderson"]["evidence"][0]["claim"] = "Invented Ellington recording with fabricated personnel."
            dossier_file.write_text(json.dumps(dossiers, sort_keys=True), encoding="ascii")
            spark_file = data / "jazz-editorial-spark.tsv"
            lines = [r.split("\t") for r in spark_file.read_text("ascii").splitlines()]
            next(r for r in lines if r[0] == "cat-anderson")[2] = "Invented Ellington recording with fabricated personnel."
            spark_file.write_text("\n".join("\t".join(r) for r in lines) + "\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed .* dossier changed"):
                audit(data)

    def test_ninth_wave_wrong_id_claim_and_source_are_rejected(self):
        for column, new, expected_error in (
                (1, "ev99", "evidence ID not found"),
                (2, "Fictitious 1950 personnel on a made-up session.", "claim text mismatch"),
                (4, "https://unverified.example/false-record", "evidence source mismatch")):
            with self.subTest(column=column):
                with tempfile.TemporaryDirectory() as temp:
                    data = self._copy_sources(temp)
                    spark_file = data / "jazz-editorial-spark.tsv"
                    rows = [r.split("\t") for r in spark_file.read_text("ascii").splitlines()]
                    next(r for r in rows if r[0] == "cat-anderson")[column] = new
                    spark_file.write_text("\n".join("\t".join(r) for r in rows) + "\n", encoding="ascii")
                    with self.assertRaisesRegex(ValueError, expected_error):
                        audit(data)

    def test_tenth_wave_exact_catalog_sources_and_now_playing_hints(self):
        for key in ("esperanza-spalding", "catherine-russell",
                    "buck-hill", "gil-coggins", "gene-roland",
                    "jack-montrose", "morris-goldberg", "motoharu-yoshizawa",
                    "mike-westbrook", "bobbi-humphrey", "steve-rodby",
                    "kaveh-rastegar", "steve-turre"):
            with self.subTest(artist=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertGreaterEqual(len(self.dossiers[key]["evidence"]), 2)

    def test_tenth_wave_coordinated_spark_dossier_forgery_fails_immutable_pin(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            dossier_file = data / "jazz-editorial-dossiers.json"
            docs = json.loads(dossier_file.read_text("ascii"))
            docs["buck-hill"]["evidence"][0]["claim"] = "Fictional recording at a nonexistent studio."
            dossier_file.write_text(json.dumps(docs, sort_keys=True), encoding="ascii")
            spark_file = data / "jazz-editorial-spark.tsv"
            records = [r.split("\t") for r in spark_file.read_text("ascii").splitlines()]
            next(r for r in records if r[0] == "buck-hill")[2] = "Fictional recording at a nonexistent studio."
            spark_file.write_text("\n".join("\t".join(r) for r in records) + "\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed .* dossier changed"):
                audit(data)

    def test_tenth_wave_wrong_spark_id_text_source_and_duplicate_are_rejected(self):
        for column, value, error in (
                (1, "ev99", "evidence ID not found"),
                (2, "Imaginary Muse recording with no musicians.", "claim text mismatch"),
                (4, "https://bad.invalid/not-the-source", "evidence source mismatch"),
                (-1, "duplicate", "duplicate|Duplicate")):
            with self.subTest(column=column):
                with tempfile.TemporaryDirectory() as temp:
                    data = self._copy_sources(temp)
                    spark_file = data / "jazz-editorial-spark.tsv"
                    records = [r.split("\t") for r in spark_file.read_text("ascii").splitlines()]
                    row = next(r for r in records if r[0] == "buck-hill")
                    if column == -1:
                        records.append(list(row))
                    else:
                        row[column] = value
                    spark_file.write_text("\n".join("\t".join(r) for r in records) + "\n", encoding="ascii")
                    with self.assertRaisesRegex(ValueError, error):
                        audit(data)

    def test_eleventh_wave_exact_sd_catalog_and_now_playing(self):
        approved = json.loads((ROOT / "out/editorial-next-20261115/editorial-wave11-decisions.json").read_text("utf-8"))["accepted"]
        self.assertEqual(len(approved), 38)
        for key in approved:
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])
                self.assertIn(key, RESEARCH_CAPTIONS)
        self.assertIn("nathan-davis", self.catalog)
        self.assertIn("nathan-davis", RESEARCH_CAPTIONS)
        self.assertIn("Happy Girl", self.researched["nathan-davis"][0])
        for key, distinct in (("fred-guy", "East St. Louis Toodle-O"),
                              ("george-tucker", "Us Three"),
                              ("othello-molineaux", "Opus Pocus")):
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertIn(distinct, self.researched[key][0])
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
        self.assertIn("Jackie Dougan", self.researched["stan-tracey"][0])
        self.assertNotIn("Jackie Douglas", self.researched["stan-tracey"][0])

    def test_latest_source_qa_new_artist_identity_and_precise_musical_proofs(self):
        proof = json.loads((DATA / "jazz-music-evidence.json").read_text("utf-8"))
        expected = {"fred-guy": "W143705-3", "george-tucker": "April 20, 1960",
                    "othello-molineaux": "Opus Pocus"}
        for key, detail in expected.items():
            with self.subTest(key=key):
                self.assertEqual(proof[key]["identity_link_strength"], "reciprocal")
                self.assertEqual(proof[key]["status"], "checked_mb")
                self.assertFalse(proof[key]["ambiguous_mbids"])
                self.assertTrue(any(detail in item["claim"] for item in self.dossiers[key]["evidence"]))
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])

    def test_wave12_new_artist_ancestry_exact_identity_citation_and_sd_integration(self):
        approved = json.loads((ROOT / "out/editorial-next-20261115/editorial-wave12-research01-accepted.json").read_text("ascii"))
        self.assertEqual(len(approved["accepted"]), 8)
        self.assertEqual(approved["source_corrections"], ["fred-guy", "nathan-davis"])
        stage = json.loads((ROOT / "out/editorial-next-20261115/editorial-wave12-research01-staged.json").read_text("ascii"))
        identity = json.loads((DATA / "jazz-music-evidence.json").read_text("utf-8"))
        for key in approved["accepted"]:
            with self.subTest(key=key):
                self.assertIn(key, self.catalog)
                self.assertEqual(self.catalog[key][1], self.researched[key][0])
                self.assertEqual(self.catalog[key][3], self.researched[key][1])
                self.assertEqual(self.dossiers[key]["summary"], self.researched[key][0])
                self.assertEqual(self.dossiers[key]["evidence"], stage["new"][key]["evidence"])
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertEqual(identity[key]["identity_link_strength"], "reciprocal")
                self.assertEqual(identity[key]["mb_reciprocal_qids"], [identity[key]["qid"]])
                self.assertFalse(identity[key]["ambiguous_mbids"])
        self.assertIn("recorded Happy Girl in 1965", self.researched["nathan-davis"][0])
        self.assertNotIn("sessions in New York", self.researched["fred-guy"][0])

    def test_wave12_research02_primary_musician_sources_and_exact_offline_lookup(self):
        accepted = json.loads((ROOT / "out/editorial-next-20261115/editorial-wave12-research02-accepted.json").read_text("ascii"))
        staged = json.loads((ROOT / "out/editorial-next-20261115/editorial-wave12-research02-staged.json").read_text("ascii"))
        identities = json.loads((DATA / "jazz-music-evidence.json").read_text("utf-8"))
        self.assertEqual(accepted["accepted"], ["bert-boeren", "bruce-ditmas", "chris-pyne"])
        self.assertFalse(accepted["source_corrections"])
        for key in accepted["accepted"]:
            with self.subTest(key=key):
                item = staged["new"][key]
                self.assertIn(key, self.catalog)
                self.assertIn(key, RESEARCH_CAPTIONS)
                self.assertEqual(self.catalog[key][1], item["summary"])
                self.assertEqual(self.catalog[key][3], item["primary_source_url"])
                self.assertEqual(self.dossiers[key]["evidence"], item["evidence"])
                self.assertEqual(identities[key]["status"], "checked_mb")
                self.assertEqual(identities[key]["identity_link_strength"], "reciprocal")
                self.assertEqual(identities[key]["mb_reciprocal_qids"], [identities[key]["qid"]])
                self.assertFalse(identities[key]["ambiguous_mbids"])

    def test_wave12_research02_new_sha_protects_source_and_independent_claim(self):
        for variant, expected in (("source", "wave12 research02 biography changed"),
                                  ("claim", "wave12 research02 dossier changed")):
            with self.subTest(variant=variant), tempfile.TemporaryDirectory() as temp:
                data = self._copy_sources(temp)
                if variant == "source":
                    path = data / "jazz-researched-context.tsv"
                    raw = path.read_text("ascii")
                    self.assertIn("The Brass Project", raw)
                    path.write_text(raw.replace("The Brass Project", "Imaginary Brass Project", 1), "ascii")
                else:
                    path = data / "jazz-editorial-dossiers.json"
                    docs = json.loads(path.read_text("ascii"))
                    docs["bruce-ditmas"]["evidence"][0]["claim"] = "Invented session personnel and album credits."
                    path.write_text(json.dumps(docs, sort_keys=True), "ascii")
                with self.assertRaisesRegex(ValueError, expected):
                    audit(data)

    def test_wave13_source_checked_promotion_and_exact_identity(self):
        accepted = json.loads((ROOT / "out/editorial-next-20261115/editorial-wave13-accepted.json").read_text("ascii"))
        stage = json.loads((ROOT / "out/editorial-next-20261115/editorial-wave13-staged.json").read_text("ascii"))
        identity = json.loads((DATA / "jazz-music-evidence.json").read_text("utf-8"))
        self.assertEqual(len(accepted["accepted"]), 11)
        self.assertFalse(accepted["source_corrections"])
        self.assertEqual(set(accepted["accepted"]), set(stage["new"]))
        for key in accepted["accepted"]:
            with self.subTest(key=key):
                item = stage["new"][key]
                self.assertEqual(self.catalog[key][1], item["summary"])
                self.assertEqual(self.catalog[key][3], item["primary_source_url"])
                self.assertEqual(self.dossiers[key]["evidence"], item["evidence"])
                self.assertEqual(RESEARCH_CAPTIONS[key], item["now_playing_hint"])
                self.assertEqual(identity[key]["identity_link_strength"], "reciprocal")
                self.assertEqual(identity[key]["mb_reciprocal_qids"], [identity[key]["qid"]])
                self.assertFalse(identity[key]["ambiguous_mbids"])
        self.assertIn("Dark Moon", self.researched["holly-cole"][0])
        self.assertNotIn("2019", self.researched["holly-cole"][0])

    def test_wave13_independent_hashes_reject_falsified_original_sources(self):
        for kind in ("source", "claim", "source_url", "evidence_id"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temporary:
                data = self._copy_sources(temporary)
                if kind == "source":
                    path = data / "jazz-researched-context.tsv"
                    content = path.read_text("ascii")
                    self.assertIn("Village Vanguard", content)
                    path.write_text(content.replace("Village Vanguard", "Invented New York club", 1), "ascii")
                    message = "wave13 researched biography changed"
                else:
                    path = data / "jazz-editorial-dossiers.json"
                    docs = json.loads(path.read_text("ascii"))
                    evidence = docs["champian-fulton"]["evidence"][0]
                    if kind == "claim":
                        evidence["claim"] = "Invented personnel and recording date."
                    elif kind == "source_url":
                        evidence["source_url"] = "https://invalid.example/test"
                    else:
                        evidence["id"] = "ev99"
                    path.write_text(json.dumps(docs,sort_keys=True), "ascii")
                    message = "wave13 researched dossier changed"
                with self.assertRaisesRegex(ValueError, message):
                    audit(data)

    def test_wave12_pins_reject_source_and_dossier_modification(self):
        for modification, expected in (("source", "wave12 researched biography changed"),
                                       ("claim", "wave12 dossier changed")):
            with self.subTest(modification=modification), tempfile.TemporaryDirectory() as temporary:
                data = self._copy_sources(temporary)
                if modification == "source":
                    file = data / "jazz-researched-context.tsv"
                    rows = file.read_text("ascii")
                    rows = rows.replace("Dec. 24, 1947", "Dec. 24, 1948", 1)
                    file.write_text(rows, "ascii")
                else:
                    file = data / "jazz-editorial-dossiers.json"
                    docs = json.loads(file.read_text("ascii"))
                    docs["al-lucas"]["evidence"][0]["claim"] = "Falsified original Savoy band session."
                    file.write_text(json.dumps(docs, sort_keys=True), "ascii")
                with self.assertRaisesRegex(ValueError, expected):
                    audit(data)

    def test_latest_source_qa_pins_reject_new_and_coordinated_spark_tampering(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            doc_path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(doc_path.read_text("ascii"))
            docs["fred-guy"]["evidence"][0]["claim"] = "Fabricated Columbia date and musicians."
            doc_path.write_text(json.dumps(docs, sort_keys=True), "ascii")
            with self.assertRaisesRegex(ValueError, "wave12 dossier changed"):
                audit(data)
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            doc_path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(doc_path.read_text("ascii"))
            revised = "Invented concert and completely false personnel for a fictional 1948 session."
            next(x for x in docs["carlo-krahmer"]["evidence"] if x["id"] == "ev02")["claim"] = revised
            doc_path.write_text(json.dumps(docs, sort_keys=True), "ascii")
            spark_path = data / "jazz-editorial-spark.tsv"
            lines = [line.split("\t") for line in spark_path.read_text("ascii").splitlines()]
            next(line for line in lines if line[0] == "carlo-krahmer")[2] = revised
            spark_path.write_text("\n".join("\t".join(row) for row in lines) + "\n", "ascii")
            with self.assertRaisesRegex(ValueError, "source-QA wave11 dossier changed"):
                audit(data)

    def test_audited_wave11_revision_tamper_fails_new_dossier_pin(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            file = data / "jazz-editorial-dossiers.json"
            records = json.loads(file.read_text("ascii"))
            records["ann-burton"]["evidence"][1]["claim"] = "Invented Epic original issue in 1967."
            file.write_text(json.dumps(records, sort_keys=True), "ascii")
            with self.assertRaisesRegex(ValueError, "audited wave11 dossier changed"):
                audit(data)

    def test_audited_new_dossier_and_source_pins_are_separate(self):
        for target, phrase in (("jazz-researched-context.tsv", "wave12 researched biography changed"),
                               ("jazz-editorial-dossiers.json", "wave12 dossier changed")):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as temp:
                data = self._copy_sources(temp)
                path = data / target
                if path.suffix == ".tsv":
                    text = path.read_text("ascii")
                    text = text.replace("Flute in the Blues", "Fake composition", 1) if "Flute in the Blues" in text else text.replace("Happy Girl", "Fake Girl", 1)
                    path.write_text(text, "ascii")
                else:
                    docs = json.loads(path.read_text("ascii"))
                    docs["nathan-davis"]["evidence"][0]["claim"] = "Made-up 1965 personnel"
                    path.write_text(json.dumps(docs), "ascii")
                with self.assertRaisesRegex(ValueError, phrase):
                    audit(data)

    def test_eleventh_wave_spark_mismatch_and_duplicate_are_rejected(self):
        for column, replacement, expected in (
                (1, "ev99", "evidence ID not found"),
                (2, "Fabricated historical source and personnel.", "claim text mismatch"),
                (4, "https://bad.invalid/fabricated", "evidence source mismatch"),
                (-1, None, "duplicate artist or fact")):
            with self.subTest(column=column):
                with tempfile.TemporaryDirectory() as temp:
                    data = self._copy_sources(temp)
                    path = data / "jazz-editorial-spark.tsv"
                    records = [line.split("\t") for line in path.read_text("ascii").splitlines()]
                    record = next(r for r in records if r[0] == "calvin-newborn")
                    if column == -1:
                        records.append(list(record))
                    else:
                        record[column] = replacement
                    path.write_text("\n".join("\t".join(r) for r in records) + "\n", "ascii")
                    with self.assertRaisesRegex(ValueError, expected):
                        audit(data)

    def test_eleventh_wave_coordinated_claim_and_spark_tampering_fails_pin(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            doc_path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(doc_path.read_text("ascii"))
            docs["calvin-newborn"]["evidence"][0]["claim"] = "Invented personnel from a fictional 1956 session."
            doc_path.write_text(json.dumps(docs,sort_keys=True), "ascii")
            spark_path = data / "jazz-editorial-spark.tsv"
            rows = [line.split("\t") for line in spark_path.read_text("ascii").splitlines()]
            next(row for row in rows if row[0] == "calvin-newborn")[2] = docs["calvin-newborn"]["evidence"][0]["claim"]
            spark_path.write_text("\n".join("\t".join(row) for row in rows) + "\n", "ascii")
            with self.assertRaisesRegex(ValueError, "previously reviewed 419-wave dossier changed"):
                audit(data)

    def test_tampered_research_citation_does_not_pass_as_cited(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            path = data / "jazz-researched-context.tsv"
            old = path.read_text("ascii")
            self.assertIn("https://", old)
            new = old.replace("https://", "https://changed.invalid/", 1)
            path.write_bytes(new.encode("ascii"))
            with self.assertRaisesRegex(ValueError, "dossier|source|changed|original"):
                audit(data)

    def test_new_biography_and_evidence_drift_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            data = self._copy_sources(temp)
            path = data / "jazz-editorial-dossiers.json"
            docs = json.loads(path.read_text("ascii"))
            key = next(iter(docs))
            docs[key]["summary"] += " Invented."
            path.write_text(json.dumps(docs), encoding="ascii")
            with self.assertRaisesRegex(ValueError, "dossier"):
                audit(data)

    def test_name_quoting_and_ensembles_are_not_fuzzy_identity_merges(self):
        header = (ROOT / "PocketJazzRadio/JazzNowPlaying.h").read_text("ascii")
        self.assertIn(r'Johnny \"Hammond\" Smith', header)
        self.assertIn("Franco D'Andrea", header)
        self.assertIn('"The Oscar Peterson Trio"', header)
        self.assertIn('"Satoko Fujii"', header)


if __name__ == "__main__":
    unittest.main()
