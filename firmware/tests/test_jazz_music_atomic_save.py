"""Windows-friendly atomic MusicBrainz snapshot writes (no production writes)."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build_jazz_music_evidence as music


class MusicSnapshotAtomicWrite(unittest.TestCase):
    @staticmethod
    def temp_snapshot_path(target):
        return target.with_name(f"{target.name}.{os.getpid()}.tmp")

    def test_transient_windows_replace_lock_retries_without_corrupting_source(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "jazz-music-evidence.json"
            target.write_bytes(b"existing snapshot\n")
            original_replace = Path.replace
            attempts = []

            def temporarily_locked(src, dest):
                attempts.append((str(src), str(dest)))
                if len(attempts) <= 3:
                    self.assertEqual(target.read_bytes(), b"existing snapshot\n")
                    raise PermissionError(5, "sharing violation")
                return original_replace(src, dest)

            with patch.object(music, "OUTPUT", target), \
                    patch.object(Path, "replace", temporarily_locked), \
                    patch.object(music.time, "sleep") as sleep:
                music.save({"z": {"status": "pending_mb"},
                            "a": {"status": "checked_mb"}})
            self.assertEqual(len(attempts), 4)
            self.assertEqual(sleep.call_count, 3)
            self.assertFalse(self.temp_snapshot_path(target).exists())
            self.assertEqual(json.loads(target.read_text("utf-8")),
                             {"a": {"status": "checked_mb"},
                              "z": {"status": "pending_mb"}})
            self.assertTrue(target.read_bytes().endswith(b"\n"))

    def test_permanent_lock_retains_original_and_recoverable_temp_json(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "jazz-music-evidence.json"
            target.write_bytes(b"old valid snapshot\n")

            def permanently_locked(_src, _dest):
                raise PermissionError(5, "sharing violation")

            with patch.object(music, "OUTPUT", target), \
                    patch.object(Path, "replace", permanently_locked), \
                    patch.object(music.time, "sleep"):
                with self.assertRaises(PermissionError):
                    music.save({"artist": {"status": "checked_mb"}})
            self.assertEqual(target.read_bytes(), b"old valid snapshot\n")
            pending = self.temp_snapshot_path(target)
            self.assertEqual(json.loads(pending.read_text("utf-8")),
                             {"artist": {"status": "checked_mb"}})

    def test_single_writer_lock_prevents_overlapping_read_modify_write(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "jazz-music-evidence.json"
            with patch.object(music, "OUTPUT", target):
                with music.exclusive_writer():
                    # A second opener cannot use a stale snapshot while the
                    # first process still owns the cross-process OS lock.
                    with self.assertRaisesRegex(RuntimeError, "writer is active"):
                        with music.exclusive_writer():
                            self.fail("Second writer must never be admitted")
                with music.exclusive_writer():
                    pass  # Lock released; no stale permanent denial.


if __name__ == "__main__":
    unittest.main()
