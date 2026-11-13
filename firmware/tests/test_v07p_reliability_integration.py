"""Cross-layer safety contracts for post-v0.7o audio/Wi-Fi/SD work.

These are static integration assertions, NOT substitutes for live hardware.
"""

import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio' / 'PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
TELEMETRY = (ROOT / 'PocketJazzRadio' / 'AudioPlayoutTelemetry.h').read_text(encoding='utf-8')
SPEAKER = (ROOT / 'PocketJazzRadio' / 'AudioOutputM5Speaker.h').read_text(encoding='utf-8')
WIFI = (ROOT / 'PocketJazzRadio' / 'WifiProfilePolicy.h').read_text(encoding='utf-8')
BACKUP = (ROOT / 'PocketJazzRadio' / 'SettingsBackupPolicy.h').read_text(encoding='utf-8')
HEALTH = (ROOT / 'PocketJazzRadio' / 'StationHealth.h').read_text(encoding='utf-8')


class ReliabilityIntegration(unittest.TestCase):
    def test_audio_pcm_is_accepted_handoff_not_network_drop(self):
        self.assertIn('output.setMonitor(pcmMonitor, &output)', APP)
        self.assertIn('audioPlayout.pcmEnqueued(audioAcceptedSequence, millis()', APP)
        self.assertIn('audioPlayout.stopped(currentTelemetrySequence, millis())', APP)
        self.assertIn('const bool newIntent =', APP)
        self.assertIn('requestSequence_.load(std::memory_order_acquire)', TELEMETRY)
        self.assertIn('kGapThresholdMs = 100', TELEMETRY)
        self.assertIn('const uint64_t numerator = static_cast<uint64_t>(frames) * 1000u + remainder_', TELEMETRY)
        self.assertIn('remainder_ = static_cast<uint32_t>(numerator % rate)', TELEMETRY)
        self.assertNotIn('+ rate - 1u) / rate', TELEMETRY)
        self.assertIn('accountGapAt(requestAtMs_.load(std::memory_order_relaxed))', TELEMETRY)
        self.assertIn('gapAccountedMs_ = observed', TELEMETRY)
        self.assertIn('Keep the current source clock and first-PCM epoch', TELEMETRY)
        self.assertIn('setEnqueueFailureMonitor(pcmEnqueueFailure, nullptr)', APP)
        self.assertIn('if (_enqueue_failure) _enqueue_failure(_enqueue_failure_arg)', SPEAKER)
        worker = APP.split('void audioWorker(void*)', 1)[1].split('void loadPeekConfig()', 1)[0]
        self.assertNotIn('Serial.', worker)
        status = APP.split('void audioStatusCB(', 1)[1].split('void bufferStatusCB(', 1)[0]
        self.assertNotIn('Serial.', status)
        self.assertNotIn('Serial.', TELEMETRY)
        self.assertNotRegex(TELEMETRY, r'\b(malloc|new|SD\.open|String|vTaskDelay)\s*\(')

    def test_diagnostics_has_manual_reset_and_bounded_export(self):
        self.assertIn('pressed(k, \'x\')', APP)
        self.assertIn('audioPlayout.resetCounters()', APP)
        self.assertIn('pcm_gap_count=', APP)
        self.assertIn('stop_p95_upper_ms=', APP)
        self.assertIn('switch_p95_upper_ms=', APP)
        self.assertIn('stop_p50_upper_ms=', APP)
        self.assertIn('switch_p50_upper_ms=', APP)
        self.assertIn('pcm_enqueue_reject=', APP)
        self.assertIn('appendUiExport("/PocketJazz/diagnostics.tsv"', APP)

    def test_wifi_keeps_legacy_and_commits_only_stable_profiles(self):
        self.assertIn('p.getString("ssid", "")', APP)
        self.assertIn('p.getString("pass", "")', APP)
        self.assertIn('WIFI_STABLE_MS = 2000', APP)
        self.assertIn('wifiMigrationPending && !wifiMigrationAttempted', APP)
        self.assertIn('p.putUChar("wfa", target)', APP)
        self.assertIn('futureWifiProfileFormat = wifiFormat > 1', APP)
        self.assertIn('if (futureWifiProfileFormat)', APP)
        self.assertIn('const uint8_t former = p.getUChar("wfa", 255)', APP)
        self.assertIn('WifiProfilePolicy::kCapacity', APP)
        self.assertIn('if (wifiProfiles.count() >= 2)', APP)
        self.assertIn('WiFi.scanNetworks(true, false)', APP)
        self.assertNotIn('WiFi.persistent(true)', APP)
        self.assertIn('static constexpr int kCapacity = 2', WIFI)

    def test_mailbox_coalesced_stop_play_still_starts_new_audio_epoch(self):
        self.assertIn('currentTelemetrySequence != cmd.telemetrySequence', APP)
        self.assertIn('xQueueOverwrite(audioQueue, &cmd)', APP)
        self.assertIn('audioPlayout.command(currentTelemetrySequence)', APP)

    def test_sd_backup_is_whitelisted_manual_and_stop_gated(self):
        self.assertIn('if (wantPlay || audioState.load() != STOPPED)', APP)
        self.assertIn('backupConfirmation == 1', APP)
        self.assertIn('backupConfirmation == 2', APP)
        self.assertIn('"B SETTINGS BACKUP"', APP)
        self.assertIn('"/PocketJazz/settings.tmp"', APP)
        self.assertIn('"/PocketJazz/settings.bak"', APP)
        self.assertIn('SettingsBackupPolicy::decode', APP)
        self.assertIn('captureBackupSettings(settingsRollback)', APP)
        self.assertNotIn('ssid', BACKUP.lower())
        self.assertNotIn('password', BACKUP.lower())
        self.assertNotIn('WiFi.', BACKUP)
        self.assertNotIn('SD.', BACKUP)

    def test_no_unsafe_audio_tls_or_flash_paths(self):
        self.assertNotIn('setInsecure(', APP)
        self.assertNotIn('WiFi.eraseAP()', APP)
        self.assertNotIn('nvs_flash_erase', APP)
        self.assertNotIn('esp_partition_erase_range', APP)
        self.assertIn('M5.Speaker.setVolume', APP)
        self.assertEqual(len(re.findall(r'AudioOutputM5Speaker output\(', APP)), 1)

    def test_station_health_tracks_complete_failures_not_already_existing_alt(self):
        self.assertIn('const bool exhausted = usingAlt || !altUrl[0]', APP)
        self.assertIn('stationHealth.failed(stationId)', APP)
        self.assertIn('stationHealth.recovered(stationId)', APP)
        self.assertIn('audioFrames.load() - stableFrameCount', APP)
        self.assertIn('stationHealth.warning(id)', APP)
        self.assertIn('stationHealth.reset()', APP)
        self.assertIn('decodedPcmRateHz.store(rate, std::memory_order_relaxed)', APP)
        self.assertIn('static constexpr uint8_t kWarningStreak = 3', HEALTH)
        self.assertNotIn('Preferences', HEALTH)
        self.assertNotIn('SD.open', HEALTH)


if __name__ == '__main__':
    unittest.main()
