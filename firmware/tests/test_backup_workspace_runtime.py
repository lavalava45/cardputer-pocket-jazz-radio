"""Regression: optional 13 KiB SD/NVS backup workspace is not permanent DRAM.

The M5Stack ADV uses PSRAM-disabled ESP32-S3. Never force this workspace into
static DRAM or onto the UI task's limited stack on radio startup.
"""
import re
import unittest
from pathlib import Path


APP = (Path(__file__).resolve().parents[1] / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')


class BackupMemoryLifetimeTest(unittest.TestCase):
    def test_only_type_exists_at_global_scope(self):
        before = APP.split('void captureBackupSettings(', 1)[0]
        self.assertIn('struct SettingsBackupWorkspace {', before)
        self.assertIn('char wire[pocketjazz::SettingsBackupPolicy::kMaxFileBytes]', before)
        self.assertNotRegex(before, r'\bstatic\s+(?:pocketjazz::SettingsBackupData|char)\s+settings(?:Stage|Rollback|BackupWire)')
        self.assertNotRegex(before, r'\bstatic\s+SettingsBackupWorkspace\b')

    def test_workspace_allocated_only_during_user_request(self):
        for name, next_name in [('exportSettingsBackup', 'importSettingsBackup'),
                                ('importSettingsBackup', 'restorePrefs')]:
            section = APP.split('bool ' + name + '() {', 1)[1].split('\nvoid ' + next_name + '(', 1)[0] if name == 'importSettingsBackup' else APP.split('bool ' + name + '() {', 1)[1].split('\nbool ' + next_name + '(', 1)[0]
            self.assertIn('if (!settingsIoAllowed()) return false;', section)
            self.assertIn('std::unique_ptr<SettingsBackupWorkspace> workspace(new (std::nothrow) SettingsBackupWorkspace)', section)
            self.assertIn('if (!workspace) { showNotice("Backup memory low"); return false; }', section)
            self.assertIn('workspace->wire', section)
            self.assertNotIn('SettingsBackupWorkspace workspace;', section)
            self.assertNotIn('malloc(', section)
        self.assertIn('sizeof(workspace->wire)', APP)
        self.assertNotIn('sizeof(settingsBackupWire)', APP)

    def test_diagnostics_distinguish_raw_tls_from_negative_error(self):
        diag = APP.split('void drawDiagnostics(bool partial = false) {', 1)[1].split('void drawSettingsBackup() {', 1)[0]
        self.assertIn('"TLS result code: "', diag)
        self.assertIn('tlsRaw < 0 ? " (ERROR)"', diag)
        self.assertIn('tlsRaw > 0 ? " (SOCKET)"', diag)
        self.assertIn('heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)', diag)


if __name__ == '__main__':
    unittest.main()
