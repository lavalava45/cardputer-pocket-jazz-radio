"""Source regression guard for STOP, station switch and reconnect lifetime.

Static contract only: real heap reclamation and HTTPS reconnect on Cardputer
require device telemetry. This test intentionally does not claim hardware I/O.
"""
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
WORKER = APP.split("void audioWorker(void*) {", 1)[1].split("void loadPeekConfig()", 1)[0]
TLS = (ROOT / "PocketJazzRadio/SecureICYSource.cpp").read_text(encoding="utf-8")


class AudioRetryCleanupContract(unittest.TestCase):
    def test_station_switch_and_stop_release_previous_transport(self):
        dispatch = WORKER.split("if (xQueueReceive(audioQueue, &cmd, 0) == pdTRUE)", 1)[1].split(
            "if (!decoder && sourceOffline)", 1)[0]
        self.assertIn("if (changed) {\n        cleanup();", dispatch)
        self.assertIn("if (!desired || !url[0]) {", dispatch)
        self.assertIn("if (decoder || src || buffer) cleanup();", dispatch)
        self.assertIn("if (superseded())", dispatch)
        self.assertIn("strlcpy(url, cmd.url, sizeof(url));", dispatch)

    def test_failed_open_releases_socket_ring_and_codec_before_retry(self):
        cleanup = WORKER.split("auto cleanup = [&]()", 1)[1].split(
            "auto superseded =", 1)[0]
        order = ["decoder->stop()", "delete decoder", "buffer->close()",
                 "delete buffer", "src->close()", "delete src",
                 "codec.release()", "free(ring)"]
        self.assertEqual([cleanup.index(s) for s in order],
                         sorted(cleanup.index(s) for s in order))
        live = WORKER.split("if (!decoder && !sourceOffline)", 1)[1].split(
            "if (superseded()) {\n      // The UI may", 1)[0]
        self.assertIn("if (!decoder) {", live)
        self.assertIn("cleanup();\n        audioState.store(ERROR_STREAM);", live)
        self.assertIn("retryAt = millis() + 8000;", live)
        self.assertIn("ring = static_cast<uint8_t*>(malloc(ringCapacity));", live)
        self.assertLess(live.index("cleanup();\n        audioState.store(ERROR_STREAM);"),
                        live.index("retryAt = millis() + 8000;"))
        self.assertIn("http_.end();", TLS)
        self.assertIn("tls_.stop();", TLS)

    def test_chunked_and_icy_are_removed_before_mp3_buffer(self):
        self.assertIn('chunked_ = transferEncoding == "chunked";', TLS)
        self.assertLess(TLS.index("const uint32_t got = readBody("),
                        TLS.index("uint32_t SecureICYSource::read(void *data"))
        self.assertIn("chunkDecoder_.consumeFraming", TLS)
        self.assertIn("chunkDecoder_.consumePayload", TLS)
        self.assertIn("untilMetadata_ -= got", TLS)
        self.assertIn("consumeMetadata(nonBlocking)", TLS)


if __name__ == "__main__":
    unittest.main()
