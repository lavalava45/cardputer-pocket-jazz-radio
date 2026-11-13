"""A live finite HTTP body remains readable until its actual Content-Length.

Cross-check the real decoder's three shortage branches, not merely the pure
policy unit test. The latter runs with active assert under MSVC Release NDEBUG.
"""
import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioGeneratorMP3.cpp').read_text(encoding='utf-8')
POLICY = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioLiveStallPolicy.h').read_text(encoding='utf-8')
TEST = (ROOT / 'tests/live_mp3_stall_policy_test.cpp').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests/pcm/CMakeLists.txt').read_text(encoding='utf-8')


class FiniteLiveShortReadContract(unittest.TestCase):
    def test_all_three_mp3_short_read_branches_supply_source_position(self):
        self.assertEqual(3, len(re.findall(
            r'liveStallPolicy\.shouldWait\(millis\(\),\s*file->isOpen\(\),\s*'
            r'file->getSize\(\),\s*file->getPos\(\)\)', SRC)))

    def test_true_eof_only_when_finite_position_reaches_size(self):
        self.assertIn('reportedPos >= reportedSize', POLICY)
        self.assertIn('if (!timeoutMs_ || !sourceOpen || atFiniteEnd)', POLICY)
        self.assertIn('assert(policy.shouldWait(5001, true, 100000, 200));', TEST)
        self.assertIn('assert(!policy.shouldWait(9001, true, 100000, 100000));', TEST)
        self.assertIn('#undef NDEBUG', TEST)
        self.assertIn('live_mp3_temporary_empty_vs_finite_eof_bounded_wraparound', CMAKE)


if __name__ == '__main__':
    unittest.main()
