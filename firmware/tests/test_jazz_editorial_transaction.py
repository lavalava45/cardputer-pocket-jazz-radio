"""Failure injection for the five-file Jazz Notes integration transaction."""
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from jazz_editorial_transaction import publish, recover


class EditorialTransactionTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.transaction = self.root / "inflight"
        self.files = [self.root / f"canonical-{i}.txt" for i in range(4)]
        self.files.append(self.root / "accepted-journal.json")
        self.previous = {}
        self.proposed = {}
        for i, path in enumerate(self.files):
            self.previous[path] = f"OLD-{i}\n".encode()
            self.proposed[path] = f"NEW-{i}\n".encode()
            path.write_bytes(self.previous[path])

    def assert_old_complete(self):
        for path in self.files:
            self.assertEqual(path.read_bytes(), self.previous[path], path.name)
        self.assertFalse(self.transaction.exists())

    def test_exception_between_replacements_restores_all_five_targets(self):
        def fail(index, _):
            if index == 1:
                raise OSError("injected disk failure after second replacement")

        with self.assertRaisesRegex(OSError, "injected disk failure"):
            publish(self.transaction, self.proposed, after_replace=fail)
        self.assert_old_complete()

    def test_process_crash_between_replacements_is_recovered_on_next_launch(self):
        code = (
            "import os,sys\n"
            "from pathlib import Path\n"
            "sys.path.insert(0,sys.argv[1])\n"
            "from jazz_editorial_transaction import publish\n"
            "r=Path(sys.argv[2]); targets=[r/f'canonical-{i}.txt' for i in range(4)]"
            "+[r/'accepted-journal.json']\n"
            "updates={t:f'NEW-{i}\\n'.encode() for i,t in enumerate(targets)}\n"
            "publish(r/'inflight',updates,after_replace=lambda i,p:os._exit(53) if i==2 else None)\n"
        )
        completed = subprocess.run(
            [sys.executable, "-c", code, str(ROOT / "tools"), str(self.root)],
            capture_output=True, text=True, check=False)
        self.assertEqual(completed.returncode, 53, completed.stderr)
        self.assertTrue(self.transaction.exists())
        self.assertTrue(recover(self.transaction))
        self.assert_old_complete()
        self.assertFalse(recover(self.transaction))

    def test_success_updates_all_five_and_cleans_transaction(self):
        publish(self.transaction, self.proposed)
        for path in self.files:
            self.assertEqual(path.read_bytes(), self.proposed[path])
        self.assertFalse(self.transaction.exists())

    def test_twelve_target_editorial_source_pin_index_and_header_failure_restores_all(self):
        # The source-QA editorial publication has 12 distinct output targets,
        # including newly created immutable SHA pins, generated index/header,
        # and its final acceptance receipt. Verify rollback for the *whole*
        # footprint, not just the older synthetic five-file scenario.
        existing = {self.root / f"canonical-{i:02d}.dat": f"BEFORE-{i}\n".encode()
                    for i in range(9)}
        new_files = [self.root / f"pinned-baseline-{i}.sha256.tsv" for i in range(3)]
        for file, blob in existing.items():
            file.write_bytes(blob)
        updates = {file: f"AFTER-{i}\n".encode()
                   for i, file in enumerate(list(existing) + new_files)}
        self.assertEqual(len(updates), 12)
        def interrupt(index, _):
            if index == 8:
                raise OSError("source-QA injected failure after nine of twelve replacements")
        with self.assertRaisesRegex(OSError, "source-QA injected failure"):
            publish(self.transaction, updates, after_replace=interrupt)
        for file, blob in existing.items():
            self.assertEqual(file.read_bytes(), blob)
        for file in new_files:
            self.assertFalse(file.exists(), file)
        self.assertFalse(self.transaction.exists())

    def test_twelve_target_process_crash_and_next_run_recovers_complete_batch(self):
        code = (
            "import os,sys\nfrom pathlib import Path\n"
            "sys.path.insert(0,sys.argv[1])\n"
            "from jazz_editorial_transaction import publish\n"
            "r=Path(sys.argv[2]); files=[r/f'qa-{i:02}.dat' for i in range(12)]\n"
            "updates={p:f'AFTER-{i}\\n'.encode() for i,p in enumerate(files)}\n"
            "publish(r/'inflight',updates,after_replace=lambda i,p:os._exit(61) if i==7 else None)\n"
        )
        originals = [self.root / f"qa-{i:02}.dat" for i in range(12)]
        for i, file in enumerate(originals[:9]):
            file.write_bytes(f"BEFORE-{i}\n".encode())
        completed = subprocess.run([sys.executable, "-c", code, str(ROOT / "tools"),
                                    str(self.root)], capture_output=True, text=True, check=False)
        self.assertEqual(completed.returncode, 61, completed.stderr)
        self.assertTrue(self.transaction.exists())
        self.assertTrue(recover(self.transaction))
        for i, file in enumerate(originals):
            if i < 9:
                self.assertEqual(file.read_bytes(), f"BEFORE-{i}\n".encode())
            else:
                self.assertFalse(file.exists())
        self.assertFalse(self.transaction.exists())

    def test_new_acceptance_journal_is_rolled_back_after_crash(self):
        journal = self.files[-1]
        journal.unlink()
        def fail(index, _):
            if index == 4:
                raise OSError("failure after creating acceptance journal")
        with self.assertRaisesRegex(OSError, "acceptance journal"):
            publish(self.transaction, self.proposed, after_replace=fail)
        for path in self.files[:-1]:
            self.assertEqual(path.read_bytes(), self.previous[path])
        self.assertFalse(journal.exists())
        self.assertFalse(self.transaction.exists())

    def test_incomplete_preparation_can_recover_without_touching_data(self):
        self.transaction.mkdir()
        (self.transaction / "staged-000.bin").write_bytes(b"unpublished")
        self.assertTrue(recover(self.transaction))
        self.assert_old_complete()

    def test_missing_backup_aborts_without_deleting_remaining_backups(self):
        code = (
            "import os,sys\nfrom pathlib import Path\n"
            "sys.path.insert(0,sys.argv[1])\n"
            "from jazz_editorial_transaction import publish\n"
            "r=Path(sys.argv[2]); ps=[r/f'canonical-{i}.txt' for i in range(4)]"
            "+[r/'accepted-journal.json']\n"
            "publish(r/'inflight',{p:b'changed' for p in ps},"
            "after_replace=lambda i,p:os._exit(54) if i==0 else None)\n"
        )
        run = subprocess.run([sys.executable, "-c", code, str(ROOT / "tools"),
                              str(self.root)], capture_output=True, check=False)
        self.assertEqual(run.returncode, 54)
        (self.transaction / "backup-000.bin").unlink()
        with self.assertRaisesRegex(RuntimeError, "Missing/invalid rollback copy"):
            recover(self.transaction)
        self.assertTrue(self.transaction.exists())

    def _two_process_race(self, stage):
        # Both actors are real OS processes. A holds the OS lock while
        # preparing OR after replacing two canonical files. B starts
        # recover() and must remain blocked until A is forcibly killed.
        a_code = (
            "import pathlib,sys,time\n"
            "sys.path.insert(0,sys.argv[1])\n"
            "from jazz_editorial_transaction import publish\n"
            "r=pathlib.Path(sys.argv[2]); stage=sys.argv[3]\n"
            "paths=[r/f'canonical-{i}.txt' for i in range(4)]"
            "+[r/'accepted-journal.json']\n"
            "new={p:('NEW-%d\\n'%i).encode() for i,p in enumerate(paths)}\n"
            "def pause(marker):\n"
            " (r/'a-holds-lock').write_text(marker)\n"
            " time.sleep(60)\n"
            "publish(r/'inflight',new,before_journal=(lambda f:pause('preparing')) "
            "if stage=='preparing' else None,"
            "after_replace=(lambda i,p:pause('partial') if i==1 else None) "
            "if stage=='partial' else None)\n"
        )
        b_code = (
            "import pathlib,sys\n"
            "sys.path.insert(0,sys.argv[1])\n"
            "from jazz_editorial_transaction import recover\n"
            "r=pathlib.Path(sys.argv[2]); recover(r/'inflight')\n"
            "(r/'b-recovered').write_text('done')\n"
        )
        a = subprocess.Popen([sys.executable, "-c", a_code, str(ROOT / "tools"),
                              str(self.root), stage], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        b = None
        try:
            deadline = time.monotonic() + 8
            while not (self.root / "a-holds-lock").exists():
                if a.poll() is not None:
                    raise AssertionError(f"Publisher died: {a.communicate()}")
                if time.monotonic() > deadline:
                    raise AssertionError("Publisher never reached locked critical stage")
                time.sleep(0.03)
            b = subprocess.Popen([sys.executable, "-c", b_code, str(ROOT / "tools"),
                                  str(self.root)], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
            time.sleep(0.35)
            self.assertIsNone(b.poll(), "Second process must block on OS lock")
            self.assertFalse((self.root / "b-recovered").exists())
            self.assertTrue(self.transaction.exists(),
                            "Concurrent recovery must not destroy active backups")
            a.kill()  # OS itself must release the lock, no orderly finally block.
            a.communicate(timeout=8)
            _, err = b.communicate(timeout=12)
            self.assertEqual(b.returncode, 0, err.decode(errors="replace"))
            self.assertTrue((self.root / "b-recovered").exists())
            self.assert_old_complete()
        finally:
            if a.poll() is None:
                a.kill()
                a.communicate(timeout=5)
            if b is not None and b.poll() is None:
                b.kill()
                b.communicate(timeout=5)

    def test_two_processes_owner_killed_during_backup_preparation(self):
        self._two_process_race("preparing")

    def test_two_processes_owner_killed_between_canonical_replacements(self):
        self._two_process_race("partial")


if __name__ == "__main__":
    unittest.main()
