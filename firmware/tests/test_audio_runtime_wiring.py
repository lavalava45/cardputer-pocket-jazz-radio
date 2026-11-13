"""Integration wiring guards, not firmware compilation or physical proof."""
from pathlib import Path
import unittest
P = Path(__file__).resolve().parents[1] / 'PocketJazzRadio'
APP = (P/'PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
SOURCE = (P/'SecureICYSource.cpp').read_text(encoding='utf-8')

class RuntimeWiring(unittest.TestCase):
    def test_observer_and_exports_only_ui(self):
        worker = APP.split('void audioWorker(void*) {',1)[1].split('void loadPeekConfig()',1)[0]
        for forbidden in ('appendUiExport(', 'Serial.printf(', 'observeAudioRuntime('):
            self.assertNotIn(forbidden, worker)
        self.assertIn('observeAudioRuntime(now);', APP.split('void loop() {',1)[1])
        self.assertIn('if (diagPage == 7) exportAudioRuntime();', APP)
        self.assertIn('"/PocketJazz/audio-runtime.tsv"', APP)

    def test_single_audio_owner_and_unchanged_tuning(self):
        self.assertEqual(APP.count('secureSrc->setRuntimeProbe(&audioRuntime);'),2)
        self.assertIn('decoder->SetLiveStallTimeoutMs(4000)',APP)
        self.assertIn('constexpr int STREAM_BYTES = 32768;',APP)
        # A low-memory Cardputer now needs a larger prefill reserve for its
        # deliberately smaller ring, while normal RAM retains the 2-KiB goal.
        self.assertIn('pocketjazz::initialMp3PrefillGoal(ringCapacity)', APP)
        self.assertNotIn('? 8192u : 2048u', APP)
        self.assertIn('buffer->getFillLevel(), prefillGoal',APP)
        self.assertIn('SetCooperativeSourceRead(true)',APP)

    def test_live_network_and_latched_error_before_command_cancel(self):
        connect = SOURCE.split('int RadioTracedTlsClient::connect(',1)[1].split('SecureICYSource::SecureICYSource(',1)[0]
        self.assertIn('AudioStage::DNS',connect)
        self.assertIn('AudioStage::TCP_TLS',connect)
        self.assertIn('AudioStage::HTTP',connect)
        callback = SOURCE.split('bool SecureICYSource::RegisterStatusCB(',1)[1].split('void SecureICYSource::reportFailure',1)[0]
        self.assertNotIn('runtime',callback)
        attempt = APP.split('const bool alive = opened && src->isOpen();',1)[1]
        self.assertLess(attempt.index('audioRuntime.fault('),attempt.index('if (superseded())'))

    def test_peek_instance_never_attaches_player_probe(self):
        peek = APP.split('void stationPeekWorker(void*) {',1)[1].split('void invalidateStationPeek(',1)[0]
        self.assertIn('secure->setPeekLimits()',peek)
        self.assertIn('source->open(command.url)',peek)
        self.assertNotIn('setRuntimeProbe(',peek)
        self.assertNotIn('audioRuntime',peek)
        header = (P/'SecureICYSource.h').read_text(encoding='utf-8')
        self.assertIn('AudioRuntimeProbe* runtime = nullptr;',header)
        self.assertIn('tls_.runtime = p;',header)

    def test_phase_order_retry_and_no_busy_timeout_recovery(self):
        method = SOURCE.split('int RadioTracedTlsClient::connect(',1)[1].split('SecureICYSource::SecureICYSource(',1)[0]
        self.assertLess(method.index('AudioStage::DNS'),method.index('Network.hostByName('))
        self.assertLess(method.index('AudioStage::TCP_TLS'),method.index('NetworkClientSecure::connect(address'))
        opening = SOURCE.split('bool SecureICYSource::open(',1)[1]
        self.assertLess(opening.index('AudioStage::HTTP'),opening.index('http_.GET()'))
        self.assertIn('tls_.runtime->setHop(hop + 1)',opening)
        report = SOURCE.split('void SecureICYSource::reportFailure(',1)[1].split('bool SecureICYSource::safeUrl(',1)[0]
        self.assertIn('tls_.runtime->fault(',report)
        self.assertIn('audioRuntime.retry(retryAt,',APP)
        speaker = (P/'AudioOutputM5Speaker.h').read_text(encoding='utf-8')
        self.assertIn('while (_busy[_index]) { publishOutstanding(); vTaskDelay(1); }',speaker)
        release = speaker.split('static void bufferReleased(',1)[1]
        self.assertNotIn('_runtime',release) # callback must not become another probe writer

if __name__=='__main__': unittest.main()
