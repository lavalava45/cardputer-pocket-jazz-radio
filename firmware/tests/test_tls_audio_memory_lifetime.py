"""Source contract: contiguous MP3 ring before TLS; split codec after prefill.

This is a source-level guard, NOT an ESP32 heap measurement or hardware test.
The deployed v0.7z3 exhibited MBEDTLS_ERR_SSL_ALLOC_FAILED (-0x7F00).
"""
import re
import unittest
from pathlib import Path


APP = (Path(__file__).resolve().parents[1] /
       "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
WORKSPACE = (Path(__file__).resolve().parents[1] /
             "PocketJazzRadio/Mp3CodecWorkspace.h").read_text(encoding="utf-8")
WORKER = APP.split("void audioWorker(void*) {", 1)[1].split(
    "void loadPeekConfig()", 1
)[0]


class AudioTlsMemoryLifetime(unittest.TestCase):
    def test_no_pinned_large_buffers_while_retrying(self):
        beginning = WORKER.split("auto cleanup = [&]()", 1)[0]
        self.assertIn("uint8_t* ring = nullptr;", beginning)
        self.assertIn("Mp3CodecWorkspace codec;", beginning)
        self.assertNotIn("malloc(STREAM_BYTES)", beginning)
        self.assertNotIn("codec.allocate()", beginning)

    def test_ring_precedes_tls_but_codec_follows_successful_prefill(self):
        live = WORKER.split("if (!decoder && !sourceOffline)", 1)[1]
        self.assertLess(live.index("ring = static_cast<uint8_t*>(malloc(ringCapacity))"),
                        live.index("src->open(endpoint)"))
        self.assertLess(live.index("src->open(endpoint)"),
                        live.index("codec.allocate()"))
        self.assertLess(live.index("hasPlayablePrefill"),
                        live.index("codec.allocate()"))
        self.assertIn("new (std::nothrow) AudioFileSourceBuffer(src, ring, ringCapacity)", live)
        self.assertIn("STREAM_BYTES_CONSTRAINED = 16384", APP)
        self.assertIn("STREAM_FULL_RING_MIN_HEAP", live)
        self.assertIn("audioRingCapacity.store(ringCapacity", live)
        self.assertIn("codec.createDecoder()", live)
        self.assertIn("audioFailureStage.store(5)", live)
        self.assertIn("audioFailureStage.store(codec.ready() ? 3 : 6)", live)

    def test_buffer_and_codec_released_only_after_audio_objects(self):
        cleanup = WORKER.split("auto cleanup = [&]()", 1)[1].split(
            "auto superseded =", 1
        )[0]
        order = ("decoder->stop()", "delete decoder", "buffer->close()",
                 "delete buffer", "src->close()", "delete src",
                 "codec.release()", "free(ring)")
        positions = [cleanup.index(item) for item in order]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("ring = nullptr", cleanup)

    def test_adaptive_ring_retains_codec_and_tls_identity(self):
        self.assertIn("constexpr int STREAM_BYTES = 32768;", APP)
        self.assertIn("constexpr int MP3_CODEC_BYTES = 29192;", APP)
        self.assertIn("ringCapacity = STREAM_BYTES_CONSTRAINED", WORKER)
        self.assertIn("if (!ring && ringCapacity == STREAM_BYTES)", WORKER)
        self.assertIn("ringCapacity = STREAM_BYTES", WORKER)
        self.assertIn("codec.release();", WORKER)
        self.assertIn("kRadioTrustedRootsPem", WORKER)
        self.assertNotIn("setInsecure(", WORKER)
        self.assertIn("decoder = codec.allocate() ? codec.createDecoder() : nullptr;", WORKER)
        self.assertLess(WORKSPACE.index("frame_ = malloc("),
                        WORKSPACE.index("synth_ = malloc("))
        self.assertIn("new (std::nothrow) AudioGeneratorMP3(", WORKSPACE)
        # Offline reads skip the HTTP ring but still obtain their MP3 codec.
        offline = WORKER.split("if (!decoder && sourceOffline)", 1)[1].split(
            "if (!decoder && !sourceOffline)", 1
        )[0]
        self.assertNotIn("malloc(STREAM_BYTES)", offline)


if __name__ == "__main__":
    unittest.main()
