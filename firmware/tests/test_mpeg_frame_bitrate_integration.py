"""Verify the displayed MPEG bitrate comes from decoded frames, not ICY metadata."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
MP3 = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioGeneratorMP3.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioGeneratorMP3.h').read_text(encoding='utf-8')


class RealMpegBitrateContract(unittest.TestCase):
    def test_only_successfully_decoded_libmad_frames_update_bitrate(self):
        decoded = MP3.split('bool AudioGeneratorMP3::DecodeNextFrame()', 1)[1].split('bool AudioGeneratorMP3::GetOneSample', 1)[0]
        self.assertIn('mad_frame_decode(frame, stream)', decoded)
        self.assertIn('frameBitrate.decodedFrame(static_cast<uint32_t>(frame->header.bitrate))', decoded)
        self.assertGreater(decoded.index('frameBitrate.decodedFrame('), decoded.index('return false;'))
        self.assertIn('frameBitrate.reset()', MP3)
        self.assertIn('GetDecodedFrameBitrateBps()', HEADER)
        self.assertIn('HasObservedVariableBitrate()', HEADER)

    def test_audio_owner_publishes_atomic_meter_and_ui_only_renders(self):
        worker = APP.split('void audioWorker(void*)', 1)[1].split('void loadPeekConfig()', 1)[0]
        signal = APP.split('void drawDiagnostics(', 1)[1].split('void drawSettingsBackup()', 1)[0]
        self.assertIn('decoder->GetDecodedFrameBitrateBps()', worker)
        self.assertIn('decoder->HasObservedVariableBitrate()', worker)
        self.assertIn('decodedMp3FrameBps.store(0,', worker)
        self.assertIn('decodedMp3ObservedVbr.store(false,', worker)
        self.assertIn('decodedMp3FrameBps.load(', signal)
        self.assertIn('decodedMp3ObservedVbr.load(', signal)
        self.assertNotIn('icy-br', signal)
        self.assertNotIn('SD.', signal)


if __name__ == '__main__':
    unittest.main()
