"""Verify URL normalization and STOP/volume generation are wired into owners.

These are integration/source contracts. SDK-compatible parsing is exercised by
the real NDEBUG-enabled MSVC CTest, and ESP32 compilation validates includes.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
TLS = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
SAFE = (ROOT / 'PocketJazzRadio/SafeHttpsUpgrade.h').read_text(encoding='utf-8')
ICY = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceICYStream.cpp').read_text(encoding='utf-8')
TEST = (ROOT / 'tests/safe_https_upgrade_test.cpp').read_text(encoding='utf-8')
POLICY = (ROOT / 'PocketJazzRadio/AudioIntentPolicy.h').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests/pcm/CMakeLists.txt').read_text(encoding='utf-8')


class UrlAndAudioIntentIntegration(unittest.TestCase):
    def test_http_https_upgrade_preserves_host_port_query_and_sdk_split(self):
        self.assertIn('const bool needsRootSlash = hostEnd < length', SAFE)
        self.assertIn("location[hostEnd] == '?'", SAFE)
        self.assertIn('length + 1 >= cap', SAFE)
        self.assertIn("out[outLength++] = '/'", SAFE)
        self.assertIn('copySafeHttpsUpgrade(', ICY)
        self.assertIn('src->open(upgradedUrl)', APP)
        self.assertIn('https://radio.example.org?stream=1', TEST)
        self.assertIn('expectSdk339HostAndUri(', TEST)
        self.assertIn('#undef NDEBUG', TEST)

    def test_direct_https_and_redirect_share_sdk_compatible_normalization(self):
        self.assertIn('const int queryBegin = url.indexOf(\'?\', hostBegin)', TLS)
        self.assertIn('static String httpClientCompatibleHttpsUrl(', TLS)
        self.assertIn('return url.substring(0, query) + "/" + url.substring(query)', TLS)
        self.assertIn('String target = httpClientCompatibleHttpsUrl(String(url));', TLS)
        self.assertIn('target = httpClientCompatibleHttpsUrl(next);', TLS)
        self.assertIn('target.length() > 1024', TLS)
        self.assertIn('tls_.setCACert(caPem_)', TLS)
        self.assertNotIn('setInsecure(', TLS)

    def test_cancel_sequence_distinguishes_volume_from_stop_and_switch(self):
        self.assertIn('audioRequestedIntentSequence.store(telemetryNextSequence', APP)
        self.assertIn('std::memory_order_release', APP)
        self.assertIn('std::memory_order_acquire', APP)
        self.assertIn('cmd.telemetrySequence', APP)
        self.assertIn('currentTelemetrySequence', APP)
        self.assertIn('return active != latestRequested;', POLICY)
        self.assertIn('if (superseded())', APP)
        self.assertIn('const bool decoderReady = decoder && decoder->begin(buffer, &output);', APP)
        self.assertIn('if (decoder && decoder->begin(src, &output) && !superseded())', APP)
        self.assertIn('audio_intent_stop_switch_generation_vs_volume', CMAKE)
        self.assertIn('live_radio_buffer_minimum_reset_after_decoder_stop_and_reconnect', CMAKE)
        self.assertIn('plain_http_icy_partial_chunked_metadata_timeout_ring_and_reconnect', CMAKE)


if __name__ == '__main__':
    unittest.main()
