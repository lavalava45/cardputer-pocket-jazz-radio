"""Host-only regression contracts for Jazz Journey UI and SD failure scenarios.

These checks inspect the shipping application; hardware streaming and display
rendering still require device acceptance.
"""
import unittest
from pathlib import Path

APP = (Path(__file__).resolve().parents[1] / "PocketJazzRadio" /
       "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


def between(first, last):
    return APP.split(first, 1)[1].split(last, 1)[0]


class JourneyScenarios(unittest.TestCase):
    def test_corrupt_tail_never_publishes_partial_passport(self):
        block = between("void loadJourneyProgress(bool beforeAudioStartup) {", "void saveJazzJourney() {")
        self.assertIn("pocketjazz::JazzJourneyProgress recovered{}", block)
        self.assertIn("recovered = candidate", block)
        self.assertLess(block.index("if (!cleanTail)"),
                        block.index("journeyProgress = recovered"))
        self.assertIn("SD.exists(\"/PocketJazz/jazz-progress.tsv\")", block)
        self.assertIn("Passport SD read failed", block)
        self.assertIn("file.size() == 0", block)
        self.assertIn("(wantPlay && !beforeAudioStartup)", block)
        self.assertIn("audioState.load() != STOPPED", block)

    def test_autoplay_boot_restores_passport_before_audio_worker(self):
        setup = APP.split("void setup() {", 1)[1].split("void loop() {", 1)[0]
        self.assertIn("loadJourneyProgress(true);", setup)
        self.assertEqual(setup.count("loadJourneyProgress(true);"), 1)
        self.assertLess(setup.index("loadJourneyProgress(true);"),
                        setup.index("xTaskCreatePinnedToCore(audioWorker"))
        # Later saves never bypass STOP, even when autoplay was enabled.
        save = between("void saveJazzJourney() {", "void queueJazzDiary() {")
        self.assertIn("!journeyProgressLoaded) loadJourneyProgress()", save)
        self.assertNotIn("loadJourneyProgress(true)", save)

    def test_diary_needs_current_playing_radio(self):
        block = between("void queueJazzDiary() {", "void openJourneyArtist() {")
        self.assertIn("audioState.load() != PLAYING", block)
        self.assertIn("if (!currentTitle[0])", block)
        self.assertIn("journeyDiaryPendingCount >= 4", block)
        self.assertIn("cleanTsvField", block)
        save = between("void saveJazzJourney() {", "void queueJazzDiary() {")
        self.assertIn("journeyProgress.dirty = false", save)
        self.assertIn("if (status != EXPORT_WRITTEN)", save)

    def test_icy_metadata_is_session_scoped(self):
        callback = between("void icyMetadataCB(", "template <size_t N> void put")
        service = between("void serviceMetadata() {", "String normalizeArtistKey(")
        self.assertIn("event.intentSequence = audioAcceptedSequence", callback)
        self.assertIn("incoming.intentSequence != telemetryNextSequence", service)
        self.assertIn("audioState.load() != PLAYING", service)
        send = between("void sendAudio() {", "void markPrefs() {")
        self.assertIn("if (newIntent)", send)
        self.assertIn("currentTitle[0] = 0", send)

    def test_diary_return_respects_entry_point(self):
        keys = between("void handleKeys(const Press& k) {", "void serviceDailyRadioAlarm(")
        self.assertIn("journeyDiaryReturnScreen = JAZZ_JOURNEY", keys)
        self.assertIn("journeyDiaryReturnScreen = RADIO", keys)
        self.assertIn("screen = journeyDiaryReturnScreen", keys)

    def test_biography_one_screen_and_bounded(self):
        draw = between("void drawNotes() {", "void drawNotesSearch() {")
        self.assertIn("layoutNotes(notesDetail.c_str()", draw)
        self.assertIn("(hasAlbum ? 49 : 38) + i * 10", draw)
        self.assertIn('"DOWN MORE"', draw)
        self.assertIn("Record: ", draw)
        self.assertNotIn("notesDetailPage * 4", draw)
        self.assertIn('"ENTER/DEL back"', draw)

    def test_notes_spark_browse_navigation_explicit(self):
        keys = between("void handleKeys(const Press& k) {", "void serviceDailyRadioAlarm(")
        self.assertIn("notesOpenedFromJourney ? JAZZ_LESSON : RADIO", keys)
        self.assertIn("screen = sparkReturnScreen", keys)
        self.assertIn("screen = browseReturnScreen", keys)
        self.assertIn("browseReturnScreen = JAZZ_LESSON", keys)
        self.assertIn("Already marked HEARD", keys)

    def test_radio_notes_search_and_spark_respect_stream_buffer(self):
        for start, end in (
            ("void loadArtistNotes() {", "bool readNotesFile("),
            ("void pageOfflineNotes(size_t offset) {", "void searchOfflineNotes() {"),
            ("void showJazzSpark(int8_t direction) {", "void serviceJazzSpark("),
        ):
            body = between(start, end)
            self.assertLess(body.index("if (!canReadJourneyData()) return;"),
                            body.index("SD.open("))


if __name__ == "__main__":
    unittest.main()
