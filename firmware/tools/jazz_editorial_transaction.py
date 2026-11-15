"""Crash-recoverable multi-file editorial publishing, without touching SD.

Each os.replace is atomic, but a collection is not. Persist every pre-image
and its verified checksum in a transaction journal before any replacement.
An interrupted attempt is rolled back at the beginning of the next run.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import threading
import time
import uuid
from contextlib import contextmanager
from pathlib import Path

if os.name == "nt":
    import msvcrt
else:
    import fcntl

_LOCAL_LOCKS = {}
_LOCAL_GUARD = threading.Lock()
_HELD = threading.local()


@contextmanager
def transaction_lock(folder: Path):
    """OS-held, process-wide interlock; also serializes threads.

    The lock lives NEXT TO the transaction directory, not inside it, and
    is intentionally never unlinked: deleting a locked inode permits split
    locks between processes. The OS releases the held lock on process death.
    Re-entrant for the calling thread so an integrator can protect both its
    source reads and its nested publish()/recover() calls.
    """
    folder = Path(folder).resolve()
    folder.parent.mkdir(parents=True, exist_ok=True)
    lock_path = folder.parent / (folder.name + ".lock")
    with _LOCAL_GUARD:
        local = _LOCAL_LOCKS.setdefault(str(lock_path), threading.RLock())
    with local:
        held = getattr(_HELD, "locks", None)
        if held is None:
            held = {}
            _HELD.locks = held
        if lock_path in held:
            held[lock_path] += 1
            try:
                yield
            finally:
                held[lock_path] -= 1
            return
        with lock_path.open("a+b") as handle:
            if os.name == "nt":
                handle.seek(0)
                # Lock a nonempty byte region; the file itself persists.
                if handle.seek(0, os.SEEK_END) == 0:
                    handle.write(b"\0")
                    handle.flush()
                    os.fsync(handle.fileno())
                handle.seek(0)
                deadline = time.monotonic() + 120
                while True:
                    try:
                        msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise TimeoutError(f"Editorial transaction lock busy: {lock_path}")
                        time.sleep(0.05)
            else:
                fcntl.flock(handle.fileno(), fcntl.LOCK_EX)
            held[lock_path] = 1
            try:
                yield
            finally:
                del held[lock_path]
                if os.name == "nt":
                    handle.seek(0)
                    msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
                else:
                    fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


def digest(blob: bytes) -> str:
    return hashlib.sha256(blob).hexdigest()


def sync_dir(directory: Path) -> None:
    if os.name != "nt":
        fd = os.open(directory, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def durable_create(path: Path, blob: bytes) -> None:
    with path.open("xb") as output:
        output.write(blob)
        output.flush()
        os.fsync(output.fileno())
    sync_dir(path.parent)


def replace_bytes(target: Path, blob: bytes) -> None:
    """Stage in the destination directory, then atomically replace."""
    temporary = target.with_name(f".{target.name}.jazz-{uuid.uuid4().hex}.tmp")
    try:
        durable_create(temporary, blob)
        os.replace(temporary, target)
        sync_dir(target.parent)
    finally:
        temporary.unlink(missing_ok=True)


def write_journal(folder: Path, manifest: dict) -> None:
    replace_bytes(folder / "journal.json",
                  (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8"))


def _recover_locked(folder: Path) -> bool:
    """Idempotent recovery from an interrupted, prepared transaction."""
    folder = Path(folder)
    if not folder.exists():
        return False
    journal_path = folder / "journal.json"
    if not journal_path.is_file():
        # Publication has not begun: the prepared journal is written BEFORE
        # the first canonical replacement. Only expendable staged files exist.
        shutil.rmtree(folder)
        sync_dir(folder.parent)
        return True
    manifest = json.loads(journal_path.read_text("utf-8"))
    if manifest.get("schema") != 1 or manifest.get("state") not in ("prepared", "committed"):
        raise RuntimeError("Unrecognized transaction journal; refusing changes")
    entries = manifest["entries"]
    if manifest["state"] == "prepared":
        # Check EVERY backup first. An interrupted rollback can then be run again.
        for entry in entries:
            if entry["before"] is not None:
                backup = folder / entry["backup"]
                if not backup.is_file() or digest(backup.read_bytes()) != entry["before"]:
                    raise RuntimeError(f"Missing/invalid rollback copy: {entry['target']}")
        for entry in entries:
            target = Path(entry["target"])
            if entry["before"] is None:
                target.unlink(missing_ok=True)
                sync_dir(target.parent)
            elif not target.is_file() or digest(target.read_bytes()) != entry["before"]:
                replace_bytes(target, (folder / entry["backup"]).read_bytes())
        if any((Path(e["target"]).exists() if e["before"] is None
                else not Path(e["target"]).is_file() or
                digest(Path(e["target"]).read_bytes()) != e["before"]) for e in entries):
            raise RuntimeError("Rollback verification failed; backups retained")
    else:
        for entry in entries:
            target = Path(entry["target"])
            if not target.is_file() or digest(target.read_bytes()) != entry["after"]:
                raise RuntimeError(f"Committed target drift: {target}; backups retained")
    for entry in entries:
        Path(entry["staged"]).unlink(missing_ok=True)
    shutil.rmtree(folder)
    sync_dir(folder.parent)
    return True


def _publish_locked(folder: Path, changes: dict[Path, bytes], *,
                    after_replace=None, before_journal=None) -> None:
    """Commit a group of files with durable rollback for exceptions or crashes."""
    folder = Path(folder).resolve()
    if folder.exists():
        raise RuntimeError(f"Unrecovered transaction at {folder}")
    if not changes or any(not isinstance(blob, bytes) for blob in changes.values()):
        raise ValueError("Publish requires nonempty path-to-bytes mapping")
    ordered = sorted(((Path(k).resolve(), v) for k, v in changes.items()),
                     key=lambda row: str(row[0]))
    folder.mkdir(parents=True)
    sync_dir(folder.parent)
    entries = []
    try:
        for number, (target, blob) in enumerate(ordered):
            old = target.read_bytes() if target.exists() else None
            backup = f"backup-{number:03d}.bin" if old is not None else None
            if old is not None:
                durable_create(folder / backup, old)
            if target.parent.stat().st_dev != folder.stat().st_dev:
                raise ValueError(f"Transaction and target must share a volume: {target}")
            staged = folder / f"staged-{number:03d}.bin"
            durable_create(staged, blob)
            entries.append({"target": str(target), "before": digest(old) if old is not None else None,
                            "backup": backup, "after": digest(blob), "staged": str(staged)})
        if before_journal is not None:
            before_journal(folder)
        journal = {"schema": 1, "state": "prepared", "entries": entries}
        write_journal(folder, journal)
    except BaseException:
        if not (folder / "journal.json").exists():
            for entry in entries:
                Path(entry["staged"]).unlink(missing_ok=True)
            shutil.rmtree(folder)
        raise
    try:
        for index, (target, blob) in enumerate(ordered):
            os.replace(entries[index]["staged"], target)
            sync_dir(target.parent)
            if after_replace:
                after_replace(index, target)
        journal["state"] = "committed"
        write_journal(folder, journal)
    except BaseException:
        _recover_locked(folder)
        raise
    _recover_locked(folder)


def recover(folder: Path) -> bool:
    with transaction_lock(folder):
        return _recover_locked(Path(folder).resolve())


def publish(folder: Path, changes: dict[Path, bytes], *,
            after_replace=None, before_journal=None) -> None:
    with transaction_lock(folder):
        return _publish_locked(folder, changes, after_replace=after_replace,
                               before_journal=before_journal)
