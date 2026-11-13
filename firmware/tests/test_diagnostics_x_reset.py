"""Diagnostics X resets *displayed* audio history, not active audio internals."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'PocketJazzRadio/DiagnosticAudioCounterWindow.h').read_text(encoding='utf-8')


class DiagnosticsResetContract(unittest.TestCase):
    def test_x_resets_pcm_and_underrun_window(self):
        handler = APP.split('if (screen == DIAGNOSTICS) {', 1)[1].split('if (screen == SETTINGS_BACKUP)', 1)[0]
        self.assertIn("if (pressed(k, 'x'))", handler)
        self.assertIn('diagAudioWindow.reset(audioUnderruns.load(), audioReopens.load(),', handler)
        self.assertIn('audioFrames.load());', handler)
        self.assertIn('audioPlayout.resetCounters();', handler)
        self.assertNotIn('audioUnderruns.store(0)', handler)
        self.assertNotIn('audioReopens.store(0)', handler)
        self.assertNotIn('sendAudio(', handler)

    def test_visible_and_exported_counters_use_identical_window(self):
        diag = APP.split('void drawDiagnostics(bool partial = false)', 1)[1].split('void drawSettingsBackup()', 1)[0]
        export = APP.split('void exportDiagnostics() {', 1)[1].split('void exportIcyHistory()', 1)[0]
        for name, raw in [('under', 'audioUnderruns'), ('opens', 'audioReopens'), ('frames', 'audioFrames')]:
            expr = f'diagAudioWindow.{name}({raw}.load())'
            self.assertIn(expr, diag)
            self.assertIn(expr, export)
        self.assertIn('audioUnderruns.fetch_add(1)', APP)
        self.assertIn('audioUnderruns.load() == priorUnderruns', APP)
        self.assertIn('audioUnderruns.load() != peekUnderflowsAtOpen', APP)

    def test_window_retains_raw_counter_integrity_and_wrap(self):
        self.assertIn('uint32_t under(uint32_t raw) const { return raw - underBase_; }', HEADER)
        self.assertIn('uint32_t opens(uint32_t raw) const { return raw - opensBase_; }', HEADER)
        self.assertIn('uint32_t frames(uint32_t raw) const { return raw - framesBase_; }', HEADER)

    def test_help_exposes_x_reset(self):
        self.assertIn('"X RESET COUNTERS"', APP)
        keys = (ROOT / 'docs/KEYS.md').read_text(encoding='utf-8')
        self.assertIn('`Under`', keys)
        self.assertIn('**X**', keys)


if __name__ == '__main__':
    unittest.main()
