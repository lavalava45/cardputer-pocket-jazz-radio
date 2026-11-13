"""UI contract: signal diagnostics consolidated; S safe-boot remains intact."""
from pathlib import Path
import unittest

APP = (Path(__file__).resolve().parents[1] / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


class SignalScreenRemoved(unittest.TestCase):
    def test_signal_page_and_hotkey_are_removed_but_safe_boot_remains(self):
        self.assertNotIn("SIGNAL,", APP)
        self.assertNotIn("drawSignal(", APP)
        self.assertNotIn('screen = SIGNAL', APP)
        self.assertNotIn('"S SIGNAL/BUFFER"', APP)
        self.assertIn("Keyboard.isKeyPressed('s')", APP)
        self.assertIn("safeMode =", APP)

    def test_data_is_moved_to_existing_diagnostics_pages(self):
        self.assertIn('"Signal strength: "', APP)
        self.assertIn('String(audioRingCapacity.load())', APP)
        self.assertIn('"MP3 bitrate: "', APP)
        self.assertIn('decodedMp3ObservedVbr.load', APP)
        self.assertIn('"Battery voltage: "', APP)
        self.assertIn('"Sample rate: "', APP)


if __name__ == '__main__':
    unittest.main()
