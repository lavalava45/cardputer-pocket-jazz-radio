"""Non-invasive source progress is latched at FIRST_STALL for manual SD export."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
HEAP = (ROOT / "PocketJazzRadio/HeapLifecycleProfile.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "PocketJazzRadio/AudioSourceProgress.h").read_text(encoding="utf-8")

class SourceProgressWiring(unittest.TestCase):
    def test_no_extra_network_reads_or_second_audio_task(self):
        worker = APP.split("void audioWorker(void*) {", 1)[1].split("void loadPeekConfig()", 1)[0]
        self.assertIn("pocketjazz::AudioSourceProgress sourceProgress;", worker)
        self.assertIn("sourceProgress.observe(millis(), src->getPos());", worker)
        self.assertIn("sourceProgress.reset(millis());", worker)
        self.assertNotIn("sourceProgress.read(", worker)
        self.assertNotIn("sourceProgress.recv(", worker)
        self.assertIn("SetCooperativeSourceRead(true)", worker)
        self.assertIn("SetLiveStallTimeoutMs(4000)", worker)
        self.assertNotIn("setInsecure(", worker)
        self.assertNotIn("sourceProgress", APP.split("void audioWorker(void*) {", 1)[0])
    def test_first_stall_source_idle_survives_later_retries(self):
        self.assertIn("sourceBytes = 0, sourceIdleMs = 0, sourceMaxGapMs = 0", HEAP)
        self.assertIn("firstStallEver_ = points_[index]", HEAP)
        self.assertIn("snap.sourceBytes = sourceOffline ? 0 : sourceProgress.bytes();", APP)
        self.assertIn("source_idle_ms=%lu", APP)
        self.assertIn("source_gap_max_ms=%lu", APP)
        self.assertIn("source_bytes=%lu", APP)
        self.assertIn("sourceProgress.maximumGapMs()", APP)
        self.assertIn("lastPosition_ = sourcePosition", SOURCE)
        self.assertNotIn("malloc", SOURCE)

if __name__ == "__main__":
    unittest.main()
