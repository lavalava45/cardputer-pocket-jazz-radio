"""Contracts for the Cardputer ADV UX batch after v0.7z14.

These source checks are complementary to physical key/audio acceptance.
They do not simulate real keyboard matrix events or SD write latency.
"""
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
FX = (ROOT / "PocketJazzRadio/TuningSoundBed.h").read_text("utf-8")
FACTS = (ROOT / "PocketJazzRadio/JazzFacts.h").read_text("utf-8")


def code_between(start, end):
    return APP.split(start, 1)[1].split(end, 1)[0]


class FiveScreenFixes(unittest.TestCase):
    def test_jazz_notes_search_and_card_enter_are_both_reachable(self):
        controls = code_between("void handleKeys(const Press& k) {", "void serviceSleepTimer(")
        search = controls.split("if (screen == NOTES_SEARCH) {", 1)[1].split("if (screen == SPARK)", 1)[0]
        detail = controls.split("if (screen == NOTES) {", 1)[1].split("if (screen == NOTES_SEARCH)", 1)[0]
        self.assertIn("else if (!k.fn && !k.enter)", search)
        self.assertIn("else if (notesSearchCount) openSelectedOfflineNote();", search)
        self.assertIn("if (k.del || k.enter)", detail)

    def test_icy_selection_and_full_title_without_sd_writes(self):
        control = code_between("if (screen == TRACKS) {", "if (screen == SHARE) {")
        screen = code_between("void drawTracks() {", "void drawBrowse() {")
        self.assertIn("k.up && !icyDetailOpen", control)
        self.assertIn("k.down && !icyDetailOpen", control)
        self.assertIn("k.enter && trackHistory.size()", control)
        self.assertIn("icyDetailOpen = !icyDetailOpen", control)
        self.assertIn("if (icyDetailOpen) icyDetailOpen = false", control)
        self.assertIn("trackHistory.newest(chosen)", screen)
        self.assertIn("pocketjazz::utf8SafePrefix(", screen)
        self.assertIn("UP/DOWN ENTER full", screen)
        self.assertIn("mix(icySelected)", APP)
        self.assertIn("mix(icyDetailOpen)", APP)
        self.assertNotIn("SD.open(", screen)
        self.assertNotIn("SD.open(", control)
        self.assertIn("TrackHistory<5> trackHistory", APP)
        self.assertNotIn("TrackHistory<25> trackHistory", APP)

    def test_help_resumes_last_page_without_nvs_writes(self):
        controls = code_between("void handleKeys(const Press& k) {", "void serviceSleepTimer(")
        self.assertIn("if (pressed(k, 'h')) { screen = HELP; return; }", controls)
        self.assertNotIn("helpPage = 0; screen = HELP", controls)
        self.assertIn("helpPage = (helpPage + HELP_PAGE_COUNT - 1) % HELP_PAGE_COUNT", controls)
        self.assertIn("helpPage = (helpPage + 1) % HELP_PAGE_COUNT", controls)
        self.assertIn("case HELP:", APP)
        self.assertIn("mix(helpPage)", APP)

    def test_white_noise_bound_to_master_without_pcm_hotpath_processing(self):
        worker = code_between("void audioWorker(void*) {", "void loadPeekConfig()")
        self.assertIn("kQuietChannelVolume = 8", FX)
        self.assertIn("channelVolumeForMaster(uint8_t master)", FX)
        self.assertIn("channelVolumeForMaster(speakerVolume)", worker)
        self.assertIn("requestedOutputVolume.load(std::memory_order_acquire)", worker)
        self.assertIn("setChannelVolume(pocketjazz::tuning::kChannel", worker)
        self.assertIn("M5.Speaker.setVolume(speakerVolume)", worker)
        self.assertNotIn("malloc(", FX)
        self.assertNotIn("SD.open(", FX)

    def test_spark_reads_sd_on_demand_and_randomizes_all_quotes(self):
        self.assertIn("size_t quoteCount() const", FACTS)
        self.assertIn("bool quoteAt(size_t ordinal", FACTS)
        self.assertIn("size_t quoteCount_ = 0", FACTS)
        self.assertIn("walk(-1, tmp, count, &quoteCount_)", FACTS)
        self.assertNotIn("new ", FACTS)
        self.assertNotIn("malloc(", FACTS)
        spark = code_between("void showJazzSpark(int8_t direction) {", "void serviceJazzSpark(")
        self.assertIn("FILE_READ", spark)
        self.assertIn("corpus.quoteCount()", spark)
        self.assertIn("corpus.quoteAt(", spark)
        self.assertNotIn("jazzSparkTotal - 5", spark)
        keys = code_between("if (screen == SPARK) {", "if (screen == TRACKS) {")
        self.assertIn("k.down || k.enter", keys)


if __name__ == "__main__":
    unittest.main()
