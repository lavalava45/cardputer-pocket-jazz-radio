"""Production plain-HTTP ICY integration guards, not physical playback tests."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / 'arduino-sketchbook/libraries/ESP8266Audio/src'
SRC = (LIB / 'AudioFileSourceICYStream.cpp').read_text(encoding='utf-8')
HDR = (LIB / 'AudioFileSourceICYStream.h').read_text(encoding='utf-8')
INCR = (LIB / 'AudioICYIncremental.h').read_text(encoding='utf-8')
DEADLINE = (LIB / 'AudioICYDeadline.h').read_text(encoding='utf-8')
APP = (ROOT / 'PocketJazzRadio/PocketJazzRadioApp.cpp').read_text(encoding='utf-8')
SECURE = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
CMAKE = (ROOT / 'tests/pcm/CMakeLists.txt').read_text(encoding='utf-8')


class IcyNonBlockingContracts(unittest.TestCase):
    def test_state_is_persistent_and_bounded_not_stack_scoped(self):
        self.assertIn('audio_icy::IncrementalMetadata metadata;', HDR)
        self.assertIn('pocketjazz::HttpChunkedBody chunkDecoder;', HDR)
        self.assertIn('kMaxPerPoll = 256', INCR)
        self.assertIn('kTimeoutMs = 2500', INCR)
        self.assertIn('metadataLeft_ -= count;', INCR)
        self.assertIn('parser_.feed(bytes, count)', INCR)
        self.assertIn('parser_.finish(title)', INCR)
        self.assertIn('completeBlock();', INCR)
        self.assertIn('metadataTimedOut(startedAt_, now, kTimeoutMs)', INCR)
        self.assertIn('uint32_t limitMs = 2500', DEADLINE)
        for banned in ('delay(', 'yield(', 'malloc(', 'new ', 'String ', 'http.', 'WiFi.'):
            self.assertNotIn(banned, INCR)

    def test_decoder_nonblocking_callback_never_waits_for_metadata(self):
        method = SRC.split('uint32_t AudioFileSourceICYStream::readInternal(', 1)[1].split(
            'bool AudioFileSourceICYStream::close()', 1)[0]
        self.assertIn('if (nonBlock || !reconnectTries) return 0;', method)
        self.assertIn('if (metadata.timedOut(millis()))', method)
        self.assertIn('http.end(); // let owning audioWorker perform bounded reconnect', method)
        self.assertIn('if (metadata.waitingLength())', method)
        self.assertIn('metadata.acceptLength(metadataLength)', method)
        self.assertIn('metadata.acceptMetadata(scratch, got, title)', method)
        self.assertIn('if (title[0]) cb.md("StreamTitle", false, title);', method)
        self.assertIn('return delivered; //', method) if 'return delivered; //' in method else self.assertIn('return delivered;', method)
        self.assertNotIn('while (mdSize)', SRC)
        self.assertNotIn('goto retry', SRC)
        self.assertNotIn('while (stream->available()', SRC)

    def test_raw_http_chunked_is_stripped_prior_to_mp3_and_icy(self):
        self.assertIn('"Transfer-Encoding"', SRC)
        self.assertIn('chunked = encoding == "chunked"', SRC)
        self.assertIn('chunkDecoder.consumeFraming(', SRC)
        self.assertIn('chunkDecoder.consumePayload(', SRC)
        self.assertIn('metadata.acceptAudio(got)', SRC)
        self.assertIn('readBody(&metadataLength, 1)', SRC)
        self.assertIn('readBody(scratch, toRead)', SRC)
        self.assertIn('readBody(static_cast<uint8_t*>(data) + delivered, wanted)', SRC)
        self.assertIn('strncmp(url, "http://", 7) != 0', SRC)
        self.assertNotIn('setInsecure(', SRC)
        self.assertNotIn('https://', SRC.split('bool AudioFileSourceICYStream::open', 1)[1].split(
            'AudioFileSourceICYStream::~AudioFileSourceICYStream', 1)[0])

    def test_audio_worker_owns_reconnect_and_does_not_wait_in_metadata(self):
        worker = APP.split('void audioWorker(void*) {', 1)[1].split('void loadPeekConfig() {', 1)[0]
        buffer = (LIB / 'AudioFileSourceBuffer.cpp').read_text(encoding='utf-8')
        self.assertIn('buffer->SetCooperativeSourceRead(true)', worker)
        self.assertIn('decoder->SetLiveStallTimeoutMs(4000)', worker)
        self.assertIn('buffer->loop();', worker)
        # v0.7z2 routes the same nonblocking source pull through a timing
        # observer. The wrapper must still preserve the original opt-in API.
        self.assertIn('length = pullSource(buffer, buffSize, cooperativeSourceRead)', buffer)
        self.assertIn('audio_buffer::pull(src, data, len, cooperative)', buffer)
        self.assertIn('pullSource(&buffer[writePtr], bytesAvailMid, true)', buffer)
        self.assertIn('pullSource(&buffer[writePtr], bytesAvailEnd, true)', buffer)
        self.assertIn('pullSource(&buffer[writePtr], bytesAvailStart, true)', buffer)
        self.assertIn('vTaskPrioritySet(nullptr, 1);', worker)
        self.assertIn('vTaskPrioritySet(nullptr, 2);', worker)
        self.assertLess(worker.index('vTaskPrioritySet(nullptr, 1);'),
                        worker.index('bool opened = src && src->open(endpoint);'))
        self.assertLess(worker.index('const bool alive = opened && src->isOpen();'),
                        worker.index('vTaskPrioritySet(nullptr, 2);'))

    def test_secure_icy_also_caps_nonblocking_metadata_drain(self):
        self.assertIn('uint32_t pollBudget = nonBlocking ? 256u', SECURE)
        self.assertIn('if (!pollBudget) return false;', SECURE)
        self.assertIn('pollBudget -= got;', SECURE)
        self.assertIn('metadataStartedAt_ = millis()', SECURE)
        self.assertIn('reportFailure(-15, "ICY metadata stalled; closing stream")', SECURE)

    def test_behavioural_release_ctest_executes_its_assertions(self):
        self.assertIn('../icy_incremental_stream_test.cpp', CMAKE)
        self.assertIn('plain_http_icy_partial_chunked_metadata_timeout_ring_and_reconnect', CMAKE)
        self.assertIn('../radio_buffer_level_policy_test.cpp', CMAKE)
        for test in ('icy_incremental_stream_test.cpp', 'radio_buffer_level_policy_test.cpp'):
            self.assertIn('#undef NDEBUG', (ROOT / 'tests' / test).read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
