"""Finite ICY Content-Length includes metadata, unlike MP3 getPos.

Check that both real source owners account for all returned HTTP entity bytes
without changing their MP3 position, the incremental parser, or TLS security.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PLAIN = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceICYStream.cpp').read_text(encoding='utf-8')
PLAIN_H = (ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceICYStream.h').read_text(encoding='utf-8')
SECURE = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
POLICY = (ROOT / 'PocketJazzRadio/HttpBodyAudioSize.h').read_text(encoding='utf-8')
TEST = (ROOT / 'tests/http_body_audio_size_test.cpp').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests/pcm/CMakeLists.txt').read_text(encoding='utf-8')


class FiniteIcyBodyProgressIntegration(unittest.TestCase):
    def test_plain_http_increments_payload_not_chunk_framing(self):
        self.assertIn('bodyBytesConsumed_ = 0;', PLAIN)
        self.assertEqual(2, PLAIN.count('bodyBytesConsumed_ += static_cast<uint32_t>(got);'))
        self.assertIn('uint32_t AudioFileSourceICYStream::getSize()', PLAIN)
        self.assertIn('projectedMp3Size(size, bodyBytesConsumed_', PLAIN)
        self.assertIn('virtual uint32_t getSize() override', PLAIN_H)
        self.assertIn('metadata.acceptMetadata(scratch, got, title)', PLAIN)
        self.assertIn('pos += static_cast<int>(got);', PLAIN)

    def test_https_metadata_and_payload_share_http_body_accounting(self):
        self.assertIn('bodyBytesConsumed_ = 0;', SECURE)
        self.assertEqual(2, SECURE.count('bodyBytesConsumed_ += static_cast<uint32_t>(got);'))
        self.assertIn('projectedMp3Size(length_, bodyBytesConsumed_, position_, 0)', SECURE)
        self.assertIn('metadataLeft_ -= got;', SECURE)
        self.assertIn('position_ += got;', SECURE)
        self.assertIn('tls_.setCACert(caPem_)', SECURE)
        self.assertNotIn('setInsecure(', SECURE)

    def test_finite_fragmented_metadata_and_nondeadline_eof_are_real_ctest(self):
        self.assertIn('bodyBytesConsumed >= totalBody', POLICY)
        self.assertIn('const int32_t contentLength = 25;', TEST)
        self.assertIn('body += 7; // fragmented metadata', TEST)
        self.assertIn('body += 9;', TEST)
        self.assertIn('assert(!stall.shouldWait(1400, true,', TEST)
        self.assertIn('#undef NDEBUG', TEST)
        self.assertIn('finite_icy_http_body_including_metadata_stops_without_4s_pause', CMAKE)


if __name__ == '__main__':
    unittest.main()
