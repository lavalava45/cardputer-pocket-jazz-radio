"""Host checks of the pinned GitHub manifest and updater transaction contract.

The fault model uses the same pending/committed markers and rename sequence;
firmware compilation is a separate required check.
"""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
# Unified GitHub checkout: <repo>/firmware/tests (manifest at <repo>/).
# Development workspace: cardputer-adv/apps/pocket-jazz-radio/tests with
# cardputer-adv/packages/cardputer-pocket-jazz-stations as a sibling package.
# Standalone firmware source ZIPs have neither external location.
PACKAGE_CANDIDATES = (
    ROOT.parent,
    ROOT.parents[1] / "packages" / "cardputer-pocket-jazz-stations",
)


def find_catalog_package():
    for directory in PACKAGE_CANDIDATES:
        if (directory / "manifest.json").is_file():
            return directory
    return None


PACKAGE = find_catalog_package()
UPDATER = (ROOT / "PocketJazzRadio" / "CatalogUpdater.cpp").read_text(encoding="utf-8")
NAMES = (
    "01-essentials.m3u",
    "02-modern-jazz.m3u",
    "03-mellow-bossa.m3u",
    "05-swing-vocals.m3u",
    "06-latin-soul-jazz.m3u",
)


def recover(root):
    pending, committed = root / ".catalog-pending", root / ".catalog-committed"
    backup, stage = root / ".catalog-backup", root / ".catalog-stage"
    if committed.exists() and committed.read_bytes() == b"committed\n":
        for file in backup.glob("*") if backup.exists() else ():
            file.unlink()
    elif pending.exists():
        for name in NAMES:
            old = backup / name
            if old.exists():
                (root / name).unlink(missing_ok=True)
                old.rename(root / name)
    if backup.exists() and not list(backup.iterdir()):
        backup.rmdir()
    if stage.exists():
        for file in stage.glob("*"):
            file.unlink()
        stage.rmdir()
    if committed.exists():
        committed.unlink()
    if pending.exists():
        pending.unlink()


def finalize(root, activated):
    """Filesystem-level fault model of the public finalize API."""
    pending = root / ".catalog-pending"
    backup = root / ".catalog-backup"
    if not pending.exists() or pending.read_bytes() != b"pending\n":
        return "RecoveryFailed"
    if not activated:
        recover(root)
        return "ActivationFailed"
    if not all((root / name).exists() and (backup / name).exists()
               for name in NAMES):
        return "RecoveryFailed"
    (root / ".catalog-committed").write_bytes(b"committed\n")
    recover(root)
    return "Ok"


def staged_catalog(root):
    """ReadyToActivate transaction: new live files, backups and pending."""
    backup = root / ".catalog-backup"
    backup.mkdir()
    for name in NAMES:
        (backup / name).write_bytes(b"old:" + name.encode())
        (root / name).write_bytes(b"new:" + name.encode())
    (root / ".catalog-pending").write_bytes(b"pending\n")


class CatalogUpdaterTests(unittest.TestCase):
    @staticmethod
    def require_external_catalog():
        if PACKAGE is None:
            raise unittest.SkipTest(
                "External GitHub catalog manifest is not included in standalone firmware source"
            )
        return PACKAGE

    def test_manifest_is_compatible_with_firmware(self):
        package = self.require_external_catalog()
        manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual(manifest["repository"], "lavalava45/cardputer-pocket-jazz-radio")
        self.assertEqual(tuple(x["path"] for x in manifest["files"]),
                         tuple("catalog/" + x for x in NAMES))
        self.assertEqual(manifest["playlists"], 5)
        self.assertEqual(sum(x["stations"] for x in manifest["files"]), manifest["stations"])
        self.assertLessEqual(manifest["stations"], 64)
        self.assertLessEqual((package / "manifest.json").stat().st_size, 4096)
        for item in manifest["files"]:
            data = (package / item["path"]).read_bytes()
            self.assertEqual(len(data), item["bytes"])
            self.assertEqual(hashlib.sha256(data).hexdigest(), item["sha256"])
            self.assertLessEqual(len(data), 32768)
            self.assertEqual(sum(line.startswith((b"http://", b"https://"))
                                 for line in data.splitlines()), item["stations"])
            self.assertFalse(any(line.startswith(b"http://") for line in data.splitlines()))
            self.assertTrue(all(len(line) <= 512 for line in data.splitlines()))

    def test_bundled_firmware_playlists_are_valid(self):
        # Source ZIPs ship these files even without the catalog repository.
        directory = ROOT / "playlists"
        for name in NAMES:
            with self.subTest(name=name):
                content = (directory / name).read_bytes()
                self.assertTrue(content.startswith(b"#EXTM3U"))
                self.assertLessEqual(len(content), 32768)
                self.assertTrue(all(len(line) <= 512 for line in content.splitlines()))
                self.assertTrue(any(line.startswith(b"https://") for line in content.splitlines()))
                self.assertFalse(any(line.startswith(b"http://") for line in content.splitlines()))

    def test_no_personal_file_targeted_and_manual_gate(self):
        self.assertNotIn('"04-my-stations.m3u"', UPDATER)
        self.assertIn("updateCatalogManually(bool audioStopped, bool sdMounted)", UPDATER)
        self.assertIn("WiFi.status() != WL_CONNECTED", UPDATER)
        self.assertIn("setCACert(kRadioTrustedRootsPem)", UPDATER)
        self.assertIn("HTTPC_DISABLE_FOLLOW_REDIRECTS", UPDATER)
        self.assertNotIn("setInsecure(", UPDATER)
        self.assertIn("validateM3u(candidate, count, true)", UPDATER)

    def test_noop_does_not_rewrite_matching_catalog(self):
        body = UPDATER.split("CatalogUpdateResult updateCatalogManually(", 1)[1]
        self.assertIn("bool alreadyCurrent = true;", body)
        self.assertIn("File installed = SD.open(live(kNames[i]), FILE_READ);", body)
        self.assertIn("const bool same = shaMatches(installed, entries[i]);", body)
        self.assertIn("if (alreadyCurrent) return CatalogUpdateResult::NoChange;", body)
        self.assertLess(body.index("if (alreadyCurrent) return CatalogUpdateResult::NoChange;"),
                        body.index("if (!SD.mkdir(kStage))"))
        self.assertIn("count != entries[i].stations", UPDATER)
        self.assertIn('journal.print("pending\\n") == strlen("pending\\n")', UPDATER)
        self.assertIn('committed.print("committed\\n") == strlen("committed\\n")', UPDATER)

    def test_power_loss_at_every_rename_recovers_originals(self):
        for interrupt_after in range(11):
            with self.subTest(interrupt_after=interrupt_after):
                with tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp)
                    backup = root / ".catalog-backup"
                    stage = root / ".catalog-stage"
                    backup.mkdir()
                    stage.mkdir()
                    for name in NAMES:
                        (root / name).write_bytes(b"old:" + name.encode())
                        (stage / name).write_bytes(b"new:" + name.encode())
                    (root / ".catalog-pending").write_bytes(b"pending\n")
                    operations = 0
                    for name in NAMES:
                        if operations == interrupt_after:
                            break
                        (root / name).rename(backup / name)
                        operations += 1
                        if operations == interrupt_after:
                            break
                        (stage / name).rename(root / name)
                        operations += 1
                    recover(root)
                    for name in NAMES:
                        self.assertEqual((root / name).read_bytes(), b"old:" + name.encode())

    def test_commit_marker_preserves_all_new_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            backup = root / ".catalog-backup"
            stage = root / ".catalog-stage"
            backup.mkdir()
            stage.mkdir()
            for name in NAMES:
                (backup / name).write_bytes(b"old")
                (root / name).write_bytes(b"new")
            (root / ".catalog-pending").write_bytes(b"pending\n")
            (root / ".catalog-committed").write_bytes(b"committed\n")
            recover(root)
            self.assertTrue(all((root / name).read_bytes() == b"new" for name in NAMES))
            self.assertFalse((root / ".catalog-committed").exists())

    def test_ready_to_activate_never_commits_before_finalization(self):
        header = (ROOT / "PocketJazzRadio" / "CatalogUpdater.h").read_text(encoding="utf-8")
        update = UPDATER.split("CatalogUpdateResult updateCatalogManually(", 1)[1].split(
            "CatalogUpdateResult finalizeCatalogUpdate(", 1
        )[0]
        finish = UPDATER.split("CatalogUpdateResult finalizeCatalogUpdate(", 1)[1]
        self.assertIn("ReadyToActivate", header)
        self.assertIn("ActivationFailed", header)
        self.assertIn("finalizeCatalogUpdate(bool activated)", header)
        self.assertIn("outcome = CatalogUpdateResult::ReadyToActivate;", update)
        self.assertNotIn('SD.open(kCommitted', update)
        self.assertNotIn('SD.remove(kPending)', update)
        self.assertIn('committed.print("committed\\n")', finish)
        self.assertIn('markerContains(kCommitted, "committed\\n")', finish)
        self.assertIn("rollbackPending()", finish)
        self.assertIn('if (!SD.exists(live(name)) || !SD.exists(path(kBackup, name)))', finish)

    def test_failed_activation_restores_old_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            staged_catalog(root)
            self.assertEqual(finalize(root, False), "ActivationFailed")
            for name in NAMES:
                self.assertEqual((root / name).read_bytes(), b"old:" + name.encode())
            self.assertFalse((root / ".catalog-pending").exists())
            self.assertFalse((root / ".catalog-backup").exists())

    def test_successful_activation_commits_only_after_explicit_ack(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            staged_catalog(root)
            self.assertTrue((root / ".catalog-pending").exists())
            self.assertFalse((root / ".catalog-committed").exists())
            self.assertTrue((root / ".catalog-backup").exists())
            self.assertEqual(finalize(root, True), "Ok")
            for name in NAMES:
                self.assertEqual((root / name).read_bytes(), b"new:" + name.encode())
            self.assertFalse((root / ".catalog-backup").exists())
            self.assertFalse((root / ".catalog-pending").exists())

    def test_reboot_while_ready_rolls_back_without_activation_ack(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            staged_catalog(root)
            recover(root)
            for name in NAMES:
                self.assertEqual((root / name).read_bytes(), b"old:" + name.encode())

    def test_short_or_empty_committed_marker_cannot_discard_backups(self):
        self.assertIn('markerContains(kCommitted, "committed\\n")', UPDATER)
        self.assertIn("file.size() != length", UPDATER)
        for marker in (b"", b"commit", b"committed", b"committed\nX"):
            with self.subTest(marker=marker), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                staged_catalog(root)
                (root / ".catalog-committed").write_bytes(marker)
                recover(root)
                for name in NAMES:
                    self.assertEqual((root / name).read_bytes(), b"old:" + name.encode())

    def test_malformed_commit_during_partial_backup_cleanup_fails_closed(self):
        # Actual SD recoverCatalogUpdate() must not silently accept a mixed
        # catalog when a committed marker is damaged AFTER cleanup started.
        recovery = UPDATER.split("CatalogUpdateResult recoverCatalogUpdate()", 1)[1].split(
            "CatalogUpdateResult updateCatalogManually(", 1
        )[0]
        self.assertIn('if (SD.exists(kPending) && SD.exists(kBackup))', recovery)
        self.assertIn('if (!SD.exists(path(kBackup, name)))', recovery)
        self.assertLess(recovery.index('if (!SD.exists(path(kBackup, name)))'),
                        recovery.rindex('if (!SD.remove(kCommitted))'))

    def test_manifest_dynamic_buffers_are_destroyed_before_tls_file_downloads(self):
        body = UPDATER.split("CatalogUpdateResult updateCatalogManually(", 1)[1].split(
            "bool catalogActivationMatches(", 1
        )[0]
        begin = body.index('Entry entries[5]{};')
        lifetime = body[begin:body.index('bool alreadyCurrent = true;', begin)]
        self.assertIn('String body;', lifetime)
        self.assertIn('StreamString input;', lifetime)
        self.assertIn('return CatalogUpdateResult::InvalidManifest;\n  }\n', lifetime)
        self.assertLess(lifetime.index('StreamString input;'), lifetime.rfind('}'))
        self.assertNotIn('body = String();', body)

    def test_partial_journal_failure_can_leave_only_empty_backup_directory(self):
        # A failed SD.open(kPending) has not renamed a live playlist yet.
        # It must not make subsequent boot recovery fail forever.
        code = UPDATER.split("bool removeEmptyBackupDirectory()", 1)[1].split(
            "bool markerContains(", 1
        )[0]
        self.assertIn("File entry = dir.openNextFile();", code)
        self.assertIn("return empty && SD.rmdir(kBackup);", code)
        recovery = UPDATER.split("CatalogUpdateResult recoverCatalogUpdate()", 1)[1].split(
            "CatalogUpdateResult updateCatalogManually(", 1
        )[0]
        self.assertIn("if (!removeEmptyBackupDirectory())", recovery)
        update = UPDATER.split("CatalogUpdateResult updateCatalogManually(", 1)[1].split(
            "bool catalogActivationMatches(", 1
        )[0]
        self.assertIn("if (!removeEmptyBackupDirectory() || !removeStage())", update)
        self.assertIn("if (SD.exists(kPending)", update)

    def test_interrupted_activation_does_not_finalize_incomplete_rename(self):
        for missing in NAMES:
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                staged_catalog(root)
                (root / ".catalog-backup" / missing).unlink()
                self.assertEqual(finalize(root, True), "RecoveryFailed")
                self.assertFalse((root / ".catalog-committed").exists())

    def test_short_download_and_corrupted_bytes_cannot_pass_manifest_hash(self):
        # The truncation/integrity invariant is independent of the presence
        # of an external manifest; use bundled M3U payloads for source ZIPs.
        for name in NAMES:
            original = (ROOT / "playlists" / name).read_bytes()
            expected_bytes = len(original)
            expected_hash = hashlib.sha256(original).hexdigest()
            short = original[:-1]
            tampered = bytearray(original)
            tampered[-1] ^= 1
            self.assertNotEqual(len(short), expected_bytes)
            self.assertNotEqual(hashlib.sha256(short).hexdigest(), expected_hash)
            self.assertNotEqual(hashlib.sha256(tampered).hexdigest(), expected_hash)
        self.assertIn("written != (size_t)expected", UPDATER)
        self.assertIn("shaMatches(candidate, entries[i])", UPDATER)

    def test_download_failure_before_pending_keeps_original_catalog(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / ".catalog-stage"
            stage.mkdir()
            for name in NAMES:
                (root / name).write_bytes(b"original")
            (stage / NAMES[0]).write_bytes(b"incomplete")
            # Network failure occurs during staging; the pending marker was
            # never created, therefore only disposable staging is removed.
            recover(root)
            self.assertTrue(all((root / name).read_bytes() == b"original" for name in NAMES))
            self.assertFalse(stage.exists())


if __name__ == "__main__":
    unittest.main()
