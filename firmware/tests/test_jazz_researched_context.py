"""Evidence-first additions: individually sourced, non-template biographies."""
from pathlib import Path
import json
import sys
import unittest
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"

class ResearchedJazzContext(unittest.TestCase):
    def test_532_distinct_source_bound_musical_biographies_fit_offline_card(self):
        curated = {row.split("\t")[0] for row in
                   (DATA / "jazz-editorial-bios.tsv").read_text("ascii").splitlines()}
        seed = {row.split("\t")[0] for row in
                (DATA / "jazz-encyclopedia-seed.tsv").read_text("ascii").splitlines()}
        lines = (DATA / "jazz-researched-context.tsv").read_text("ascii").splitlines()
        self.assertEqual(len(lines), 532)
        keys = []
        new_editorials = json.loads(
            (DATA / "jazz-editorial-dossiers.json").read_text("ascii"))
        accepted = {"www.reuters.com", "www.loc.gov", "www.arts.gov",
                    "americanhistory.si.edu", "folkways-media.si.edu",
                    "store.bluenote.com", "findingaids.loc.gov",
                    "amhistory.si.edu", "www.nps.gov", "www.bluenote.com",
                    "blogs.loc.gov", "collections.si.edu", "nmaahc.si.edu",
                    "fittcollection.macewan.ca", "ecmrecords.com",
                    "www.milesdavis.com", "johntchicai.com",
                    "concord.com", "www.randybrecker.com", "johnclaytonjazz.com",
                    "www.johnmclaughlin.com", "www.smithsonianmag.com",
                    "www.latimes.com", "www.americanhistory.si.edu",
                    "www.waltzingparke.com", "www.eurovision.com",
                    "www.kcb.be", "sahistory.org.za", "shop.bluenote.com",
                    "www.jazzdisco.org", "gulda-school-of-music.com",
                    "www.toquinho.com.br", "www.tunecore.co.jp",
                    "spah.org", "templeofsound.at",
                    "www.jazzzeitung.de", "hfm-wuerzburg.de",
                    "www.allaboutjazz.com", "www.hmt-leipzig.de",
                    "www.jazz-fun.de", "www.hfm-nuernberg.de",
                    "www.bbcamerica.com", "www.allmusic.com",
                    "officialbrandx.com", "seatbelts.live",
                    "catalogue.bnf.fr", "www.montreuxjazzfestival.com",
                    "www.mannyalbammusic.com", "www.uncjazzpress.com",
                    "earlbostic.com", "www.wrti.org",
                    "libweb.umkc.edu", "jazz.com"}
        for line in lines:
            parts = line.split("\t")
            self.assertEqual(len(parts), 3)
            key, biography, source = parts
            keys.append(key)
            self.assertIn(key, seed)
            self.assertNotIn(key, curated)
            self.assertTrue(biography.isascii())
            self.assertGreaterEqual(len(biography), 80)
            self.assertLessEqual(len(biography), 180)
            self.assertEqual(urlsplit(source).scheme, "https")
            if key in new_editorials:
                self.assertEqual(source, new_editorials[key]["primary_source_url"])
            else:
                self.assertIn(urlsplit(source).netloc, accepted)
            self.assertRegex(biography.lower(),
                             r"pian|sax|jazz|cornet|composer|guitar|conga|blues|"
                             r"clarinet|swing|bebop|bandleader|ragtime|singer|vocal|trumpet|organ|oboi|reed|bass|"
                             r"percussion|harp|drum|tuba|french horn|kora|vibraphon|violin|cornet|trombon|flut|cell|electronic|quartet|orchestra|banjo|steel.pan")
        self.assertEqual(keys, sorted(set(keys)))

    def test_sources_are_references_not_required_online_dependencies(self):
        app = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
        self.assertIn("jazz-encyclopedia.tsv", app)
        self.assertNotIn("jazz-researched-context.tsv", app)
        self.assertNotIn("www.reuters.com", app)

    def test_reviewed_historical_dates_and_music_credits_do_not_regress(self):
        rows = {r.split("\t")[0]: r.split("\t")[1] for r in
                (DATA / "jazz-researched-context.tsv").read_text("ascii").splitlines()}
        self.assertIn("February 1941", rows["duke-ellington-and-his-orchestra"])
        self.assertNotIn("1940", rows["duke-ellington-and-his-orchestra"])
        self.assertIn("1967", rows["al-green"])
        self.assertIn("1925-2020", rows["johnny-mandel"])
        self.assertIn("joined jazz-fusion group Brand X in 1975",
                      rows["phil-collins"])
        self.assertNotIn("co-founded", rows["phil-collins"])
        self.assertIn("EP 'The Watching Silence' was preceded by single",
                      rows["michelle-dockery"])

    def test_new_recorded_music_examples_are_not_generic_instrument_labels(self):
        biographies = dict((cells[0], cells[1]) for cells in (
            line.split("\t") for line in
            (DATA / "jazz-researched-context.tsv").read_text("ascii").splitlines()))
        examples = {
            "hank-mobley": ("Soul Station", "Wynton Kelly"),
            "jeanne-lee": ("Escalator Over the Hill", "Marion Brown"),
            "john-gilmore": ("Blowing in from Chicago", "Sun Ra"),
            "john-mclaughlin": ("Bitches Brew", "Shakti"),
            "john-tchicai": ("Ascension", "New York Art Quartet"),
            "johnny-hammond-smith": ("Forever Taurus", "Milestone"),
            "johnny-mandel": ("Shadow of Your Smile", "Sinatra"),
            "kenny-burrell": ("Midnight Blue", "Coltrane"),
            "kenny-wheeler": ("Gnu High", "Jarrett"),
            "larry-willis": ("Lee Morgan", "Woody Shaw"),
            "larry-young": ("Unity", "Joe Henderson"),
            "marian-mcpartland": ("Piano Jazz", "Hickory House"),
            "paul-desmond": ("Take Five", "Brubeck"),
            "randy-brecker": ("Score", "Michael"),
            "woody-herman": ("Four Brothers", "Stan Getz"),
            "woody-shaw": ("Rosewood", "Larry Young"),
            "gene-krupa": ("Carnegie Hall", "Goodman"),
            "harold-mabern": ("Jazztet", "Lee Morgan"),
            "jo-jones": ("Count Basie", "Freddie Green"),
            "joe-pass": ("Virtuoso", "Ella Fitzgerald"),
            "lionel-hampton": ("Flying Home", "Jacquet"),
            "mary-osborne": ("Charlie Christian", "Coleman Hawkins"),
            "ramsey-lewis": ("The In Crowd", "Bohemian Caverns"),
            "thad-jones": ("Mel Lewis", "Village Vanguard"),
            "duke-ellington-and-his-orchestra": ("Take the A Train", "Victor"),
            "gast-waltzing": ("Atmosphere", "Conservatory"),
            "geta-burlacu": ("Ce n-as da", "Eurovision"),
            "nelson-veras": ("Label Bleu", "Magic Malik"),
            "nikele-moyake": ("Blue Notes", "Chris McGregor"),
            "the-horace-silver-quintet": ("6 Pieces of Silver", "Donald Byrd"),
            "the-oscar-peterson-trio": ("Herb Ellis", "Ray Brown"),
            "thomas-gansch": ("Mnozil Brass", "Vienna Art Orchestra"),
            "toquinho": ("Vinicius de Moraes", "Aquarela"),
            "uyama-hiroto": ("Nujabes", "A Son of the Sun"),
            "chris-bauer": ("In A Yuletide Groove", "harmonica"),
            "gerhard-laber": ("experimental", "percussionist"),
            "giorgi-vintilescu": ("Palais de Danse", "ragtime"),
            "michael-arlt": ("Red Holloway", "Paquito"),
            "michael-vatcher": ("John Zorn", "The Ex"),
            "pepe-berns": ("Malta Jazzfest", "Ben Monder"),
            "philipp-schaufelberger": ("Kenny Wheeler", "Dewey Redman"),
            "roland-neffe": ("Gary Burton", "Wood & Steel"),
            "al-green": ("Soul Mates", "Willie Mitchell"),
            "alan-greenspan": ("Henry Jerome", "Federal Reserve"),
            "michelle-dockery": ("The Watching Silence", "Michael Fox"),
            "phil-collins": ("Unorthodox Behaviour", "Percy Jones"),
            "viggo-mortensen": ("Buckethead", "1997"),
            "yoko-kanno": ("Seatbelts", "Cowboy Bebop"),
            "stan-kenton": ("Artistry in Rhythm", "Capitol"),
            "joe-lovano": ("Live at the Village Vanguard", "1995"),
            "eddie-lang": ("Lonnie Johnson", "1927 and 1929"),
            "lyle-mays": ("Pat Metheny Group", "Phase Dance"),
            "marcus-miller": ("Tutu", "Miles Davis"),
            "hugh-masekela": ("Grazing in the Grass", "Jazz Epistles"),
            "al-di-meola": ("Return to Forever", "Elegant Gypsy"),
            "charlie-haden": ("The Shape of Jazz to Come", "Carla Bley"),
            "manny-albam": ("Jazz Workshop", "1957"),
            "bill-chase": ("Get It On", "four-trumpet"),
            "earl-bostic": ("Flamingo", "John Coltrane"),
            "kenny-drew-jr": ("In Walked Bud", "Marian McPartland"),
            "billy-eckstine": ("1944", "Dizzy Gillespie"),
            "cab-calloway": ("Minnie the Moocher", "1931"),
            "rahsaan-roland-kirk": ("circular breathing", "Charles Mingus"),
            "tim-berne": ("Snakeoil", "Matt Mitchell"),
        }
        for key, details in examples.items():
            with self.subTest(artist=key):
                self.assertIn(key, biographies)
                for detail in details:
                    self.assertIn(detail, biographies[key])
                self.assertLessEqual(len(biographies[key]), 180)

if __name__ == "__main__":
    unittest.main()
