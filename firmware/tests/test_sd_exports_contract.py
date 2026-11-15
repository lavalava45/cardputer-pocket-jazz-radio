"""Offline source contracts for explicitly requested bounded SD log export.

These are source-level checks, not an actual microSD write or latency test.
"""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / 'PocketJazzRadio' / 'PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
DOC = (ROOT / 'docs' / 'SD-EXPORTS.md').read_text(encoding='utf-8')


def section(first, last):
    return APP.split(first, 1)[1].split(last, 1)[0]


class SDExportContracts(unittest.TestCase):
    def test_only_r_exports_diagnostics_while_e_remains_for_personal_icy_and_backup(self):
        keys = section('void handleKeys(const Press& k)', 'void serviceSleepTimer(')
        diagnostics = section('if (screen == DIAGNOSTICS)', 'if (screen == SETTINGS_BACKUP)')
        backup = section('if (screen == SETTINGS_BACKUP)', 'if (screen == RECENTS)')
        tracks = section('if (screen == TRACKS)', 'if (screen == SHARE)')
        self.assertIn("if (pressed(k, 'r')) { exportIncidentReport(); return; }", diagnostics)
        self.assertNotIn("pressed(k, 'e')", diagnostics)
        self.assertNotIn("pressed(k, 'm')", diagnostics)
        self.assertIn("if (pressed(k, 'x'))", diagnostics)
        self.assertIn("if (pressed(k, 'e')) { exportIcyHistory(); return; }", tracks)
        self.assertIn("if (pressed(k, 'e'))", backup)
        self.assertIn("backupConfirmation == 1", backup)
        # New E entry to Jazz Journey adds a third context; the Diagnostics
        # handler still must not contain E or M.
        self.assertEqual(keys.count("pressed(k, 'e')"), 3)
        help_screen = section('void drawHelp() {', 'void drawRecents()')
        self.assertIn('F BROWSE STATIONS', help_screen)
        self.assertIn('U NOTES SEARCH', help_screen)
        self.assertIn('I R FULL REPORT', help_screen)
        self.assertIn('R FULL REPORT(STOP)  X RESET', APP)
        self.assertIn('UP/DOWN ENTER full  E save DEL', APP)

    def test_export_uses_no_new_network_or_decoder_io(self):
        func = section('ExportWriteStatus appendUiExport(', 'void showExportStatus(')
        self.assertIn('SD.open(path, FILE_READ)', func)
        self.assertIn('SD.open(path, FILE_APPEND)', func)
        self.assertIn('canAppend(existing, length, maxFileBytes)', func)
        self.assertIn('canAppend(file.size(), length, maxFileBytes)', func)
        self.assertIn('file.write(', func)
        self.assertIn('if (written != length) return EXPORT_FAILED;', func)
        self.assertIn('length > 2048', func)
        self.assertIn('old.read() != \'\\n\'', func)
        self.assertIn('return EXPORT_INCOMPLETE', func)
        self.assertIn('file.flush()', func)
        self.assertIn('File verify = SD.open(path, FILE_READ)', func)
        self.assertIn('verify.size() != existing + length', func)
        self.assertIn('memcmp(check, data + checked, count)', func)
        # The new optional peek worker is a DIFFERENT task; isolate the
        # primary decoder's exact function rather than all later helpers.
        worker = section('void audioWorker(void*)', 'void loadPeekConfig()')
        self.assertNotIn('appendUiExport(', worker)
        self.assertNotIn('exportIcyHistory(', worker)
        self.assertNotIn('exportIncidentReport(', worker)
        self.assertNotIn('SD.open(', worker)

    def test_diagnostics_excludes_sensitive_values(self):
        diagnostic = section('void exportIncidentReport()', 'void exportIcyHistory()')
        self.assertIn('/PocketJazz/incident-', diagnostic)
        self.assertIn('ntpSynced.load()', diagnostic)
        for secret in ('savedPassword', 'savedSsid', 'wifiPassword',
                       'wifiSsid', 'stations[idx].url', 'lastSelectedUrl',
                       'localIP().toString()', 'p.getString('):
            self.assertNotIn(secret, diagnostic, secret)
        self.assertIn('SSID', DOC)
        self.assertIn('16 КиБ', DOC)
        self.assertNotIn('void exportAudioRuntime()', APP)
        self.assertNotIn('void exportDiagnostics()', APP)
        self.assertNotIn('void exportHeapLifecycle()', APP)

    def test_track_exports_only_observed_ram_history_with_safe_tsv(self):
        track = section('void exportIcyHistory()', 'void likeCurrentTrack()')
        self.assertIn('trackHistory.size()', track)
        self.assertIn('trackHistory.newest(i - 1)', track)
        self.assertIn('observedAtMs', track)
        self.assertIn('cleanTsvField(item->title', track)
        self.assertIn('cleanTsvField(item->station', track)
        self.assertIn('/PocketJazz/icy-history.tsv', track)
        self.assertIn('char row[256]', track)
        self.assertIn('const ExportWriteStatus status = appendUiExport(', track)
        self.assertIn('if (status != EXPORT_WRITTEN)', track)
        self.assertIn('ICY export partial: SD', track)
        self.assertNotIn('char rows[1200]', track)
        self.assertNotIn('WiFiClient', track)
        self.assertNotIn('HTTPClient', track)
        self.assertNotIn('SD.open(', track)
        self.assertIn('не доказанная граница композиции', DOC)

    def test_like_remains_manual_and_checks_whole_line_limit(self):
        like = section('void likeCurrentTrack()', 'int findStationUrl(')
        self.assertIn('/PocketJazz/liked.txt', like)
        self.assertIn('row.c_str(), row.length(), 65536', like)
        self.assertNotIn('log.size() >= 65536', like)
        keys = section('void handleKeys(const Press& k)', 'void serviceSleepTimer(')
        self.assertIn("if (pressed(k, 'k')) { likeCurrentTrack(); return; }", keys)


if __name__ == '__main__':
    unittest.main()
