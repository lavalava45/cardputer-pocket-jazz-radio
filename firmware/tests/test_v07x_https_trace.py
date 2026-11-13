"""v0.7x: structural guards; hardware TLS/latency requires physical Cardputer."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
SECURE = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'PocketJazzRadio/SecureICYSource.h').read_text(encoding='utf-8')


class RadioTraceContract(unittest.TestCase):
    def test_buffer_sample_after_decode_and_cleanup_clears_all(self):
        worker = APP.split('void audioWorker(void*) {', 1)[1].split('void loadPeekConfig() {', 1)[0]
        cleanup = worker.split('auto cleanup = [&]() {', 1)[1].split('while (true)', 1)[0]
        for field in ('audioBufferedBytes', 'audioBufferMinimum', 'audioBufferMinAtMs'):
            self.assertIn(field + '.store(0', cleanup)
        self.assertIn('bufferLevels.reset()', cleanup)
        self.assertIn('publishBuffer(); // first valid minimum', worker)
        self.assertLess(worker.index('decoder->loop();'),
                        worker.index('publishBuffer(); // measure drain AFTER decoder->loop'))
        self.assertNotIn('if (buffer) audioBufferedBytes.store(buffer->getFillLevel())', worker)

    def test_single_dns_lookup_and_verified_hostname_connect(self):
        method = SECURE.split('int RadioTracedTlsClient::connect(', 1)[1].split(
            'SecureICYSource::SecureICYSource(', 1)[0]
        self.assertEqual(method.count('Network.hostByName('), 1)
        self.assertIn('NetworkClientSecure::connect(address, port, host,', method)
        self.assertIn('_CA_cert, _cert, _private_key', method)
        self.assertIn('trace.largestBefore =', method)
        self.assertIn('trace.largestAfter =', method)
        self.assertIn('int connect(const char* host, uint16_t port, int32_t timeout) override;', HEADER)
        self.assertNotIn('setInsecure(', SECURE + HEADER)

    def test_trace_classifies_post_connect_separately(self):
        opening = SECURE.split('bool SecureICYSource::open(', 1)[1].split(
            'uint32_t SecureICYSource::readBody(', 1)[0]
        self.assertIn('trace_.dnsOk == 0', opening)
        self.assertIn('trace_.secureOk == 0', opening)
        self.assertIn('trace_.secureOk == 1', opening)
        self.assertIn('trace_.phase = HTTPS_POST_CONNECT', opening)
        self.assertIn('trace_.hop = hop + 1;', opening)
        self.assertIn('lastTlsError_ = trace_.dnsOk == 0 ? 0', opening)
        self.assertIn('http_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS)', SECURE)

    def test_ui_reports_latched_fault_not_secret_and_gap(self):
        worker = APP.split('void audioWorker(void*) {', 1)[1].split('void loadPeekConfig() {', 1)[0]
        self.assertIn('const RadioHttpsTrace trace = secureSrc->lastHttpsTrace();', worker)
        self.assertIn('if (opened && !alive) audioHttpsPhase.store(HTTPS_POST_CONNECT)', worker)
        self.assertNotIn('Serial.printf(', worker)
        ui = APP.split('void drawDiagnostics(bool partial = false) {', 1)[1].split(
            'void drawSettingsBackup()', 1)[0]
        self.assertIn('audioHttpsLargestBefore.load()', ui)
        self.assertIn('audioHttpsLargestAfter.load()', ui)
        self.assertIn('uiLoopMaxGapMs.load()', ui)
        self.assertIn('"HTTPS CONNECTION"', ui)
        self.assertIn('} else if (diagPage == 2) {', ui)
        self.assertIn('diagPage = (diagPage + DIAG_PAGE_COUNT - 1) % DIAG_PAGE_COUNT;', APP)


if __name__ == '__main__':
    unittest.main()
