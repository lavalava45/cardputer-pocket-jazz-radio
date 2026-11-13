"""Offline source guards; never build, contact a network, or create releases.

These are structural regression tests, not hardware TLS or audio integration tests.
"""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp"


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing {signature}")
    left = source.find("{", start)
    if left < 0:
        raise AssertionError(f"Missing body for {signature}")
    level = 0
    for i in range(left, len(source)):
        level += (source[i] == "{") - (source[i] == "}")
        if level == 0:
            return source[left + 1:i]
    raise AssertionError(f"Unclosed body for {signature}")


class SourceSafetyContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = APP.read_text(encoding="utf-8")

    def test_https_does_not_silently_fall_back_to_unauthenticated_http(self):
        """HTTPS sources must be handled by an explicitly secure implementation."""
        src = self.app
        secure_sources = list((ROOT / "PocketJazzRadio").glob("Secure*"))
        combined = src + "\n" + "\n".join(
            p.read_text(encoding="utf-8", errors="replace")
            for p in secure_sources if p.is_file() and p.suffix in {".cpp", ".h"}
        )
        self.assertNotRegex(combined, r"\bsetInsecure\s*\(", "TLS certificate validation must not be disabled")
        self.assertNotRegex(combined, r'\.replace\s*\(\s*"https://"\s*,\s*"http://"', "HTTPS downgrade")
        worker = function_body(src, "void audioWorker(void*)")
        self.assertRegex(worker, r"\b(?:new\s*(?:\([^)]*\))?\s*SecureICYSource|SecureICYSource\s*\()",
                         "audioWorker must actually construct and route to secure HTTPS source")
        self.assertRegex(combined, r"(?i)(?:setCACert|setTrustAnchors|esp_crt_bundle_attach|cert_bundle_attach)",
                         "Require CA trust material or a system certificate bundle for HTTPS")
        self.assertRegex(combined, r"(?i)(?:redirect|location)", "Review redirect handling including HTTPS-to-HTTP rejection")

    def test_audio_failure_stage_and_source_code_reset_on_each_open_attempt(self):
        worker = function_body(self.app, "void audioWorker(void*)")
        open_pos = worker.index("audioState.store(CONNECTING)")
        source_open_match = re.search(r"\bsrc\s*->\s*open\s*\(\s*(?:url|endpoint)\s*\)", worker[open_pos:])
        self.assertIsNotNone(source_open_match, "Missing stream open call using url or failover endpoint")
        source_open = open_pos + source_open_match.start()
        portion = worker[open_pos:source_open]
        self.assertIn("audioFailureStage.store(0)", portion,
                      "Clear stale failure attribution before every connection retry")
        self.assertIn("audioSourceStatusCode.store(0)", portion,
                      "Clear stale ICY/HTTP status before every connection retry")

    def test_decoder_failure_codes_match_diagnostics_contract(self):
        self.assertRegex(self.app, r"audioFailureStage\.store\(1\)")
        self.assertRegex(self.app, r"audioFailureStage\.store\(2\)")
        self.assertIn("audioFailureStage.store(codec.ready() ? 3 : 6)", self.app)
        self.assertRegex(self.app, r"audioFailureStage\.store\(4\)")
        diagnostics = function_body(self.app, "void drawDiagnostics(bool partial = false)")
        self.assertIn("audioFailureStage.load()", diagnostics)
        self.assertIn("audioSourceStatusCode.load()", diagnostics)

    def test_keyboard_help_has_open_close_and_visible_key_bindings(self):
        handle = function_body(self.app, "void handleKeys(const Press& k)")
        help_screen = function_body(self.app, "void drawHelp()")
        self.assertRegex(handle, r"pressed\(k,\s*'h'\).*screen\s*=\s*HELP")
        self.assertRegex(handle, r"screen\s*==\s*HELP[\s\S]*?if\s*\(k\.del\)\s*screen\s*=\s*RADIO")
        for key in ("UP/DOWN", "- / =", "ENTER", "P", "W", "R", "I", "FN+1-9", "M", "O", "T", "L"):
            with self.subTest(key=key):
                self.assertIn(key, help_screen)
        self.assertIn("UP/DOWN PAGES", help_screen, "Multi-page help must explain navigation")
        self.assertIn("helpPage", handle, "Help pages must respond to keyboard navigation")

    def test_icy_track_data_crosses_tasks_by_copied_queue(self):
        self.assertIn("TrackMetadata", self.app)
        self.assertIn("RegisterMetadataCB(icyMetadataCB", self.app)
        self.assertIn("xQueueOverwrite(metadataQueue, &event)", self.app)
        self.assertIn("xQueueReceive(metadataQueue, &incoming, 0)", self.app)
        self.assertIn("incoming.stationId != stationIndex()", self.app)

    def test_pcm_vu_uses_monitor_without_ui_work_in_audio_task(self):
        callback = function_body(self.app, "void pcmMonitor(void* sink, const int16_t* stereo, size_t frames)")
        self.assertIn("pcmPeak.store", callback)
        self.assertNotRegex(callback, r"\b(?:malloc|new|Display|draw|fillScreen)\b")
        self.assertIn("output.setMonitor(pcmMonitor", self.app)
        self.assertIn("void serviceVu(uint32_t now)", self.app)
        self.assertIn("case CLOCK: drawNight(); break;", self.app)


if __name__ == "__main__":
    unittest.main()
