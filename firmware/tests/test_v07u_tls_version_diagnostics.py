"""v0.7u: real mbedTLS error without insecure fallback; build-driven app version."""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
SECURE_H = (ROOT / 'PocketJazzRadio/SecureICYSource.h').read_text(encoding='utf-8')
SECURE_CPP = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
BUILD = (ROOT / 'tools/build.ps1').read_text(encoding='utf-8')
VERSION = (ROOT / 'PocketJazzRadio/BuildVersion.h').read_text(encoding='utf-8')


class TlsAndFirmwareVersionTests(unittest.TestCase):
    def test_https_error_is_sampled_before_source_cleanup(self):
        self.assertIn('int lastTlsError() const { return lastTlsError_; }', SECURE_H)
        opening = SECURE_CPP.split('bool SecureICYSource::open(const char *url) {', 1)[1].split(
            'uint32_t SecureICYSource::readBody(', 1)[0]
        self.assertIn('lastTlsError_ = 0;', opening)
        self.assertIn('const int status = http_.GET();', opening)
        self.assertIn('if (status < 0)', opening)
        self.assertIn('tls_.lastError(tlsErrorText, sizeof(tlsErrorText))', opening)
        self.assertIn('lastTlsError_ = trace_.dnsOk == 0 ? 0 :', opening)
        self.assertLess(opening.index('lastTlsError_ = trace_.dnsOk == 0 ? 0 :'),
                        opening.index('reportFailure(status,'))
        self.assertNotIn('setInsecure(', SECURE_CPP)
        self.assertNotIn('http://', opening.split('const int status = http_.GET();', 1)[1])

    def test_audio_worker_crosses_only_atomic_failure_counter(self):
        worker = APP.split('void audioWorker(void*) {', 1)[1].split('void loadPeekConfig() {', 1)[0]
        self.assertIn('std::atomic<int> audioTlsErrorCode{0};', APP)
        self.assertIn('SecureICYSource* secureSrc = nullptr;', worker)
        self.assertIn('audioTlsErrorCode.store(secureSrc->lastTlsError()', worker)
        self.assertLess(worker.index('audioTlsErrorCode.store(secureSrc->lastTlsError()'),
                        worker.index('const bool exhausted = usingAlt'))
        self.assertNotIn('Serial.printf(', worker)
        self.assertNotIn('Serial.println(', worker)

    def test_ui_and_serial_expose_only_numeric_codes(self):
        self.assertIn('POCKETJAZZ_FIRMWARE_VERSION', VERSION)
        self.assertIn('#include "BuildVersion.h"', APP)
        help_screen = APP.split('void drawHelp() {', 1)[1].split('void drawRecents()', 1)[0]
        diag = APP.split('void drawDiagnostics(bool partial = false) {', 1)[1].split(
            'void drawSettingsBackup() {', 1)[0]
        self.assertIn('POCKETJAZZ_FIRMWARE_VERSION', help_screen)
        self.assertIn('POCKETJAZZ_FIRMWARE_VERSION', diag)
        self.assertIn('audioTlsErrorCode.load()', diag)
        log = APP.split('void reportAudioFailureToSerial() {', 1)[1].split('void pcmMonitor(', 1)[0]
        self.assertIn('[PocketJazz][Audio]', log)
        for secret in ('cmd.url', 'endpoint', 'savedPassword', 'wifiSsid', 'Serial.print(url'):
            self.assertNotIn(secret, log)
        self.assertIn('reportAudioFailureToSerial();', APP.split('void loop() {', 1)[1])

    def test_version_generated_from_build_parameter_before_compile_inputs(self):
        self.assertIn("$buildVersionPath = Join-Path $sketch 'BuildVersion.h'", BUILD)
        self.assertIn('POCKETJAZZ_FIRMWARE_VERSION', BUILD)
        self.assertIn("$Version +", BUILD)
        self.assertLess(BUILD.index('$expectedVersionHeader ='),
                        BUILD.index('$compileInputs = @()'))
        self.assertIn('if (-not $PackageOnly)', BUILD)


if __name__ == '__main__':
    unittest.main()
