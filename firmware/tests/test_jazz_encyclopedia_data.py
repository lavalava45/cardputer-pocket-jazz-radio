"""Offline full-size jazz encyclopedia / Jazz Spark contracts (no network)."""
from pathlib import Path
import hashlib
import json
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"


class VerifiedJazzDataset(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = (DATA / "jazz-encyclopedia.tsv").read_bytes()
        cls.facts_raw = (DATA / "jazz-facts.tsv").read_bytes()
        cls.rows = [line.split("\t") for line in cls.raw.decode("ascii").splitlines()]
        cls.facts = [line.split("\t") for line in cls.facts_raw.decode("ascii").splitlines()]

    def test_more_than_order_of_magnitude_growth_over_original_36(self):
        self.assertGreaterEqual(len(self.rows), 360)
        self.assertLessEqual(len(self.rows), 8000)
        self.assertLessEqual(len(self.raw), 3 * 1024 * 1024)
        self.assertEqual(len({r[0] for r in self.rows}), len(self.rows))

    def test_original_36_verified_album_selections_are_preserved(self):
        original = {
            r.split("\t")[0]: r.split("\t")[2]
            for r in (DATA / "artist-notes.tsv").read_text("ascii").splitlines()
        }
        self.assertEqual(len(original), 36)
        large = {row[0]: row for row in self.rows}
        for key, album in original.items():
            self.assertIn(key, large)
            self.assertEqual(large[key][3], album)

    def test_every_record_follows_bounded_sd_schema_and_has_a_source(self):
        new_editorials = json.loads(
            (DATA / "jazz-editorial-dossiers.json").read_text("ascii"))
        for row in self.rows:
            self.assertEqual(len(row), 5)
            key, artist, bio, album, source = row
            self.assertRegex(key, r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
            for text, maximum in ((key, 64), (artist, 96), (bio, 192),
                                  (album, 112), (source, 224)):
                self.assertTrue(0 < len(text) <= maximum, row)
            self.assertTrue(source.startswith("https://"))
            if key in new_editorials:
                # A mere domain allowlist could approve a mismatched artist's
                # citation. These 74 exact sources are reviewed per claim.
                self.assertEqual(source, new_editorials[key]["primary_source_url"])
            else:
                self.assertIn(source.split("/", 3)[2],
                          ("www.wikidata.org", "musicbrainz.org", "en.wikipedia.org",
                           "www.reuters.com", "www.loc.gov", "www.arts.gov",
                           "americanhistory.si.edu", "folkways-media.si.edu",
                           "store.bluenote.com", "findingaids.loc.gov",
                           "amhistory.si.edu", "www.nps.gov", "www.bluenote.com",
                           "blogs.loc.gov", "collections.si.edu", "nmaahc.si.edu",
                           "fittcollection.macewan.ca", "ecmrecords.com",
                           "www.milesdavis.com", "johntchicai.com",
                           "concord.com", "www.randybrecker.com",
                           "johnclaytonjazz.com", "www.johnmclaughlin.com",
                           "www.smithsonianmag.com", "www.latimes.com",
                           "www.americanhistory.si.edu", "www.waltzingparke.com",
                           "www.eurovision.com", "www.kcb.be", "sahistory.org.za",
                           "shop.bluenote.com", "www.jazzdisco.org",
                           "gulda-school-of-music.com", "www.toquinho.com.br",
                           "www.tunecore.co.jp", "spah.org",
                           "templeofsound.at", "www.jazzzeitung.de",
                           "hfm-wuerzburg.de", "www.allaboutjazz.com",
                           "www.hmt-leipzig.de", "www.jazz-fun.de",
                           "www.hfm-nuernberg.de", "www.bbcamerica.com",
                           "www.allmusic.com", "officialbrandx.com",
                           "seatbelts.live", "catalogue.bnf.fr",
                           "www.montreuxjazzfestival.com", "www.mannyalbammusic.com",
                           "www.uncjazzpress.com", "earlbostic.com", "www.wrti.org",
                           "libweb.umkc.edu", "jazz.com"))
            self.assertNotIn("\r", "\t".join(row))
            self.assertFalse(any(ord(ch) < 32 for part in row for ch in part))

    def test_facts_and_attributed_quotes_are_different_types(self):
        self.assertGreaterEqual(len(self.facts), 150)
        self.assertLessEqual(len(self.facts_raw), 512 * 1024)
        self.assertGreaterEqual(sum(r[0] == "QUOTE" for r in self.facts), 25)
        self.assertGreater(sum(r[0] == "FACT" for r in self.facts), 100)
        for row in self.facts:
            self.assertEqual(len(row), 4)
            kind, text, subject, url = row
            self.assertIn(kind, ("FACT", "QUOTE"))
            self.assertTrue(0 < len(text) <= 192)
            self.assertTrue(0 < len(subject) <= 96)
            self.assertTrue(url.startswith("https://"))
            if kind == "QUOTE":
                self.assertIn("wikiquote.org", url)

    def test_source_document_records_exact_hashes(self):
        provenance = (DATA / "JAZZ-ENCYCLOPEDIA-PROVENANCE.md").read_text("utf-8")
        self.assertIn(hashlib.sha256(self.raw).hexdigest(), provenance)
        self.assertIn(hashlib.sha256(self.facts_raw).hexdigest(), provenance)
        self.assertIn("Q15981151", provenance)
        self.assertNotIn("Q15980158", provenance)  # non-fiction writer, not jazz!

    def test_release_packager_installs_content_not_just_source_snapshot(self):
        script = (ROOT / "tools/build.ps1").read_text("utf-8-sig")
        for name in ("jazz-encyclopedia.tsv", "jazz-facts.tsv",
                     "artist-notes.tsv", "artist-notes.idx"):
            self.assertIn(name, script)
        self.assertIn("Entry = ('PocketJazz/' + $dataName)", script)
        self.assertIn("'docs/JAZZ-ENCYCLOPEDIA.md'", script)
        self.assertIn("Where-Object { $_.Extension -in '.m3u', '.pls' }", script)
        self.assertIn("Source = $inputData", script)

    def test_editorial_spark_is_meaningful_and_auto_cards_are_bounded(self):
        curated = (DATA / "jazz-curated-facts.tsv").read_text("ascii").splitlines()
        researched = (DATA / "jazz-editorial-spark.tsv").read_text("ascii").splitlines()
        self.assertGreaterEqual(len(curated), 100)
        self.assertEqual(len(set(curated)), len(curated))
        self.assertTrue(all(len(line.split("\t")) == 3 for line in curated))
        self.assertEqual(len(researched), 150)
        self.assertEqual(len(self.facts), len(curated) + len(researched) + 15 + 25)
        source = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
        for expected in ("SPARK_AUTO_INTERVAL_MS = 12u * 60u * 1000u",
                         "SPARK_AUTO_VISIBLE_MS = 10000u",
                         'p.putBool("sparkAuto", jazzSparkAutoEnabled)',
                         'p.getBool("sparkAuto", true)',
                         "void serviceJazzSpark(uint32_t now)",
                         "audioBufferedBytes.load() < STREAM_BYTES / 2",
                         "serviceJazzSpark(now);"):
            self.assertIn(expected, source)

    def test_key_artist_biographies_are_editorial_and_offline(self):
        bio_rows = (DATA / "jazz-editorial-bios.tsv").read_text("ascii").splitlines()
        self.assertGreaterEqual(len(bio_rows), 60)
        entries = {r[0]: r for r in self.rows}
        for row in bio_rows:
            key, summary, source = row.split("\t")
            self.assertIn(key, entries)
            self.assertEqual(entries[key][2], summary)
            self.assertEqual(entries[key][4], source)
            self.assertGreater(len(summary), 70)


if __name__ == "__main__":
    unittest.main()
