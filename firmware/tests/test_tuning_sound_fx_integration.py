"""Sound FX is a strict opt-in and cannot interfere with default radio playback."""
from pathlib import Path
import unittest

APP = (Path(__file__).resolve().parents[1] / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
FX = (Path(__file__).resolve().parents[1] / 'PocketJazzRadio/TuningSoundBed.h').read_text(encoding='utf-8')
WORKER = APP.split('void audioWorker(void*) {', 1)[1].split('void loadPeekConfig()', 1)[0]


class TuningSoundFxIntegration(unittest.TestCase):
    def test_both_settings_off_by_default_and_saved_independently_in_nvs(self):
        self.assertIn('vinylCrackleEnabled{false}', APP)
        self.assertIn('tuningHissEnabled{false}', APP)
        self.assertIn('p.getBool("fxVinyl", false)', APP)
        self.assertIn('p.getBool("fxHiss", false)', APP)
        self.assertIn('p.putBool("fxVinyl", vinylCrackleEnabled.load', APP)
        self.assertIn('p.putBool("fxHiss", tuningHissEnabled.load', APP)
        self.assertIn('p.putBool(key, next) == 1 && p.getBool(key, !next) == next', APP)
        self.assertIn('if (futureNvsSchema)', APP)

    def test_fx_settings_screen_is_only_opt_in_and_safe_boot_key_survives(self):
        self.assertIn("if (pressed(k, 's')) { screen = SOUND_FX; return; }", APP)
        self.assertIn("if (pressed(k, '1')) { saveTuningEffect(true); return; }", APP)
        self.assertIn("if (pressed(k, '2')) { saveTuningEffect(false); return; }", APP)
        self.assertIn('safeMode = M5Cardputer.Keyboard.isKeyPressed(\'s\')', APP)
        self.assertIn('"S TUNING SOUND FX"', APP)
        self.assertIn('"NEVER ON RETRIES OR MUTE"', APP)

    def test_single_audio_owner_channel_separation_and_first_pcm_fade(self):
        self.assertIn('static AudioOutputM5Speaker output(&M5.Speaker, 0)', WORKER)
        self.assertIn('M5.Speaker.playRaw(clip, pocketjazz::tuning::kSamples', WORKER)
        self.assertIn('pocketjazz::tuning::kChannel, true)', WORKER)
        self.assertIn('M5.Speaker.stop(pocketjazz::tuning::kChannel)', WORKER)
        self.assertIn('audioFrames.load(std::memory_order_relaxed) > fxInitialFrames', WORKER)
        self.assertIn('const bool firstUserAttempt = initialTunePending && !usingAlt', WORKER)
        self.assertIn('initialTunePending = false; // no new noise on automatic retries', WORKER)
        self.assertLess(WORKER.index('src->open(endpoint)'), WORKER.index('M5.Speaker.playRaw(clip'))
        self.assertIn('requestedOutputVolume.load(std::memory_order_acquire) == 0', WORKER)
        self.assertIn('stopTuningFx(); // also STOP, station change, failed TLS', WORKER)
        self.assertIn('heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)', WORKER)
        self.assertNotIn('vTaskCreatePinnedToCore(', FX)
        self.assertNotIn('malloc(', FX)
        self.assertNotIn('SD.open(', FX)

    def test_never_changes_decoder_or_music_channel(self):
        self.assertIn('kChannel = 1', FX)
        self.assertIn('kQuietChannelVolume = 8', FX)
        self.assertIn('channelVolumeForMaster(speakerVolume)', WORKER)
        self.assertIn('channelVolumeForMaster(', FX)
        self.assertIn('kFadeMs = 90', FX)
        self.assertIn('M5.Speaker.setVolume(speakerVolume)', WORKER)
        self.assertIn('output.setMonitor(pcmMonitor, &output)', WORKER)
        self.assertIn('constexpr std::array<int8_t, kSamples> makeClip', FX)


if __name__ == '__main__':
    unittest.main()
