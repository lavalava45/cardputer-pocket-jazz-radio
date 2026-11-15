"""Source contracts: new learning UI is offline, bounded and user-directed.

These are not firmware compile tests or physical SD/audio acceptance.
"""
import re
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
from build_now_playing_hints import render_header
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
HEADER = (ROOT / "PocketJazzRadio/JazzJourney.h").read_text("utf-8")
FACTS = (ROOT / "PocketJazzRadio/JazzFacts.h").read_text("utf-8")
KEYS = APP.split("void handleKeys(const Press& k) {", 1)[1].split(
    "void serviceDailyRadioAlarm(", 1
)[0]


def block(a, b):
    return APP.split(a, 1)[1].split(b, 1)[0]


class JazzJourneyIntegration(unittest.TestCase):
    def test_e_entry_hub_and_six_distinct_screens(self):
        self.assertIn("if (pressed(k, 'e'))", KEYS)
        self.assertIn("screen = JAZZ_JOURNEY", KEYS)
        for screen in ("JAZZ_JOURNEY", "JAZZ_TRAIL", "JAZZ_LESSON",
                       "JAZZ_QUIZ", "JAZZ_LISTEN", "JAZZ_PASSPORT", "JAZZ_DIARY"):
            self.assertIn(f"if (screen == {screen})", KEYS)
            self.assertIn(f"case {screen}:", APP)
        for hook in ("drawJazzJourney()", "drawJazzTrails()", "drawJazzLesson()",
                     "drawJazzQuiz()", "drawJazzListen()", "drawJazzPassport()",
                     "drawJazzDiary()"):
            self.assertIn(hook, APP)
        self.assertIn("UP/DOWN ENTER  P SAVE(STOP) DEL", APP)

    def test_course_lesson_links_notes_spark_and_station_browse(self):
        lesson = KEYS.split("if (screen == JAZZ_LESSON) {", 1)[1].split(
            "if (screen == JAZZ_QUIZ)", 1
        )[0]
        self.assertIn("openJourneyArtist()", lesson)
        self.assertIn("openJourneySpark()", lesson)
        self.assertIn("browseReturnScreen = JAZZ_LESSON", lesson)
        self.assertIn("journeyProgress.markHeard(journeyLessonId)", lesson)
        self.assertIn("journeyProgress.markLearned(journeyLessonId)", KEYS)
        self.assertIn("if (answer == journeyLesson.correct)", KEYS)
        self.assertIn("db.find(journeyLesson.artist, artist, index,", APP)
        self.assertIn('"/PocketJazz/jazz-encyclopedia.idx"', APP)
        self.assertIn("db.findSubject(journeyLesson.sparkTopic", APP)
        self.assertIn("bool findSubject(const char* subject", FACTS)
        self.assertIn("notesOpenedFromJourney ? JAZZ_LESSON : RADIO", KEYS)
        self.assertIn("screen = sparkReturnScreen", KEYS)
        self.assertIn("N NOTE  E JOURNEY", APP)
        self.assertIn("if (pressed(k, 't'))", lesson)
        self.assertIn("journeyLessonListenView = !journeyLessonListenView", lesson)
        draw=block("void drawJazzLesson() {", "void drawJazzQuiz() {")
        self.assertIn("drawWrappedRadioText(journeyLesson.listen, 52, C_WHITE, 5)", draw)
        self.assertIn("drawWrappedRadioText(journeyLesson.story, 40, C_WHITE, 4)", draw)

    def test_now_playing_has_immediate_curated_caption_no_sd_in_repaint(self):
        body=block("void drawPlayer(bool partial", "void drawNight(bool partial")
        self.assertIn("JazzNowPlaying::hint(artist)", body)
        self.assertIn("drawWrappedRadioText(preview, 60", body)
        self.assertNotIn("SD.open(", body)
        self.assertNotIn("WiFi.begin(", body)
        hints=(ROOT/"PocketJazzRadio/JazzNowPlaying.h").read_text("utf-8")
        for name in ("Louis Armstrong", "Duke Ellington", "Miles Davis",
                     "John Coltrane", "Art Blakey", "Ella Fitzgerald"):
            self.assertIn(name, hints)
        self.assertIn("return !*a && !*b", hints)
        # Each flash caption must point to a genuinely curated, non-generic
        # biography. This never silently upgrades a one-word nationality.
        import re
        import json
        biographies = {
            p[0]: p[2] for line in
            (ROOT/"data/jazz-encyclopedia.tsv").read_text("ascii").splitlines()
            if len(p := line.split("\t")) == 5
        }
        captions = [
            (json.loads(name), json.loads(description))
            for name, description in re.findall(
                r'\{("(?:\\.|[^"\\])*"), ("(?:\\.|[^"\\])*")\}', hints)
        ]
        self.assertEqual(len(captions), 595)
        for artist, description in captions:
            key = re.sub(r"[^a-z0-9]+", "-", artist.lower()).strip("-")
            self.assertIn(key, biographies)
            self.assertGreaterEqual(len(biographies[key]), 95)
            self.assertLessEqual(len(description), 36)
        self.assertIn("kCuratedCount = 595", hints)
        # Strictly read-only: a drift in the checked-in C++ header is a test
        # failure, not a reason to rewrite production files during discovery.
        before = (ROOT/"PocketJazzRadio/JazzNowPlaying.h").read_bytes()
        expected = render_header().encode("ascii")
        self.assertEqual(before, expected)

    def test_no_hallucinated_audio_or_auto_station_control(self):
        # Training data is only keyed to a reference artist and a source;
        # browsing remains an explicit action, never an automatic station tune.
        path = block("void openJourneySpark() {", "void pageOfflineNotes(")
        self.assertNotIn("sendAudio(", path)
        self.assertNotIn("WiFi.begin(", path)
        self.assertIn("browseSelected = currentOffset", KEYS)
        self.assertIn("if (pressed(k, 'f'))", KEYS)
        self.assertIn("user explicitly saves",
                      (ROOT/"data/JAZZ-JOURNEY-PROVENANCE.md").read_text("utf-8").lower())

    def test_progress_manual_stop_only_no_nvs_or_audio_worker_writes(self):
        save = block("void saveJazzJourney() {", "void queueJazzDiary() {")
        self.assertIn("wantPlay || audioState.load() != STOPPED", save)
        self.assertIn("appendUiExport(", save)
        self.assertIn('"/PocketJazz/jazz-progress.tsv"', save)
        self.assertIn('"/PocketJazz/jazz-diary.tsv"', save)
        self.assertIn("journeyDiaryPendingCount", save)
        self.assertIn("65536", save)
        self.assertNotIn("Preferences", save)
        self.assertNotIn("sendAudio(", save)
        worker = block("void audioWorker(void*) {", "void loadPeekConfig()")
        self.assertNotIn("saveJazzJourney(", worker)
        self.assertNotIn("queueJazzDiary(", worker)
        self.assertNotIn("JazzJourneyCatalog", worker)
        self.assertIn("JazzJourneyProgress", HEADER)

    def test_all_course_screens_acknowledge_then_expire_sd_failure_toasts(self):
        draw=block("void drawScreen() {", "uint32_t uiSignature(uint32_t now) {")
        self.assertIn("screen >= JAZZ_JOURNEY && screen <= JAZZ_DIARY", draw)
        self.assertIn("shortName(uiNotice, 36)", draw)
        for screen in ("JAZZ_JOURNEY", "JAZZ_TRAIL", "JAZZ_LESSON",
                       "JAZZ_QUIZ", "JAZZ_LISTEN", "JAZZ_PASSPORT",
                       "JAZZ_DIARY"):
            sig=APP.split("case "+screen+":", 2)[-1]
            self.assertTrue("uiNoticeUntil" in sig)

    def test_diary_original_k_behavior_not_regressed(self):
        self.assertIn("if (pressed(k, 'k')) { likeCurrentTrack(); return; }", KEYS)
        self.assertIn("if (pressed(k, 'k') && k.fn)", KEYS)
        self.assertIn("if (k.enter) queueJazzDiary();", KEYS)
        self.assertIn("const ExportWriteStatus result = appendUiExport(\"/PocketJazz/liked.txt\"", APP)
        self.assertNotIn("stations[idx].url", block("void queueJazzDiary() {", "void openJourneyArtist()"))
        self.assertIn("cleanTsvField", block("void queueJazzDiary() {", "void openJourneyArtist()"))
        self.assertIn("if (!currentTitle[0])", block(
            "void queueJazzDiary() {", "void openJourneyArtist()"))

    def test_install_safeguards_for_new_sd_data(self):
        pack=(ROOT/"tools/build.ps1").read_text("utf-8")
        self.assertIn("'jazz-journey.tsv', 'jazz-listen.tsv'", pack)
        self.assertIn("Do NOT overwrite your personal /PocketJazz/04-my-stations.m3u", pack)
        self.assertIn('No new NVS keys', APP)
        self.assertIn("kLessons = 24", HEADER)
        self.assertIn("kTips = 30", HEADER)


if __name__ == "__main__":
    unittest.main()
