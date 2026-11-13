"""Opt-in monitor/source contracts; no RF/audio claims from source checks."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
HTTPS = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'PocketJazzRadio/SecureICYSource.h').read_text(encoding='utf-8')


class PeekIntegrationContracts(unittest.TestCase):
    def test_monitor_is_sd_opt_in_and_single_connection(self):
        self.assertIn('peekOptIn = false;', APP)
        self.assertIn('"/PocketJazz/peek.cfg"', APP)
        self.assertIn('line == "PEEK=1"', APP)
        self.assertIn('if (peekOptIn) {', APP)
        self.assertIn('xQueueCreate(1, sizeof(PeekCommand))', APP)
        self.assertIn('xQueueCreate(1, sizeof(PeekResult))', APP)
        self.assertIn('xTaskCreatePinnedToCore(stationPeekWorker', APP)

    def test_browse_does_not_change_active_station_until_enter(self):
        browse = APP.split('if (screen == BROWSE) {', 1)[1].split('if (screen == SEARCH)', 1)[0]
        self.assertIn('browseList = (browseList + 1)', browse)
        self.assertIn('selectByUrl(stations[id].url)', browse)
        self.assertNotIn('sendAudio();', browse)
        self.assertIn('const bool allowed = screen == BROWSE && peekOptIn;', APP)
        self.assertIn('else peekWanted.store(allowed);', APP)
        self.assertIn('audioUnderruns.load() != peekUnderflowsAtOpen', APP)

    def test_probe_has_time_traffic_heap_and_tls_limits(self):
        worker = APP.split('void stationPeekWorker(void*)', 1)[1].split('int browseFirstVisible()', 1)[0]
        self.assertIn('bytesRead < 65536u', worker)
        self.assertIn('3500u', worker)
        self.assertIn('source->readNonBlock', worker)
        self.assertIn('vTaskDelay(', worker)
        # Worker now uses a generation/request-aware helper instead of the
        # former bare flag. Verify the helper really gates the same flag.
        self.assertIn('peekRequestCurrent(command)', worker)
        helper = APP.split('bool peekRequestCurrent(const PeekCommand& command)', 1)[1].split('void stationPeekWorker(', 1)[0]
        self.assertIn('peekWanted.load()', helper)
        self.assertIn('audioUnderruns.load() == priorUnderruns', worker)
        self.assertIn('kRadioTrustedRootsPem', worker)
        self.assertIn('secure->setPeekLimits()', worker)
        self.assertNotIn('new (std::nothrow) AudioFileSourceICYStream()', worker)
        self.assertNotIn('setInsecure(', worker + HTTPS + HEADER)
        self.assertIn('maxRedirects_ = 1', HEADER)
        self.assertIn('http_.setConnectTimeout(connectTimeoutMs_)', HTTPS)
        self.assertIn('http_.setTimeout(ioTimeoutMs_)', HTTPS)
        self.assertIn('tls_.setCACert(caPem_)', HTTPS)

    def test_off_channel_probe_never_changes_playing_history(self):
        worker = APP.split('void stationPeekWorker(void*)', 1)[1].split('int browseFirstVisible()', 1)[0]
        self.assertNotIn('trackHistory.', worker)
        self.assertNotIn('currentTitle', worker)
        self.assertNotIn('SD.open', worker)
        self.assertIn('trackHistory.add(stations[idx].name, incoming.title, millis())', APP)

    def test_generation_and_request_fence_worker_and_reload(self):
        self.assertIn('std::atomic<uint32_t> peekLiveGeneration{1}', APP)
        self.assertIn('std::atomic<uint32_t> peekLiveRequest{0}', APP)
        self.assertIn('uint32_t playlistGeneration = 0;', APP)
        self.assertIn('uint32_t requestId = 0;', APP)
        self.assertIn('result.playlistGeneration = command.playlistGeneration;', APP)
        self.assertIn('result.requestId = command.requestId;', APP)
        self.assertIn('command.playlistGeneration == peekLiveGeneration.load()', APP)
        self.assertIn('command.requestId == peekLiveRequest.load()', APP)
        self.assertIn('if (peekRequestCurrent(command)) xQueueOverwrite(peekResultQueue, &result);', APP)
        self.assertIn('completed.playlistGeneration, completed.requestId', APP)
        self.assertIn('stationPeek.start(candidate, now, request.playlistGeneration,', APP)
        reload = APP.split("if (pressed(k, 'r')) {", 1)[1].split("if (pressed(k, 'p')", 1)[0]
        self.assertLess(reload.index('invalidateStationPeek(true)'), reload.index('loadPlaylists()'))
        self.assertIn('peekLiveGeneration.store(generation);', APP)
        self.assertIn('peekLiveRequest.store(0);', APP)
        browse = APP.split('if (screen == BROWSE) {', 1)[1].split('if (screen == SEARCH)', 1)[0]
        self.assertIn('invalidateStationPeek(false);', browse)

    def test_no_unbounded_queue_or_http_icy_probe(self):
        worker = APP.split('void stationPeekWorker(void*)', 1)[1].split('void invalidateStationPeek(', 1)[0]
        self.assertNotIn('AudioFileSourceICYStream()', worker)
        self.assertIn('peekRequestCurrent(command)', worker)
        self.assertIn('xQueueCreate(1, sizeof(PeekCommand))', APP)
        self.assertIn('xQueueCreate(1, sizeof(PeekResult))', APP)

    def test_on_device_help_discloses_optional_probe_and_search(self):
        help_screen = APP.split('void drawHelp() {', 1)[1].split('void drawRecents()', 1)[0]
        self.assertIn('F BROWSE STATIONS', help_screen)
        self.assertIn('U NOTES SEARCH', help_screen)
        self.assertIn('ICY PEEK SD OPT-IN', help_screen)


if __name__ == '__main__':
    unittest.main()
