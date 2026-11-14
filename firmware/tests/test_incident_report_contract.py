"""One-key, post-STOP, noninvasive comprehensive audio report."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")
DOC = (ROOT / "docs/SD-EXPORTS.md").read_text("utf-8")


class IncidentReportContract(unittest.TestCase):
    def test_one_key_on_any_diagnostics_page_only_after_stop(self):
        keys = APP.split("if (screen == DIAGNOSTICS) {", 1)[1].split(
            "if (screen == SETTINGS_BACKUP)", 1
        )[0]
        self.assertIn("if (pressed(k, 'r')) { exportIncidentReport(); return; }", keys)
        self.assertIn("if (pressed(k, 'e')) { if (diagPage == 7) exportAudioRuntime(); else exportDiagnostics(); return; }", keys)
        self.assertIn("if (pressed(k, 'm') && diagPage == 6) { exportHeapLifecycle(); return; }", keys)
        self.assertIn("if (pressed(k, 'x'))", keys)
        report = APP.split("void exportIncidentReport() {", 1)[1].split(
            "void exportIcyHistory() {", 1
        )[0]
        self.assertIn("audioState.load() != STOPPED || wantPlay", report)
        self.assertIn("appendUiExport(path, row", report)
        self.assertIn("SD.exists(path)", report)
        self.assertIn('"/PocketJazz/incident-%010lu-%02u.tsv"', report)
        self.assertIn('"END\\tcomplete=1\\n"', report)
        self.assertIn('showNotice("Report saved: incident-*.tsv")', report)
        self.assertIn("AudioRuntimeJournal", APP)

    def test_station_and_all_relevant_diagnostic_fields_and_history(self):
        report = APP.split("void exportIncidentReport() {", 1)[1].split(
            "void exportIcyHistory() {", 1
        )[0]
        for label in ("BEGIN", "STATION", "WIFI", "HTTPS", "STREAM",
                      "DECODE", "PCM", "MEMORY", "EVENT", "HEAP", "END"):
            self.assertIn(f'"{label}\\t', report, label)
        for important in ("POCKETJAZZ_FIRMWARE_VERSION", "stations[idx].name",
                          "currentTitle", "cleanTsvField", "audioBufferedBytes.load()",
                          "audioBufferMinimum.load()", "audioUnderruns.load()",
                          "audioRingCapacity.load()", "audioReopens.load()",
                          "audioSourcePullMaxUs.load()", "audioDecodeLoopMaxUs.load()",
                          "audioPcmFlushMaxUs.load()", "audioTlsErrorCode.load()",
                          "pcm.pcmGapEpisodes", "pcm.pcmGapMaxMs", "audioJournal.newest",
                          "heapLifecycle.firstStallEver", "point.sourceIdleMs",
                          "heap_caps_get_largest_free_block", "WiFi.RSSI()"):
            self.assertIn(important, report, important)
        self.assertNotIn("WiFi.begin(", report)
        self.assertNotIn("http.GET(", report)
        self.assertNotIn("NVS", report)
        for secret in ("savedPassword", "wifiPassword", "savedSsid", "wifiSsid",
                       "stations[idx].url", "lastSelectedUrl", "WiFi.localIP()"):
            self.assertNotIn(secret, report, secret)
        self.assertIn("R", DOC)
        self.assertIn("incident-", DOC)

    def test_no_background_writes(self):
        worker = APP.split("void audioWorker(void*) {", 1)[1].split(
            "void loadPeekConfig()", 1
        )[0]
        self.assertNotIn("exportIncidentReport(", worker)
        self.assertNotIn("appendUiExport(", worker)
        self.assertIn("R REPORT (STOP)", APP)
        self.assertIn("R report(STOP)", APP)


if __name__ == "__main__":
    unittest.main()
