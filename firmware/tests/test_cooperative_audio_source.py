"""Contracts for the shared HTTPS/ICY live-audio nonblocking read class."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
LIB = ROOT / "arduino-sketchbook/libraries/ESP8266Audio/src"
SOURCE = (LIB / "AudioFileSourceBuffer.cpp").read_text(encoding="utf-8")
HEADER = (LIB / "AudioFileSourceBuffer.h").read_text(encoding="utf-8")
TLS = (ROOT / "PocketJazzRadio" / "SecureICYSource.cpp").read_text(encoding="utf-8")


class CooperativeAudioSourceTests(unittest.TestCase):
    def test_app_enables_only_one_decoder_owner_cooperative_buffer(self):
        self.assertIn("buffer->SetCooperativeSourceRead(true)", APP)
        self.assertIn("decoder->SetLiveStallTimeoutMs(4000)", APP)
        self.assertIn("constexpr int STREAM_BYTES = 32768", APP)
        self.assertIn("hasPlayablePrefill(buffer->getFillLevel(), 512)", APP)
        self.assertIn("SetCooperativeSourceRead(bool enabled)", HEADER)
        self.assertIn("bool cooperativeSourceRead = false", HEADER)
        self.assertNotIn("setInsecure(", APP + TLS)
        self.assertNotIn("xTaskCreatePinnedToCore(audioWorker2", APP)

    def test_ring_and_direct_fallback_both_use_the_opt_in_policy(self):
        self.assertIn("pullSource(buffer, buffSize, cooperativeSourceRead)", SOURCE)
        self.assertIn("pullSource(ptr, missing, cooperativeSourceRead)", SOURCE)
        self.assertIn("audio_buffer::pull(src, data, len, cooperative)", SOURCE)
        self.assertIn("shortageEpisode.onRead(requested, bytes)", SOURCE)
        self.assertEqual(SOURCE.count(
            "filled = audio_buffer::keepOpportunisticBytes(cooperativeSourceRead, filled, cnt)"
        ), 3)
        self.assertIn('#include "AudioShortageEpisode.h"', HEADER)
        self.assertNotIn("bytes += src->read(ptr, len)", SOURCE)
        # Opportunistic refill and our live metadata path already accept
        # nonblocking reads; this does not add a second network producer.
        self.assertEqual(SOURCE.count('pullSource(&buffer[writePtr],'), 3)
        self.assertIn('pullSource(&buffer[writePtr], bytesAvailMid, true)', SOURCE)
        self.assertIn('pullSource(&buffer[writePtr], bytesAvailEnd, true)', SOURCE)
        self.assertIn('pullSource(&buffer[writePtr], bytesAvailStart, true)', SOURCE)
        self.assertIn("readAudio(data, len, true)", TLS)
        self.assertIn("consumeMetadata(nonBlocking)", TLS)

    def test_underflow_counts_one_episode_not_each_nonblocking_poll(self):
        self.assertIn("audio_buffer::ShortageEpisode shortageEpisode", HEADER)
        self.assertIn("shortageEpisode.onRead(requested, bytes)", SOURCE)
        self.assertIn("shortageEpisode.reset()", SOURCE)
        self.assertNotIn("cb.st(STATUS_UNDERFLOW, PSTR(\"Buffer underflow\"))", SOURCE)
        self.assertIn("AudioShortageEpisode.h", HEADER)


if __name__ == "__main__":
    unittest.main()
