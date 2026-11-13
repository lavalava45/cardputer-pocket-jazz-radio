"""Read-only gate for the opt-in Pocket Jazz/SDK compile-only sample."""
from pathlib import Path
import hashlib
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "tests/pcm_mirror_app_compile/pcm_mirror_app_compile.ino"
LEGACY = ROOT / "PocketJazzRadio/AudioOutputM5Speaker.h"
PINNED_SDK = ROOT.parent.parent / "audio-router-sdk/dist/releases/0.8e-host-only/audio-router-sdk-0.8e-host-only-source.zip"
PINNED_ZIP_SHA = "867bc067d73900cfcde1f41e8a0d97420bd1782bdc774a076bb822afbe7c6020"
PINNED_FILES = {
    "LICENSE": "1f05a3681ad4de9d18e0970d07dfeb804d25141825341cf4ef9f983d7737c95e",
    "include/cardputer_audio/AudioRouter.h": "747eb6fafb5cd01494aa0bc914b31c1e46b48fc3ac213ef51844f5af0c759d5a",
    "include/cardputer_audio/CardputerPcmMirror.h": "a57af4b468779e1a0feb32f08e0fb9504165b519aec2447a79da76b8782f4277",
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class PcmMirrorAppProbe(unittest.TestCase):
    def test_mirror_is_bound_to_immutable_sdk_source_with_mit_license(self):
        # In the live tree the immutable sibling SDK release is mandatory;
        # in a downloaded release/source snapshot, vendored exact headers and
        # their MIT license permit rebuilding without the sibling workspace.
        vendor = ROOT / "vendor/audio-router-sdk-0.8e-host-only"
        if PINNED_SDK.is_file():
            self.assertEqual(sha256(PINNED_SDK.read_bytes()), PINNED_ZIP_SHA)
            with zipfile.ZipFile(PINNED_SDK) as packaged:
                for name, expected in PINNED_FILES.items():
                    self.assertEqual(sha256(packaged.read('source/' + name)), expected)
                    if vendor.is_dir():
                        self.assertEqual((vendor / name).read_bytes(),
                                         packaged.read('source/' + name))
        else:
            self.assertTrue(vendor.is_dir(), "Missing both pinned SDK ZIP and release/source vendor snapshot")
            for name, expected in PINNED_FILES.items():
                self.assertEqual(sha256((vendor / name).read_bytes()), expected)

    def test_one_legacy_owner_and_nonowning_monitor_signature(self):
        probe = PROBE.read_text(encoding="utf-8")
        legacy = LEGACY.read_text(encoding="utf-8")
        self.assertIn('AudioOutputM5Speaker soleLegacySpeaker(&M5.Speaker, 0)', probe)
        self.assertIn('soleLegacySpeaker.setMonitor(compositeMonitor, nullptr)', probe)
        self.assertIn('monitorMirror.observeAccepted(acceptedStereo, frames,', probe)
        self.assertIn('void setMonitor(monitor_fn_t fn, void* arg)', legacy)
        self.assertIn('if (_monitor) { _monitor(_monitor_arg, queued ?', legacy)
        self.assertNotIn('soleLegacySpeaker.setup(', probe)
        self.assertNotIn('M5.Speaker.begin(', probe)
        self.assertNotIn('M5.Speaker.playRaw(', probe)
        self.assertNotIn('CardputerM5SpeakerSink', probe)

    def test_release_builder_actually_compiles_pinned_header_probe(self):
        script = (ROOT / 'tools/build.ps1').read_text(encoding='utf-8')
        self.assertIn(PINNED_ZIP_SHA, script)
        self.assertIn('sdkPinnedHeaderHashes', script)
        self.assertIn("Folder = 'tests/pcm_mirror_app_compile'", script)
        self.assertIn('Pinned SDK header changed during Arduino compilation', script)
        self.assertIn('SDK MIT license snapshot mismatch', script)


if __name__ == '__main__':
    unittest.main()
