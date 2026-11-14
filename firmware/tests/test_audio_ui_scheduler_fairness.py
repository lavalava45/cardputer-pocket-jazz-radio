"""Guard against the continuous-MP3 UI starvation reported on Swiss Jazz.

This verifies FreeRTOS task configuration in the source, NOT actual physical
keyboard scan timing or acoustic continuity on Cardputer ADV hardware.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")


class AudioUiSchedulingPolicy(unittest.TestCase):
    def test_audio_task_shares_loop_task_priority_and_core(self):
        # Arduino ESP32 3.3.9 creates loopTask at priority 1; our supported
        # board's LoopCore defaults to core 1.
        self.assertRegex(
            APP,
            r'xTaskCreatePinnedToCore\(audioWorker,\s*"pocketjazz_audio",\s*'
            r'8192,\s*nullptr,\s*1,\s*nullptr,\s*1\)',
        )

    def test_stream_open_cannot_raise_decoder_above_keyboard(self):
        worker = APP.split("void audioWorker(void*) {", 1)[1].split(
            "void loadPeekConfig()", 1
        )[0]
        self.assertNotIn("vTaskPrioritySet(nullptr, 2)", worker)
        self.assertNotIn("vTaskPrioritySet(nullptr, 1)", worker)
        self.assertNotIn("portMAX_DELAY", worker)
        self.assertIn("audioState.store(PLAYING)", worker)
        self.assertRegex(worker, r'vTaskDelay\(1\);\s*\}\s*\}\s*')

    def test_ui_remains_on_normal_arduino_loop_with_keyboard_first(self):
        setup = APP.split("void setup() {", 1)[1].split("void loop() {", 1)[0]
        loop = APP.split("void loop() {", 1)[1]
        self.assertIn('M5Cardputer.begin(cfg, true)', setup)
        self.assertLess(loop.index("M5Cardputer.update();"),
                        loop.index("handleKeys(input)"))
        self.assertLess(loop.index("handleKeys(input)"),
                        loop.index("serviceVu(now)"))
        self.assertIn("delay(12);", loop)
        self.assertIn("uiLoopMaxGapMs.load()", APP)


if __name__ == "__main__":
    unittest.main()
