"""Only one audio task measures existing source, decode and speaker operations.

Never create another connection or serialize/print in a real-time callback.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
BUF = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceBuffer.cpp').read_text(encoding='utf-8')
BUF_H = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceBuffer.h').read_text(encoding='utf-8')
OUT = (ROOT / 'PocketJazzRadio/AudioOutputM5Speaker.h').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests/pcm/CMakeLists.txt').read_text(encoding='utf-8')
TEST = (ROOT / 'tests/audio_hot_path_timing_test.cpp').read_text(encoding='utf-8')


class HotPathTimingIntegration(unittest.TestCase):
    def test_source_measurement_preserves_cooperative_reads(self):
        self.assertIn('SetSourcePollMonitor(source_poll_fn_t fn, void* arg)', BUF_H)
        self.assertIn('SetSourcePollMonitor(sourcePullTimingCB, nullptr)', APP)
        self.assertIn('audio_buffer::pull(src, data, len, cooperative)', BUF)
        self.assertEqual(3, BUF.count('pullSource(&buffer[writePtr],'))
        self.assertIn('pullSource(ptr, missing, cooperativeSourceRead)', BUF)

    def test_flush_and_decode_costs_measured_without_second_task(self):
        self.assertIn('setFlushTimingMonitor(pcmFlushTimingCB, nullptr)', APP)
        self.assertIn('_flush_timing(_flush_timing_arg, micros() - flushStarted)', OUT)
        self.assertIn('const uint32_t decodeLoopStarted = micros();', APP)
        self.assertIn('pocketjazz::elapsedMicros(decodeLoopStarted, micros())', APP)
        worker = APP.split('void audioWorker(void*) {', 1)[1].split('void loadPeekConfig() {', 1)[0]
        self.assertNotIn('Serial.printf(', worker)
        self.assertIn('buffer->SetCooperativeSourceRead(true)', worker)

    def test_audio_pipeline_page_five_and_x_reset_are_distinct_from_https_page_three(self):
        self.assertIn('} else if (diagPage == 4) {', APP)
        self.assertIn('String(diagPage + 1) + "/" + String(DIAG_PAGE_COUNT)', APP)
        self.assertIn('audioSourcePullMaxUs.store(0)', APP)
        self.assertIn('audioDecodeLoopMaxUs.store(0)', APP)
        self.assertIn('audioPcmFlushMaxUs.store(0)', APP)
        self.assertIn('"Network read max: "', APP)
        self.assertIn('"MP3 decode max: "', APP)
        self.assertIn('"Speaker write max: "', APP)
        # Eight-page diagnostics moved the UI gap to SYSTEM HEALTH (page 8),
        # without removing its measurement or X-reset behavior.
        system_health = APP.split('} else if (diagPage == 7) {', 1)[1].split(
            '\n  if (uiNoticeUntil', 1
        )[0]
        self.assertIn('"Screen max pause: " + String(uiLoopMaxGapMs.load())', system_health)
        self.assertIn('#undef NDEBUG', TEST)
        self.assertIn('audio_source_mp3_pcm_flush_timing_max_and_reset_wrap', CMAKE)


if __name__ == '__main__':
    unittest.main()
