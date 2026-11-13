"""Source contract tests for Arduino-free, on-demand bounded TSV helpers.

These do NOT inspect PocketJazzRadioApp.cpp or claim device-side logging.
Behavioral execution is covered by bounded_log_test.cpp through CTest.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / 'PocketJazzRadio' / 'BoundedLog.h').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests' / 'pcm' / 'CMakeLists.txt').read_text(encoding='utf-8')


class BoundedLogFormatContract(unittest.TestCase):
    def test_explicit_bounded_append_api_and_overflow_safe_subtraction(self):
        self.assertIn('namespace log {', HEADER)
        self.assertRegex(HEADER, r'bool\s+canAppend\(size_t\s+existingBytes,\s*size_t\s+incomingBytes,')
        self.assertIn('kDefaultLogLimit = 16384', HEADER)
        self.assertIn('incomingBytes <= capBytes - existingBytes', HEADER)
        self.assertIn('existingBytes <= capBytes', HEADER)

    def test_clean_tsv_utf8_and_nul_bounded_api(self):
        self.assertRegex(HEADER, r'size_t\s+cleanTsvField\(const char\*\s+src,\s*char\*\s+dest,\s*size_t\s+destBytes\)')
        self.assertIn('kMaxFieldInputBytes = 512', HEADER)
        self.assertIn("dest[out] = '\\0';", HEADER)
        self.assertIn('forbiddenCodepoint(cp)', HEADER)
        self.assertIn('span > kMaxFieldInputBytes - pos', HEADER)
        self.assertIn('if (size > (destBytes - 1) - out) break;', HEADER)

    def test_no_autologging_sd_dependency_or_app_source_reference(self):
        for name in ('SD.open', 'SD.write', 'File ', 'Preferences', 'Arduino.h', 'WiFi.h', 'printf(', 'fopen(', 'std::vector'):
            with self.subTest(name=name):
                self.assertNotIn(name, HEADER)
        self.assertIn('bounded_sd_log_format_escape_and_cap', CMAKE)
        self.assertIn('../bounded_log_test.cpp', CMAKE)


if __name__ == '__main__':
    unittest.main()
