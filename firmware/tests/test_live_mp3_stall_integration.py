"""Source contracts for the optional live-MP3 would-block recovery path."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "arduino-sketchbook/libraries/ESP8266Audio/src"
MP3 = (LIB / "AudioGeneratorMP3.cpp").read_text(encoding="utf-8")
MP3_H = (LIB / "AudioGeneratorMP3.h").read_text(encoding="utf-8")
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


class LiveMP3StallContracts(unittest.TestCase):
    def test_only_pocket_jazz_opts_into_bounded_live_wait(self):
        self.assertIn("SetLiveStallTimeoutMs(uint32_t timeoutMs)", MP3_H)
        self.assertIn("decoder->SetLiveStallTimeoutMs(4000)", APP)
        self.assertIn("IsWaitingForLiveInput() const", MP3_H)
        self.assertIn("decoder->IsWaitingForLiveInput() ? BUFFERING : PLAYING", APP)
        self.assertIn("timeoutMs_ = 0;", (LIB / "AudioLiveStallPolicy.h").read_text())

    def test_audio_loop_defers_incomplete_frames_without_closing(self):
        self.assertIn("return MAD_FLOW_IGNORE; // return to app task", MP3)
        self.assertIn("if (inputFlow == MAD_FLOW_IGNORE)", MP3)
        self.assertIn("stream->next_frame = unused ? buff : NULL", MP3)
        self.assertIn("if (stream->error == MAD_ERROR_BUFLEN &&", MP3)
        self.assertIn("stream->next_frame = buff;", MP3)
        self.assertIn("if (!resumedFromWorkYield && !liveStallPolicy.waiting() &&", MP3)
        self.assertIn("!output->ConsumeSample(lastSample))", MP3)
        self.assertIn("liveStallPolicy.reset();", MP3)
        self.assertIn("audio_mp3::findSyncHeader(buff, lastBuffLen)", MP3)
        self.assertIn("audio_mp3::carryPartialSync(buff, lastBuffLen)", MP3)
        self.assertIn("frameWatchdog.expired(millis())", MP3)
        self.assertIn("frameWatchdog.decodedFrame(millis())", MP3)
        self.assertIn("frameWatchdog.start(millis())", MP3)
        self.assertIn("timeoutMs * 3", MP3_H)
        self.assertIn('#include "AudioMp3SyncBoundary.h"', MP3)
        self.assertIn("if (unused >= buffLen)", MP3)
        self.assertIn("liveStallPolicy.reset();", MP3)

    def test_real_eof_and_dead_source_remain_reconnectable(self):
        self.assertIn("file->isOpen()", MP3)
        self.assertIn("file->getSize()", MP3)
        self.assertIn("return MAD_FLOW_STOP;", MP3)
        self.assertIn("const bool decoderHealthy = decoder->isRunning() && decoder->loop()", APP)
        self.assertIn("if (!decoderHealthy)", APP)
        self.assertIn("audioState.store(ERROR_STREAM)", APP)


if __name__ == "__main__":
    unittest.main()
