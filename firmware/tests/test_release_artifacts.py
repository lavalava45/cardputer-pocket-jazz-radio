"""Validate SD release files read-only; never invoke esptool or flash anything."""

import hashlib
import re
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAX_APP_BYTES = 0x150000
HARD_MAX_APP_BYTES = 0x400000


class ReleaseArtifactContract(unittest.TestCase):
    def test_packager_never_deletes_published_versions_or_flashes(self):
        script = (ROOT / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("if (Test-Path -LiteralPath $release)", script)
        self.assertNotIn("ForceOverwriteRelease", script)
        self.assertNotRegex(script, r"(?i)Remove-Item\s+-LiteralPath\s+\$release\b")
        self.assertNotRegex(script, r"(?im)^\s*&?\s*(?:esptool(?:\.py)?|idf\.py|arduino-cli)\s+(?:.*\s)?(?:write_flash|erase_flash|upload|flash)\b")

    def test_versioned_release_snapshot_integrity(self):
        releases = ROOT / "dist" / "releases"
        for release in sorted(releases.iterdir()) if releases.exists() else []:
            if not release.is_dir() or release.name.startswith(".staging-"):
                continue
            # v0.1.1 predates source snapshots and must remain untouched.
            if release.name == "0.1.1":
                continue
            with self.subTest(version=release.name):
                source = release / "source"
                manifest = source / "SHA256-SOURCES.txt"
                self.assertTrue(manifest.is_file(), "New release requires source hash manifest")
                records = [line for line in manifest.read_text(encoding="utf-8-sig").splitlines()
                           if line and not line.startswith("#")]
                listed = set()
                for line in records:
                    match = re.fullmatch(r"([a-f0-9]{64})  ([\w.\-/]+)", line)
                    self.assertIsNotNone(match, f"Malformed source SHA256 entry: {line}")
                    digest, relative = match.groups()
                    self.assertNotIn("..", Path(relative).parts)
                    self.assertNotIn(relative, listed)
                    listed.add(relative)
                    item = source.joinpath(*relative.split("/"))
                    self.assertTrue(item.is_file(), f"Missing snapshot file: {relative}")
                    self.assertEqual(hashlib.sha256(item.read_bytes()).hexdigest(), digest)
                required = {
                    "PocketJazzRadio/PocketJazzRadioApp.cpp",
                    "PocketJazzRadio/PocketJazzRadioApp.h",
                    "PocketJazzRadio/PocketJazzRadio.ino",
                    "PocketJazzRadio/AudioOutputM5Speaker.h",
                    "README.md", "ROADMAP.md", "tools/build.ps1",
                }
                self.assertTrue(required.issubset(listed))
                if release.name >= '0.7y':
                    self.assertIn('arduino-sketchbook/libraries/ESP8266Audio/src/AudioICYIncremental.h', listed)
                    self.assertIn('tests/icy_incremental_stream_test.cpp', listed)
                    self.assertIn('tests/radio_buffer_level_policy_test.cpp', listed)
                if release.name >= '0.7z':
                    self.assertIn('PocketJazzRadio/SafeHttpsUpgrade.h', listed)
                    self.assertIn('tests/safe_https_upgrade_test.cpp', listed)
                    self.assertIn('docs/HTTPS-TRACE-V07X.md', listed)
                    self.assertIn('docs/RELEASE-V07Y-VERIFICATION.md', listed)
                # A release is an immutable historical snapshot. Requiring it
                # to contain *today's* added compilation units would falsely
                # fail every past release whenever development moves forward.
                # Verify exact hashes for the files captured in THAT release.
                for sketch_path in (p for p in listed if p.startswith("PocketJazzRadio/")):
                    self.assertTrue((source / sketch_path).is_file())
                self.assertTrue(any(path.startswith("playlists/") and path.endswith(".m3u") for path in listed))
                self.assertEqual(listed, {p.relative_to(source).as_posix()
                                          for p in source.rglob("*") if p.is_file()
                                          and p.name != "SHA256-SOURCES.txt"}, "Manifest must cover every source file")
                archive = release / f"PocketJazzRadio-v{release.name}-Launcher3-SD.zip"
                with zipfile.ZipFile(archive) as z:
                    self.assertFalse(any(n.replace("\\", "/").startswith("source/") for n in z.namelist()))
                    self.assertIsNone(z.testzip())
                text = (release / "SHA256-MANIFEST.txt").read_text(encoding="utf-8-sig")
                for artifact, digest in (
                    (archive, hashlib.sha256(archive.read_bytes()).hexdigest()),
                    (release / "firmware" / f"PocketJazzRadio-v{release.name}-app-only.bin", None),
                ):
                    self.assertTrue(artifact.is_file())
                    expected = digest or hashlib.sha256(artifact.read_bytes()).hexdigest()
                    self.assertIn(expected, text, "Release manifest must match artifact SHA256")
                image = release / "firmware" / f"PocketJazzRadio-v{release.name}-app-only.bin"
                size = image.stat().st_size
                self.assertGreater(size, 512)
                self.assertLessEqual(size, HARD_MAX_APP_BYTES)
                self.assertEqual(image.read_bytes()[0], 0xE9)
                # The old v0.7x / v0.7z release scripts did enforce an
                # unverified 0x150000 threshold. Historical frozen manifests
                # remain exactly as published, but new versions do not inherit
                # that obsolete packaging policy.
                old_policy = "$maxAppBytes = 0x150000" in (
                    source / "tools" / "build.ps1"
                ).read_text(encoding="utf-8-sig")
                if old_policy and size > MAX_APP_BYTES:
                    self.assertIn("NOT CLEARED FOR INSTALL", text,
                                  "Oversize BIN must not be marked installable")
                    with zipfile.ZipFile(archive) as zipped:
                        self.assertIn("NOT CLEARED FOR INSTALL",
                                      zipped.read("README-INSTALL.txt").decode("utf-8"))

    def test_every_versioned_app_binary_has_valid_header_and_size(self):
        binaries = sorted((ROOT / "dist" / "release" / "firmware").glob("PocketJazzRadio-v*-app-only.bin"))
        if not binaries:
            self.skipTest("No packaged release yet")
        for path in binaries:
            with self.subTest(path=path.name):
                content = path.read_bytes()
                self.assertGreater(len(content), 512)
                self.assertLessEqual(len(content), HARD_MAX_APP_BYTES)
                self.assertEqual(content[0], 0xE9, "ESP32 application header magic")

    def test_sd_zip_contains_only_app_binary_and_station_files(self):
        packages = sorted((ROOT / "dist").glob("PocketJazzRadio-v*-Launcher3-SD.zip"))
        if not packages:
            self.skipTest("No SD release ZIP yet")
        for package in packages:
            with self.subTest(zip=package.name), zipfile.ZipFile(package) as archive:
                names = [n.replace("\\", "/") for n in archive.namelist() if not n.endswith("/")]
                binary_names = [n for n in names if n.lower().endswith(".bin")]
                self.assertEqual(len(binary_names), 1, "ZIP must contain exactly one firmware BIN")
                self.assertRegex(binary_names[0], r"(?:^|/)firmware/PocketJazzRadio-v[\w.\-]+-app-only\.bin$")
                self.assertTrue(any(n.endswith(".m3u") for n in names))
                self.assertTrue(any(n.endswith("README-INSTALL.txt") for n in names))
                for name in names:
                    self.assertNotIn("..", Path(name).parts)
                    self.assertFalse(re.search(r"(?i)(merged|bootloader|partition|\.elf|\.map)", name), name)
                self.assertEqual(archive.testzip(), None, "ZIP CRC mismatch")

    def test_v01_manifest_hash_and_original_binary(self):
        path = ROOT / "dist" / "release" / "firmware" / "PocketJazzRadio-v0.1.0-app-only.bin"
        manifest = ROOT / "dist" / "BUILD-MANIFEST.txt"
        if not path.is_file() or not manifest.is_file():
            self.skipTest("v0.1.0 reference files unavailable")
        text = manifest.read_text(encoding="utf-8-sig")
        size = re.search(r"App-only binary length:\s*(\d+)", text)
        digest = re.search(r"App-only SHA256:\s*([a-fA-F0-9]{64})", text)
        self.assertIsNotNone(size)
        self.assertIsNotNone(digest)
        self.assertEqual(path.stat().st_size, int(size.group(1)))
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest.group(1).lower())


if __name__ == "__main__":
    unittest.main()
