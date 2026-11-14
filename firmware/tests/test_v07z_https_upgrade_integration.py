"""Strict HTTP -> verified HTTPS and stale-request integration contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
PLAIN = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceICYStream.cpp').read_text(encoding='utf-8')
HDR = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceICYStream.h').read_text(encoding='utf-8')
POLICY = (ROOT / 'PocketJazzRadio/SafeHttpsUpgrade.h').read_text(encoding='utf-8')
TLS = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests/pcm/CMakeLists.txt').read_text(encoding='utf-8')


class HttpsUpgradeContracts(unittest.TestCase):
    def test_plain_client_exposes_only_validated_upgrade_and_closes_socket(self):
        opening = PLAIN.split('bool AudioFileSourceICYStream::open(', 1)[1].split(
            'AudioFileSourceICYStream::~AudioFileSourceICYStream(', 1)[0]
        self.assertIn('http.setRedirectLimit(5)', opening)
        self.assertIn('http.getLocation().c_str()', opening)
        self.assertIn('pocketjazz::copySafeHttpsUpgrade(', opening)
        self.assertIn('http.end();\n            return false;', opening)
        self.assertIn('const char* httpsUpgrade() const', HDR)
        self.assertIn('httpsUpgrade_[0] = 0;', PLAIN)
        self.assertIn('strncmp(url, "http://", 7) != 0', PLAIN)
        self.assertNotIn('setInsecure(', PLAIN + HDR + TLS)

    def test_security_validation_is_bounded_without_scheme_downgrade(self):
        self.assertIn('std::strncmp(location, "https://", 8)', POLICY)
        self.assertIn('length < cap', POLICY)
        self.assertIn('length == cap', POLICY)
        self.assertIn("c == '#' || c == '@'", POLICY)
        self.assertIn('port > 65535u', POLICY)
        self.assertNotIn('http://', POLICY)
        test = (ROOT / 'tests/safe_https_upgrade_test.cpp').read_text(encoding='utf-8')
        self.assertIn('#undef NDEBUG', test)
        self.assertIn('HTTPS://station.example/live', test)
        self.assertIn('https://user:pass@station.example/live', test)
        self.assertIn('bounded_http_to_verified_https_upgrade_no_downgrade_credentials_or_truncation', CMAKE)

    def test_owner_closes_plain_socket_before_constructing_verified_tls(self):
        w = APP.split('void audioWorker(void*) {', 1)[1].split('void loadPeekConfig() {', 1)[0]
        a = w.index('strlcpy(upgradedUrl, plainSrc->httpsUpgrade()')
        b = w.index('src->close();\n        delete src;', a)
        c = w.index('secureSrc = new (std::nothrow) SecureICYSource(', b)
        d = w.index('opened = src->open(upgradedUrl);', c)
        self.assertTrue(a < b < c < d)
        self.assertIn('!superseded()', w)
        # The audio task now stays at Arduino loopTask's priority 1 during
        # both verified HTTPS open and decoding. Otherwise a continuous
        # source can starve keyboard and screen servicing on core 1.
        self.assertIn('xTaskCreatePinnedToCore(audioWorker, "pocketjazz_audio", 8192, nullptr, 1, nullptr, 1)', APP)
        self.assertNotIn('vTaskPrioritySet(nullptr, 2);', w)
        self.assertNotIn('vTaskPrioritySet(nullptr, 1);', w)
        self.assertIn('if (superseded())', w)
        self.assertIn('cleanup();\n        vTaskDelay(1);\n        continue;', w)
        self.assertIn('const bool decoderReady = decoder && decoder->begin(buffer, &output);', w)
        self.assertIn('kRadioTrustedRootsPem', w)
        self.assertIn('NetworkClientSecure::connect(address, port, host,', TLS)
        self.assertNotIn('Serial.', w)


if __name__ == '__main__':
    unittest.main()
