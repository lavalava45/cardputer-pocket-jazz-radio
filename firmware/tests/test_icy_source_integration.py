"""Static integration checks; real HTTPS/ICY decoder still requires ADV."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'PocketJazzRadio' / 'SecureICYSource.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'PocketJazzRadio' / 'SecureICYSource.h').read_text(encoding='utf-8')
APP = (ROOT / 'PocketJazzRadio' / 'PocketJazzRadioApp.cpp').read_text(encoding='utf-8')


class IcySourceIntegration(unittest.TestCase):
    def test_http_chunked_transfer_decoded_before_mp3_and_icy(self):
        # Raw FIP Jazz HTTP/1.1 uses Transfer-Encoding: chunked.
        # getStreamPtr() returns TCP bytes including chunk-size lines and CRLF.
        # Neither MP3 nor ICY metadata may see these framing bytes.
        parser = (ROOT / 'PocketJazzRadio' / 'HttpChunkedBody.h').read_text(encoding='utf-8')
        self.assertIn('class HttpChunkedBody', parser)
        self.assertIn('std::min<uint32_t>', SOURCE)
        self.assertIn('"Transfer-Encoding"', SOURCE)
        self.assertIn('chunked_ = transferEncoding == "chunked"', SOURCE)
        self.assertIn('chunkDecoder_.consumePayload', SOURCE)
        self.assertIn('chunkDecoder_.consumeFraming', SOURCE)
        self.assertIn('readBody(&lengthByte, 1)', SOURCE)
        self.assertIn('readBody(scratch, wanted)', SOURCE)
        self.assertIn('readBody(static_cast<uint8_t*>(data), len)', SOURCE)
        self.assertIn('reportFailure(-16, "Malformed HTTP chunked body; closing stream")', SOURCE)
        self.assertNotIn('setInsecure(', SOURCE)

    def test_secure_icy_uses_incremental_bounded_parser_not_per_byte_string(self):
        self.assertIn('#include "IcyMetadataText.h"', HEADER)
        self.assertIn('pocketjazz::IcyMetadataText metadataParser_', HEADER)
        self.assertIn('metadataParser_.reset()', SOURCE)
        self.assertIn('metadataParser_.feed(scratch, static_cast<size_t>(got))', SOURCE)
        self.assertIn('metadataParser_.finish(title)', SOURCE)
        self.assertIn('if (!metadataMalformed_ &&', SOURCE)
        self.assertIn('cb.md("StreamTitle", false, title)', SOURCE)
        self.assertNotIn('String metadata_', HEADER)
        self.assertNotIn('metadata_ +=', SOURCE)
        self.assertNotIn('metadata_.reserve(', SOURCE)
        self.assertNotIn('metadata_.substring(', SOURCE)

    def test_parser_rejects_lookalike_key_without_hiding_valid_later_title(self):
        helper = (ROOT / 'PocketJazzRadio' / 'IcyMetadataText.h').read_text(encoding='utf-8')
        suite = (ROOT / 'tests' / 'icy_metadata_text_test.cpp').read_text(encoding='utf-8')
        self.assertIn('state_ = c == \';\' ? SEEK : SKIP', helper)
        self.assertIn('StreamTitleExtra=', suite)
        self.assertIn('for (size_t split = 1; split <= prefixLookalike.size()', suite)
        self.assertIn('if (c == 0) state_ = PAD_DONE', helper)
        self.assertIn('dirtyAfterSemicolon', suite)

    def test_interval_is_strict_and_metadata_stall_budget_persists(self):
        self.assertIn('parseIcyMetaInterval(interval.c_str(), icyInterval_)', SOURCE)
        self.assertNotIn('interval.toInt()', SOURCE)
        self.assertIn('metadataStartedAt_ = millis()', SOURCE)
        self.assertIn('(uint32_t)(millis() - metadataStartedAt_) >= METADATA_TIMEOUT_MS', SOURCE)
        self.assertIn('reportFailure(-15, "ICY metadata stalled; closing stream")', SOURCE)
        self.assertRegex(SOURCE, r'reportFailure\(-15,[\s\S]*?close\(\);')
        self.assertIn('METADATA_TIMEOUT_MS = 2500', SOURCE)
        self.assertNotIn('setInsecure(', SOURCE)

    def test_callback_sanitizes_plain_http_and_secure_http_before_queue(self):
        callback = APP.split('void icyMetadataCB(', 1)[1].split('template <size_t N> void put(', 1)[0]
        self.assertIn('cleanTsvField(value, event.title, sizeof(event.title))', callback)
        self.assertIn('if (!event.title[0]) return;', callback)
        self.assertIn('xQueueOverwrite(metadataQueue, &event)', callback)
        self.assertNotIn('event.title[j++]', callback)
        self.assertNotIn('SD.open(', callback)
        self.assertNotIn('Display.', callback)


if __name__ == '__main__':
    unittest.main()
