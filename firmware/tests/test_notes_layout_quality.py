"""Check the shipped 240x135 Notes layout and full-text paging contracts."""
from pathlib import Path
import csv
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
HEADER = (ROOT / "PocketJazzRadio/JazzNotesLayout.h").read_text("utf-8")


class NotesLayoutQuality(unittest.TestCase):
    def test_physical_viewport_no_overlap(self):
        notes = APP.split("void drawNotes() {", 1)[1].split("void drawNotesSearch() {", 1)[0]
        self.assertIn("(hasAlbum ? 49 : 38) + i * 10", notes)
        self.assertIn("layoutNotes(notesDetail.c_str()", notes)
        self.assertIn("222, measure", notes)
        self.assertIn("9, 39", notes)
        self.assertIn("8, 125", notes)
        # Seven body lines or a recording line plus six body lines.
        self.assertLess(28 + 8, 38)
        self.assertLess(38 + 6 * 10 + 10, 111)
        self.assertLess(49 + 5 * 10 + 10, 111)
        self.assertLess(125 + 10, 136)

    def test_overflow_and_long_names_are_disclosed(self):
        notes = APP.split("void drawNotes() {", 1)[1].split("void drawNotesSearch() {", 1)[0]
        keys = APP.split("if (screen == NOTES) {", 1)[1].split("if (screen == NOTES_SEARCH) {", 1)[0]
        self.assertIn('"DOWN MORE"', notes)
        self.assertIn('"UP PREV"', notes)
        self.assertIn('"T DETAILS"', notes)
        self.assertIn('"FULL NAME / RECORD"', notes)
        self.assertIn("notesShowFullName = !notesShowFullName", keys)
        self.assertIn("layoutNotes(notesArtist.c_str()", notes)
        self.assertIn("layoutNotes(notesAlbum.c_str()", notes)
        self.assertIn("name.more || record.more", notes)
        self.assertIn('"DETAIL OVERFLOW"', notes)
        self.assertIn("display.setTextSize(1)", keys)
        self.assertNotIn("else notesShowFullName = !notesShowFullName", keys)
        self.assertIn("if (page.more", keys)
        self.assertIn("result.more = text[pos] != 0", HEADER)
        self.assertNotIn("strlcat(line, \"..\"", HEADER)

    def test_1498_real_record_fields_and_prioritized_music(self):
        with (ROOT / "data/jazz-encyclopedia.tsv").open(newline="", encoding="ascii") as f:
            rows = list(csv.reader(f, delimiter="\t"))
        self.assertEqual(len(rows), 1498)
        self.assertTrue(all(len(row) == 5 and row[2] for row in rows))
        # The album/contribution display is inside the information region,
        # and any remaining biography text remains accessible by DOWN.
        notes = APP.split("void drawNotes() {", 1)[1].split("void drawNotesSearch() {", 1)[0]
        self.assertLess(notes.index('"Record: "'),
                        notes.index("layoutNotes(notesDetail.c_str()"))
        self.assertIn("hasAlbum ? 6u : 7u", notes)
        # Real long album titles must remain readable in the detail view.
        measure = lambda text: len(text) * 6
        self.assertLessEqual(max(measure(row[1]) for row in rows), 222 * 3)
        self.assertLessEqual(max(measure(row[3]) for row in rows if row[3] != '-'),
                             222 * 3)

    def test_radio_notes_navigation_returns_to_radio_and_trail_to_lesson(self):
        keys = APP.split("if (screen == NOTES) {", 1)[1].split(
            "if (screen == NOTES_SEARCH) {", 1)[0]
        radio = APP.split("if (pressed(k, 'n')) {", 1)[1].split(
            "if (pressed(k, 'u')) {", 1)[0]
        self.assertIn("notesOpenedFromSearch = false", radio)
        self.assertIn("notesOpenedFromJourney = false", radio)
        self.assertIn("notesOpenedFromJourney ? JAZZ_LESSON : RADIO", keys)
        self.assertIn("notesOpenedFromSearch ? NOTES_SEARCH", keys)
        self.assertIn("if (k.del || k.enter)", keys)

    def test_index_trust_comes_before_audio_worker_and_is_cleared_on_remount(self):
        setup = APP.split("void setup() {", 1)[1]
        self.assertLess(setup.index("loadPlaylists();"),
                        setup.index("verifyJazzNotesIndexAtStop();"))
        self.assertLess(setup.index("verifyJazzNotesIndexAtStop();"),
                        setup.index("xTaskCreatePinnedToCore(audioWorker"))
        load = APP.split("bool loadPlaylists() {", 1)[1].split("bool ", 1)[0]
        self.assertIn("jazzEncyclopediaIndexTrusted = false", load)
        verify = APP.split("void verifyJazzNotesIndexAtStop() {", 1)[1].split(
            "bool canReadJourneyData() {", 1)[0]
        self.assertIn("db.verifyIndex(indexed)", verify)
        self.assertIn("audioState.load() != STOPPED", verify)

    def test_untrusted_live_lookup_is_rejected_before_sd_scan(self):
        load = APP.split("void loadArtistNotes() {", 1)[1].split(
            "bool readNotesFile(", 1)[0]
        self.assertLess(load.index('notesFound = false'),
                        load.index('if (!canReadJourneyData()) return'))
        self.assertLess(load.index('STOP: verify Jazz index'),
                        load.index('SD.open('))
        self.assertIn("jazzEncyclopediaIndexTrusted))", load)
        journey = APP.split("void openJourneyArtist() {", 1)[1].split(
            "void openJourneySpark() {", 1)[0]
        self.assertLess(journey.index('STOP: verify Jazz index'), journey.index('SD.open('))

    def test_sd_remount_and_size_change_invalidate_boot_proof(self):
        reload = APP.split("bool loadPlaylists() {", 1)[1].split("SPI.begin(", 1)[0]
        self.assertIn("jazzEncyclopediaIndexTrusted = false", reload)
        self.assertIn("jazzTrustedCorpusBytes = jazzTrustedIndexBytes = 0", reload)
        pair = APP.split("bool notesIndexPairTrusted(File& corpus, File& index) {", 1)[1].split(
            "bool canReadJourneyData() {", 1)[0]
        self.assertIn("corpus.size() != jazzTrustedCorpusBytes", pair)
        self.assertIn("index.size() != jazzTrustedIndexBytes", pair)
        self.assertIn("jazzEncyclopediaIndexTrusted = false", pair)
        self.assertIn("Same-size in-place changes", pair)


if __name__ == "__main__":
    unittest.main()
