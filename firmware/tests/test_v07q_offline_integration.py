"""Source-level contracts: manual-only local MP3 through the SAME decoder task.

No test here asserts physical SD/FATFS latency or acoustic ES8311 output.
"""

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
POLICY = (ROOT / 'PocketJazzRadio/OfflineLibraryPolicy.h').read_text(encoding='utf-8')


class OfflineLibraryIntegration(unittest.TestCase):
    def test_playlist_and_offline_library_have_disjoint_namespaces(self):
        self.assertIn('"/PocketJazz/offline"', APP)
        self.assertIn('"/PocketJazz/offline/"', APP)
        self.assertIn('if (pressed(k, \'0\')) { enterOfflineLibrary(); return; }', APP)
        self.assertIn('if (wantPlay || audioState.load() != STOPPED || offlineSourceOpen.load())', APP)
        self.assertIn('checked < 96 && offlineCatalog.count() < 24', APP)
        self.assertIn('OfflineLibraryPolicy<24>', APP)
        self.assertIn('static constexpr uint32_t kMaxId3Bytes = 2u * 1024u * 1024u', POLICY)
        self.assertNotIn('SD.mkdir(', APP)

    def test_audio_worker_only_one_es8311_owner_and_offline_not_wifi_gated(self):
        self.assertEqual(APP.count('static AudioOutputM5Speaker output(&M5.Speaker, 0)'), 1)
        self.assertIn('if (!sourceOffline && (!wifiReady.load()', APP)
        self.assertIn('if (!sourceOffline && strncmp(', APP)
        self.assertIn('if (!decoder && sourceOffline)', APP)
        self.assertIn('AudioFileSourceSD()', APP)
        self.assertIn('id3Start(', APP)
        self.assertIn('file->seek((int32_t)contentOffset, SEEK_SET)', APP)
        self.assertIn('decoder->begin(src, &output)', APP)
        self.assertIn('if (!decoder && !sourceOffline)', APP)
        self.assertIn('if (sourceOffline) {', APP)
        self.assertIn('src->getPos() >= src->getSize()', APP)
        self.assertIn('audioState.store(reachedEnd ? STOPPED : ERROR_STREAM)', APP)
        worker = APP.split('void audioWorker(void*)', 1)[1].split('void loadPeekConfig()', 1)[0]
        self.assertNotIn('Serial.', worker)
        self.assertNotIn('appendUiExport(', worker)
        self.assertNotIn('SD.mkdir(', worker)

    def test_bounded_close_and_no_automatic_fallback(self):
        self.assertIn('offlineLeaving = true', APP)
        self.assertIn('offlineSourceOpen.store(false, std::memory_order_release)', APP)
        self.assertIn('offlineFinished.store(true, std::memory_order_release)', APP)
        self.assertIn('if (offlineLeaving && !offlineSourceOpen.load', APP)
        self.assertIn('desired = false; // do not spin/reopen a missing local file', APP)
        self.assertIn('offlineMode = false;', APP)
        self.assertIn('lastRequestedAudioPlay = false;', APP)
        self.assertIn('if (offlineMode || idx < 0', APP)
        self.assertIn('if (offlineMode) return; // old network titles', APP)
        self.assertIn('!(offlineMode && audioState.load() == PLAYING)', APP)
        self.assertNotIn('offlineFallbackEnabled', APP)

    def test_screen_scope_and_data_preservation(self):
        self.assertIn('case OFFLINE_LIBRARY: drawOfflineLibrary()', APP)
        self.assertIn('case OFFLINE_LIBRARY:', APP)
        self.assertIn('"0 OFFLINE MP3"', APP)
        self.assertIn('"-/+ volume  DEL stop+radio"', APP)
        offline_handler = APP.split('if (screen == OFFLINE_LIBRARY)', 1)[1].split('if (screen == RECENTS)', 1)[0]
        for forbidden in ('SD.open(', 'savePreset(', 'likeCurrentTrack(', 'WiFi.begin('):
            self.assertNotIn(forbidden, offline_handler)


if __name__ == '__main__':
    unittest.main()
