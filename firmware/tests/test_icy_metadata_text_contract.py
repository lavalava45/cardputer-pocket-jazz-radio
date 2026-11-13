"""Source-only checks of the independent C++11 ICY parser; never loads App.cpp."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / 'PocketJazzRadio' / 'IcyMetadataText.h').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests' / 'pcm' / 'CMakeLists.txt').read_text(encoding='utf-8')


class IcyMetadataTextContract(unittest.TestCase):
    def test_incremental_fixed_memory_api(self):
        for exact in ('class IcyMetadataText', 'void reset()',
                      'bool feed(const uint8_t* bytes, size_t count)',
                      'bool feed(const char* bytes, size_t count)',
                      'bool finish(char (&out)[kTitleCapacity])',
                      'kMaxMetadataBytes = 4080', 'kTitleCapacity = 96',
                      'sizeof(IcyMetadataText) <= 256'):
            self.assertIn(exact, HEADER)

    def test_no_dynamic_allocation_and_strict_interval(self):
        self.assertIn('parseIcyMetaInterval(const char* raw, uint32_t& result)', HEADER)
        self.assertIn('parsed > (1048576u - digit) / 10u', HEADER)
        # icy-metaint: 0 explicitly means no metadata; rejecting zero was a
        # real defect fixed in the current ICY state machine.
        self.assertIn('if (!raw || !*raw) return true;', HEADER)
        self.assertIn('result = parsed;', HEADER)
        for forbidden in ('Arduino.h', 'String ', 'std::string', 'std::vector',
                          'malloc(', 'new ', 'SD.', 'WiFi.', 'HTTPClient', 'Serial.'):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, HEADER)

    def test_registered_release_assert_behavioral_test(self):
        self.assertIn('../icy_metadata_text_test.cpp', CMAKE)
        self.assertIn('icy_metadata_text_utf8_chunks_and_intervals', CMAKE)
        test = (ROOT / 'tests' / 'icy_metadata_text_test.cpp').read_text(encoding='utf-8')
        self.assertIn('#undef NDEBUG', test)
        self.assertIn('parseIcyMetaInterval', test)
        self.assertIn('static_assert(sizeof(IcyMetadataText) <= 256', test)


if __name__ == '__main__':
    unittest.main()
