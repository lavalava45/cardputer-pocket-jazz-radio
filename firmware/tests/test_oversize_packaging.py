"""Exercise the real PowerShell packager inside a disposable project copy.

No Arduino compilation and no device access: images here are deliberately synthetic.
The test MUST never touch the genuine dist/releases tree.
"""
import pathlib
import hashlib
import re
import shutil
import subprocess
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
POWERSHELL = shutil.which("powershell") or shutil.which("pwsh")


@unittest.skipUnless(POWERSHELL, "PowerShell not available")
class OversizePackaging(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="pocketjazz-size-gate-")
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name)
        paths = [
            "tools/build.ps1",
            "LICENSE", "COPYRIGHT-NOTICES.md",
            "arduino-sketchbook/libraries/ESP8266Audio/LICENSE",
            "arduino-sketchbook/libraries/M5Unified/LICENSE",
            "arduino-sketchbook/libraries/M5GFX/LICENSE",
            "README.md",
            "docs/KEYS.md", "docs/CHANGELOG.md", "docs/build-release-notes.md",
            "PocketJazzRadio/PocketJazzRadioApp.cpp",
            "PocketJazzRadio/PocketJazzRadioApp.h",
            "PocketJazzRadio/PocketJazzRadio.ino",
            "PocketJazzRadio/SecureICYSource.h",
            "PocketJazzRadio/SecureICYSource.cpp",
            "PocketJazzRadio/RadioTrustedRoots.h",
            "PocketJazzRadio/AudioOutputM5Speaker.h",
            "playlists/01-essentials.m3u",
        ]
        for relative in paths:
            dest = self.root / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, dest)
        # The developer's live ROADMAP is intentionally not published.
        # The isolated synthetic package still needs a harmless placeholder,
        # because the real packager includes a ROADMAP in local source archives.
        (self.root / "ROADMAP.md").write_text(
            "# Synthetic test fixture; no private project plan.\n",
            encoding="utf-8",
        )
        self.bin = self.root / "dist" / "build" / "PocketJazzRadio.ino.bin"
        self.bin.parent.mkdir(parents=True)

    def write_image(self, size):
        with self.bin.open("wb") as f:
            f.write(b"\xe9")
            f.seek(size - 1)
            f.write(b"\x00")

    def fixture_test(self, passing=True):
        """Only copy one small test into the fixture: never recurse into this harness."""
        tests = self.root / "tests"
        tests.mkdir(exist_ok=True)
        name = "test_pass.py" if passing else "test_fail.py"
        (tests / name).write_text(
            "import unittest\n"
            "class FixtureTest(unittest.TestCase):\n"
            "    def test_fixture(self):\n"
            + ("        self.assertEqual(2 + 2, 4)\n" if passing else
               "        self.assertEqual(2 + 2, 5)\n"),
            encoding="utf-8",
        )
        cmake = tests / "pcm"
        cmake.mkdir(exist_ok=True)
        (tests / "fixture_smoke.cpp").write_text(
            "#ifdef NDEBUG\n#undef NDEBUG\n#endif\n"
            "#include <cassert>\n"
            "int main(){assert(2+2==4); return 0;}\n",
            encoding="utf-8",
        )
        (cmake / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(PocketJazzPackagingFixture LANGUAGES CXX)\n"
            "enable_testing()\n"
            "add_executable(fixture_smoke ../fixture_smoke.cpp)\n"
            "add_test(NAME fixture_smoke COMMAND fixture_smoke)\n",
            encoding="utf-8",
        )

    def assert_no_published_release(self):
        self.assertFalse((self.root / "dist" / "releases" / "99.98").exists())
        releases = self.root / "dist" / "releases"
        if releases.exists():
            self.assertFalse(list(releases.glob(".staging-*")), "Failed package left a staging directory")

    def assert_report_verified(self, release):
        report = release / "TEST-REPORT.txt"
        self.assertTrue(report.is_file())
        content = report.read_text(encoding="utf-8-sig")
        self.assertIn("Ran 1 test", content)
        self.assertIn("OK", content)
        manifest = (release / "SHA256-MANIFEST.txt").read_text(encoding="utf-8-sig")
        match = re.search(r"(?m)^TEST-REPORT\.txt SHA256: ([0-9a-f]{64})\s*$", manifest)
        self.assertIsNotNone(match, "Missing digest for TEST-REPORT.txt")
        self.assertEqual(hashlib.sha256(report.read_bytes()).hexdigest(), match.group(1))
        host = release / "HOST-CTEST-REPORT.txt"
        self.assertTrue(host.is_file())
        host_hash = re.search(r"(?m)^HOST-CTEST-REPORT\.txt SHA256: ([0-9a-f]{64})\s*$", manifest)
        self.assertIsNotNone(host_hash, "Missing CTest report hash")
        self.assertEqual(hashlib.sha256(host.read_bytes()).hexdigest(), host_hash.group(1))
        self.assertIn("100% tests passed out of 1", host.read_text(encoding="utf-8-sig"))
        build_inputs = release / "BUILD-INPUTS.txt"
        self.assertTrue(build_inputs.is_file())
        input_hash = re.search(r"(?m)^BUILD-INPUTS\.txt SHA256: ([0-9a-f]{64})\s*$", manifest)
        self.assertIsNotNone(input_hash)
        self.assertEqual(hashlib.sha256(build_inputs.read_bytes()).hexdigest(), input_hash.group(1))
        arduino = release / 'ARDUINO-COMPILE-REPORT.txt'
        self.assertTrue(arduino.is_file())
        match = re.search(r'(?m)^ARDUINO-COMPILE-REPORT\.txt SHA256: ([0-9a-f]{64})\s*$', manifest)
        self.assertIsNotNone(match)
        self.assertEqual(hashlib.sha256(arduino.read_bytes()).hexdigest(), match.group(1))
        self.assertIn('NOT COMPILED (PackageOnly', arduino.read_text(encoding='utf-8-sig'))

    def invoke(self, *extra):
        return subprocess.run(
            [POWERSHELL, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
             str(self.root / "tools" / "build.ps1"), "-PackageOnly",
             "-AllowUnverifiedPackageOnly", "-Version", "99.98", *extra],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            timeout=90,
        )

    def test_unverified_old_threshold_does_not_block_archiving(self):
        self.fixture_test(passing=True)
        self.write_image(0x150000 + 100)
        result = self.invoke()
        self.assertEqual(0, result.returncode, result.stdout)
        release = self.root / "dist" / "releases" / "99.98"
        self.assertTrue((release / "firmware" / "PocketJazzRadio-v99.98-app-only.bin").is_file())
        self.assertIn("UNVERIFIED BUILD SOURCE", (release / "SHA256-MANIFEST.txt").read_text())
        self.assert_report_verified(release)

    def test_override_archives_but_warns_not_installable(self):
        self.fixture_test(passing=True)
        self.write_image(0x150000 + 100)
        result = self.invoke("-PackageUnverifiedPManSize")
        self.assertEqual(0, result.returncode, result.stdout)
        release = self.root / "dist" / "releases" / "99.98"
        manifest = (release / "SHA256-MANIFEST.txt").read_text()
        self.assertIn("NOT CLEARED FOR INSTALL", manifest)
        archive = next(release.glob("*.zip"))
        with zipfile.ZipFile(archive) as z:
            names = z.namelist()
            self.assertFalse(any('\\' in name for name in names),
                             'ZIP entries must use portable forward slashes')
            self.assertIn('firmware/PocketJazzRadio-v99.98-app-only.bin', names)
            self.assertIn('PocketJazz/01-essentials.m3u', names)
            self.assertEqual(z.read('LICENSE'), (ROOT / 'LICENSE').read_bytes())
            self.assertEqual(z.read('COPYRIGHT-NOTICES.md'),
                             (ROOT / 'COPYRIGHT-NOTICES.md').read_bytes())
            self.assertFalse(any(n.startswith("source/") for n in names))
            readme = z.read("README-INSTALL.txt").decode("utf-8")
            self.assertIn("NOT CLEARED FOR INSTALL", readme)
            self.assertNotIn("compare app size with free APP space", readme)
        self.assertTrue((release / "source" / "SHA256-SOURCES.txt").exists())
        archived_source = release / 'source'
        for required in ('LICENSE', 'COPYRIGHT-NOTICES.md',
                         'arduino-sketchbook/libraries/ESP8266Audio/LICENSE',
                         'arduino-sketchbook/libraries/M5Unified/LICENSE',
                         'arduino-sketchbook/libraries/M5GFX/LICENSE'):
            self.assertEqual((ROOT / required).read_bytes(),
                             (archived_source / required).read_bytes(), required)
        with zipfile.ZipFile(archive) as z:
            for notice in ('LICENSE', 'COPYRIGHT-NOTICES.md'):
                self.assertEqual(z.read(notice),
                                 (archived_source / notice).read_bytes(),
                                 f'{notice} must come from the verified source snapshot')
        with tempfile.TemporaryDirectory(prefix='pocketjazz-zip-compat-') as temp:
            with zipfile.ZipFile(archive) as z:
                z.extractall(temp)
            self.assertTrue((pathlib.Path(temp) / 'firmware' /
                             'PocketJazzRadio-v99.98-app-only.bin').is_file())
            self.assertTrue((pathlib.Path(temp) / 'PocketJazz' /
                             '01-essentials.m3u').is_file())
        self.assertIn("UNVERIFIED BUILD SOURCE", manifest)
        self.assert_report_verified(release)
        self.assertNotEqual(0, self.invoke("-PackageUnverifiedPManSize").returncode,
                            "Published release must be immutable")

    def test_hard_cap_blocks_even_explicit_override(self):
        self.write_image(0x400000 + 1)
        result = self.invoke("-PackageUnverifiedPManSize")
        self.assertNotEqual(0, result.returncode)
        self.assertFalse((self.root / "dist" / "releases" / "99.98").exists())

    def test_missing_root_license_blocks_release(self):
        self.write_image(4096)
        self.fixture_test(passing=True)
        (self.root / 'LICENSE').unlink()
        result = self.invoke()
        self.assertNotEqual(0, result.returncode, result.stdout)
        self.assertIn('Required full original GPL-3 license text missing or incomplete',
                      result.stdout)
        self.assert_no_published_release()

    def test_missing_upstream_license_blocks_release(self):
        self.write_image(4096)
        self.fixture_test(passing=True)
        (self.root / 'arduino-sketchbook/libraries/ESP8266Audio/LICENSE').unlink()
        result = self.invoke()
        self.assertNotEqual(0, result.returncode, result.stdout)
        self.assertIn('Required redistributed dependency license absent', result.stdout)
        self.assert_no_published_release()

    def test_missing_tests_directory_blocks_package(self):
        self.write_image(4096)
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("tests/ directory is absent", result.stdout)
        self.assert_no_published_release()

    def test_empty_tests_directory_blocks_package(self):
        self.write_image(4096)
        (self.root / "tests").mkdir()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("No test_*.py", result.stdout)
        self.assert_no_published_release()

    def test_non_test_python_file_does_not_satisfy_gate(self):
        self.write_image(4096)
        tests = self.root / "tests"
        tests.mkdir()
        (tests / "helper.py").write_text("x = 1\n", encoding="utf-8")
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assert_no_published_release()

    def test_zero_discovered_tests_blocks_package(self):
        self.write_image(4096)
        tests = self.root / "tests"
        tests.mkdir()
        (tests / "test_empty.py").write_text("# no test functions\n", encoding="utf-8")
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("no tests were executed", result.stdout)
        self.assert_no_published_release()

    def test_intentionally_failing_test_blocks_package(self):
        self.write_image(4096)
        self.fixture_test(passing=False)
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Host-side tests failed", result.stdout)
        self.assert_no_published_release()

    def test_passing_test_publishes_report_with_verified_sha256(self):
        self.write_image(4096)
        self.fixture_test(passing=True)
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stdout)
        release = self.root / "dist" / "releases" / "99.98"
        self.assert_report_verified(release)
        self.assertTrue((release / "source" / "tests" / "test_pass.py").is_file())
        self.assertFalse((release / "source" / "tests" / "test_oversize_packaging.py").exists())

    def test_missing_cpp_suite_blocks_package(self):
        self.write_image(4096)
        self.fixture_test(passing=True)
        (self.root / "tests" / "pcm" / "CMakeLists.txt").unlink()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Required C++ host test suite is absent", result.stdout)
        self.assert_no_published_release()

    def test_zero_cpp_tests_blocks_package(self):
        self.write_image(4096)
        self.fixture_test(passing=True)
        (self.root / "tests" / "pcm" / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.16)\nproject(empty LANGUAGES CXX)\n"
            "enable_testing()\n", encoding="utf-8")
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Host C++ tests failed or no test was executed", result.stdout)
        self.assert_no_published_release()

    def test_failing_cpp_test_blocks_package(self):
        self.write_image(4096)
        self.fixture_test(passing=True)
        (self.root / "tests" / "fixture_smoke.cpp").write_text(
            "#ifdef NDEBUG\n#undef NDEBUG\n#endif\n"
            "#include <cassert>\nint main(){assert(false); return 0;}\n",
            encoding="utf-8")
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Host C++ tests failed", result.stdout)
        self.assert_no_published_release()


if __name__ == "__main__":
    unittest.main()
