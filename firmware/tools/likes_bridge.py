#!/usr/bin/env python3
"""Offline, read-only Pocket Jazz likes importer and manual match review.

No Spotify API, credentials, listeners, network requests, SD writes or music
downloads.  A person may export a COPY of liked.txt from the Cardputer SD and
explicitly run this tool on a PC. The original log is never modified.

Usage:
 python tools/likes_bridge.py ingest --source COPY/liked.txt --state bridge/likes.json
 python tools/likes_bridge.py export --state bridge/likes.json --output bridge/likes.html
 python tools/likes_bridge.py confirm --state bridge/likes.json --id <id> --url https://open.spotify.com/track/<22 chars>
 python tools/likes_bridge.py skip --state bridge/likes.json --id <id>
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import html
import json
import os
import re
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import quote, urlsplit

MAX_LOG_BYTES = 65536
MAX_STATE_BYTES = 2 * 1024 * 1024
# A valid firmware 64-KiB log can contain >800 short lines; do not reject it
# solely because listeners use short station/track names.
MAX_ROWS = 4096
SCHEMA_VERSION = 1
REPORT_MARKER = "pocketjazz-likes-report-v1"
MAX_REPORT_BYTES = 8 * 1024 * 1024
LOCK_SIGNATURE = b"POCKETJAZZ-LIKES-LOCK-v1\n"
TRACK_ID = re.compile(r"^[A-Za-z0-9]{22}$")
TIME = re.compile(r"^(?:time-unknown|\d{4}-\d{2}-\d{2}T\d{2}:\d{2}Z)$")


class BridgeError(ValueError):
    """Fail-closed input or state validation error."""


@contextmanager
def _state_guard(path: Path):
    """Serialize state transactions across *processes*, not just threads.

    Lock is a persistent, recognisable sidecar; NEVER delete it while another
    process could be waiting on its inode. Locking the byte AFTER the marker
    permits Windows to read the marker while a second writer owns the lock.
    A busy or unrecognised lock fails closed rather than losing confirmed IDs.
    """
    if path.is_symlink():
        raise BridgeError("refusing symlink state path")
    lock_path = path.with_name("." + path.name + ".pocketjazz.lock")
    if lock_path.is_symlink():
        raise BridgeError("refusing symlink lock path")
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    marker = LOCK_SIGNATURE + b"\0"
    if not lock_path.exists():
        try:
            with lock_path.open("xb") as init:
                init.write(marker)
                init.flush()
                os.fsync(init.fileno())
        except FileExistsError:
            pass  # another instance just created the same sidecar
    # Unbuffered I/O is essential on Windows: Python's buffered read may
    # prefetch the adjacent LOCKED sentinel byte and raise PermissionError.
    with lock_path.open("r+b", buffering=0) as handle:
        # On Windows msvcrt locks a byte range for *reading* as well as
        # writing; never read the protected sentinel byte just to verify the
        # sidecar identity, or contending processes get raw PermissionError.
        if (lock_path.stat().st_size != len(marker) or
                handle.read(len(LOCK_SIGNATURE)) != LOCK_SIGNATURE):
            raise BridgeError("unrecognised lock file; refusing to modify it")
        handle.seek(len(LOCK_SIGNATURE))
        try:
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except (OSError, BlockingIOError) as exc:
            raise BridgeError("state is locked by another process; retry later") from exc
        try:
            yield
        finally:
            handle.seek(len(LOCK_SIGNATURE))
            if os.name == "nt":
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


def _read_liked(path: Path) -> bytes:
    with path.open("rb") as f:
        raw = f.read(MAX_LOG_BYTES + 1)
    if len(raw) > MAX_LOG_BYTES:
        raise BridgeError("liked.txt exceeds firmware's 64-KiB limit")
    if raw and not raw.endswith(b"\n"):
        raise BridgeError("liked.txt has an incomplete final line; leaving state unchanged")
    return raw


def _rows(raw: bytes) -> list[tuple[str, str, str, str]]:
    try:
        content = raw.decode("utf-8", "strict")
    except UnicodeDecodeError as exc:
        raise BridgeError("liked.txt is not valid UTF-8; no changes made") from exc
    if content.startswith("\ufeff"):
        raise BridgeError("liked.txt contains an unexpected BOM")
    lines = content[:-1].split("\n") if content else []
    if len(lines) > MAX_ROWS:
        raise BridgeError("too many likes for bounded importer")
    result = []
    appearances: dict[str, int] = {}
    for number, line in enumerate(lines, 1):
        # The firmware emits LF; Windows copy tools may produce CRLF.
        # Splitting ONLY on LF also preserves other Unicode/control separators
        # for explicit rejection rather than silently accepting damaged rows.
        if line.endswith("\r"):
            line = line[:-1]
        cells = line.split("\t")
        if len(cells) != 3:
            raise BridgeError(f"liked.txt row {number}: expected 3 TSV cells")
        timestamp, station, title = cells
        if (not TIME.fullmatch(timestamp) or not station or not title or
                len(timestamp.encode("utf-8")) > 24 or
                len(station.encode("utf-8")) > 55 or
                len(title.encode("utf-8")) > 95):
            raise BridgeError(f"liked.txt row {number}: invalid/oversize field")
        if any(ord(c) < 32 or ord(c) == 127 or 0x80 <= ord(c) <= 0x9F
               or c in "\u2028\u2029"
               for c in timestamp + station + title):
            raise BridgeError(f"liked.txt row {number}: control character")
        # Preserve repeated listening events, but re-importing an appended log
        # must not make duplicates. Ordinal handles equal rows at unknown time.
        appearances[line] = appearances.get(line, 0) + 1
        digest = hashlib.sha256(
            (line + "\n#" + str(appearances[line])).encode("utf-8")
        ).hexdigest()[:24]
        result.append((digest, timestamp, station, title))
    return result


def _new_state() -> dict:
    return {"schema": SCHEMA_VERSION, "source_size": 0, "source_sha256":
            hashlib.sha256(b"").hexdigest(), "likes": []}


def _load_state(path: Path) -> dict:
    if not path.exists():
        return _new_state()
    if path.stat().st_size > MAX_STATE_BYTES:
        raise BridgeError("existing state exceeds 2 MiB; refusing to modify")
    try:
        state = json.loads(path.read_text(encoding="utf-8"))
    except (UnicodeError, ValueError) as exc:
        raise BridgeError("existing state is unreadable; refusing overwrite") from exc
    if (not isinstance(state, dict) or state.get("schema") != SCHEMA_VERSION or
            not isinstance(state.get("source_size"), int) or
            not isinstance(state.get("source_sha256"), str) or
            not isinstance(state.get("likes"), list)):
        raise BridgeError("unknown state schema; refusing overwrite")
    if (not (0 <= state["source_size"] <= MAX_LOG_BYTES) or
            not re.fullmatch(r"[0-9a-f]{64}", state["source_sha256"]) or
            len(state["likes"]) > MAX_ROWS):
        raise BridgeError("invalid state source length")
    ids: set[str] = set()
    for entry in state["likes"]:
        if (not isinstance(entry, dict) or
                not isinstance(entry.get("id"), str) or
                not re.fullmatch(r"[0-9a-f]{24}", entry["id"]) or
                not isinstance(entry.get("utc"), str) or
                not TIME.fullmatch(entry["utc"]) or
                not isinstance(entry.get("station"), str) or
                not isinstance(entry.get("title"), str) or
                entry.get("status") not in ("needs-review", "confirmed", "skipped") or
                entry["id"] in ids):
            raise BridgeError("malformed/duplicate state entry")
        if (not entry["station"] or not entry["title"] or
                len(entry["station"].encode("utf-8")) > 55 or
                len(entry["title"].encode("utf-8")) > 95 or
                any(ord(c) < 32 or ord(c) == 127 or
                    0x80 <= ord(c) <= 0x9F or c in "\u2028\u2029"
                    for c in entry["station"] + entry["title"])):
            raise BridgeError("malformed state metadata")
        if entry["status"] == "confirmed":
            try:
                if checked_track_url(entry.get("spotify_track_url", "")) != entry["spotify_track_url"]:
                    raise BridgeError("noncanonical saved track URL")
            except (TypeError, AttributeError) as exc:
                raise BridgeError("invalid confirmed track URL") from exc
        elif entry.get("spotify_track_url") is not None:
            raise BridgeError("nonconfirmed Like must not have a Spotify track URL")
        ids.add(entry["id"])
    return state


def _atomic_json(path: Path, obj: dict) -> None:
    # Never follow a symlink to a possibly sensitive/unrelated local file.
    if path.is_symlink():
        raise BridgeError("refusing symlink output path")
    path.parent.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(obj, ensure_ascii=False, indent=2) + "\n"
    if len(serialized.encode("utf-8")) > MAX_STATE_BYTES:
        raise BridgeError("state would exceed 2 MiB")
    handle, temporary = tempfile.mkstemp(prefix=".likes-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as f:
            f.write(serialized)
            f.flush()
            os.fsync(f.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _ingest_unlocked(source: Path, state_path: Path) -> tuple[int, int]:
    if source.resolve() == state_path.resolve():
        raise BridgeError("source and state paths must differ")
    raw = _read_liked(source)
    parsed = _rows(raw)  # Fully validate before touching any state.
    state = _load_state(state_path)
    prefix_size = state["source_size"]
    if len(raw) < prefix_size or hashlib.sha256(raw[:prefix_size]).hexdigest() != state["source_sha256"]:
        raise BridgeError("source log changed/truncated, not a pure append; state unchanged")
    known = {entry["id"] for entry in state["likes"]}
    imported = 0
    for digest, timestamp, station, title in parsed:
        if digest in known:
            continue
        state["likes"].append({
            "id": digest, "utc": timestamp, "station": station, "title": title,
            "status": "needs-review", "spotify_track_url": None
        })
        known.add(digest)
        imported += 1
    state["source_size"] = len(raw)
    state["source_sha256"] = hashlib.sha256(raw).hexdigest()
    _atomic_json(state_path, state)
    return imported, len(state["likes"])


def ingest(source: Path, state_path: Path) -> tuple[int, int]:
    with _state_guard(state_path):
        return _ingest_unlocked(source, state_path)


def search_links(title: str) -> tuple[str | None, str | None]:
    # A broadcaster's metadata may be advertisements, a DJ name, a show name,
    # etc.; the links are search suggestions, NEVER verified matches.
    if len(title.strip()) < 3 or title.strip().lower() == "track-unknown":
        return None, None
    search = quote(title, safe="")
    return "https://open.spotify.com/search/" + search, "https://www.google.com/search?q=" + search


def checked_track_url(url: str) -> str:
    if len(url) > 100 or any(ord(c) < 33 or ord(c) > 126 for c in url):
        raise BridgeError("track URL is not a short ASCII HTTPS URL")
    parts = urlsplit(url)
    if (parts.scheme != "https" or parts.netloc != "open.spotify.com" or
            parts.query or parts.fragment or parts.username or parts.password):
        raise BridgeError("only a canonical Spotify track URL is accepted")
    prefix = "/track/"
    track_id = parts.path[len(prefix):] if parts.path.startswith(prefix) else ""
    if not TRACK_ID.fullmatch(track_id):
        raise BridgeError("URL must be https://open.spotify.com/track/<22-character ID>")
    return "https://open.spotify.com/track/" + track_id


def _review_unlocked(state_path: Path, item_id: str, action: str, url: str | None = None) -> dict:
    state = _load_state(state_path)
    item = next((x for x in state["likes"] if x["id"] == item_id), None)
    if not item:
        raise BridgeError("no such Like ID")
    if action == "confirm":
        if not url:
            raise BridgeError("a manually checked Spotify track URL is required")
        item["spotify_track_url"] = checked_track_url(url)
        item["status"] = "confirmed"
    elif action == "skip":
        item["spotify_track_url"] = None
        item["status"] = "skipped"
    else:
        raise BridgeError("unknown review action")
    item["reviewed_utc"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    _atomic_json(state_path, state)
    return item


def review(state_path: Path, item_id: str, action: str, url: str | None = None) -> dict:
    with _state_guard(state_path):
        return _review_unlocked(state_path, item_id, action, url)


def _report_html(items: list[dict]) -> str:
    rows = []
    for item in items:
        spot, web = search_links(item["title"])
        links = ("<a rel='noopener noreferrer' target='_blank' href='" +
                 html.escape(spot, quote=True) + "'>Spotify search</a> / " +
                 "<a rel='noopener noreferrer' target='_blank' href='" +
                 html.escape(web, quote=True) + "'>Web search</a>") if spot and web else "No searchable ICY title"
        matched = ("<a rel='noopener noreferrer' target='_blank' href='" +
                   html.escape(item["spotify_track_url"], quote=True) + "'>Manually confirmed track</a>"
                   if item.get("status") == "confirmed" and item.get("spotify_track_url")
                   else "")
        rows.append("<tr><td>" + html.escape(str(item.get("utc", ""))) + "</td><td>" +
                    html.escape(item["station"]) + "</td><td>" +
                    html.escape(item["title"]) + "</td><td>" +
                    html.escape(item["status"]) + "</td><td>" + links + "</td><td>" +
                    matched + "</td><td><code>" + html.escape(item["id"]) + "</code></td></tr>")
    return """<!doctype html><!-- pocketjazz-likes-report-v1 --><html lang="ru"><meta charset="utf-8">
<meta name="referrer" content="no-referrer">
<title>Pocket Jazz — личные лайки (офлайн)</title>
<style>body{font:14px system-ui;background:#121925;color:#fafafa;padding:2rem}
table{border-collapse:collapse;max-width:100%}th,td{padding:.6rem;border:1px solid #566278;vertical-align:top}
a{color:#a5d5ff}td{max-width:320px;overflow-wrap:anywhere}</style>
<h1>Pocket Jazz Radio — локальный список лайков</h1>
<p>Поиск не подтверждает совпадение записи. Только пользователь выбирает правильный трек;
проверенную ссылку можно сохранить отдельной командой confirm.
Этот файл не подключается к HA/Spotify API и ничего не отправляет сам.</p>
<table><thead><tr><th>UTC</th><th>Станция</th><th>ICY</th><th>Статус</th><th>Поиск</th><th>Подтверждённый трек</th><th>Like ID</th></tr></thead>
<tbody>""" + "\n".join(rows) + "</tbody></table></html>\n"


def _is_generated_report(destination: Path) -> bool:
    """Permit --replace only for reports previously created by this tool."""
    if destination.is_symlink() or not destination.is_file() or (
            destination.stat().st_size > MAX_REPORT_BYTES):
        return False
    try:
        if destination.suffix.lower() == ".html":
            with destination.open("rb") as report:
                return REPORT_MARKER.encode("ascii") in report.read(256)
        if destination.suffix.lower() == ".json":
            payload = json.loads(destination.read_text(encoding="utf-8"))
            return (isinstance(payload, dict) and
                    payload.get("report_format") == REPORT_MARKER and
                    isinstance(payload.get("likes"), list) and
                    "source_size" not in payload and
                    "source_sha256" not in payload)
    except (OSError, ValueError, UnicodeError):
        return False
    return False


def _export_unlocked(state_path: Path, destination: Path, replace: bool) -> tuple[int, Path]:
    state = _load_state(state_path)
    if destination.resolve() == state_path.resolve():
        raise BridgeError("export path must differ from state path")
    if destination.is_symlink():
        raise BridgeError("refusing symlink report path")
    if destination.exists() and (
            not replace or not _is_generated_report(destination)):
        raise BridgeError("report exists or is not a generated Pocket Jazz report")
    if destination.suffix.lower() not in (".html", ".json"):
        raise BridgeError("export extension must be .html or .json")
    destination.parent.mkdir(parents=True, exist_ok=True)
    items = state["likes"]
    if destination.suffix.lower() == ".html":
        body = _report_html(items)
    else:
        # JSON can be consumed by a future opt-in PC/HA bridge. It contains
        # *no* secrets; only user-selected likes and reviewed matches.
        body = json.dumps({"report_format": REPORT_MARKER, "schema": 1,
                           "likes": items}, ensure_ascii=False, indent=2) + "\n"
    handle, tmp = tempfile.mkstemp(prefix=".likes-export-", suffix=".tmp", dir=destination.parent)
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as f:
            f.write(body)
            f.flush()
            os.fsync(f.fileno())
        if destination.exists() and (
                not replace or not _is_generated_report(destination)):
            raise BridgeError("report appeared concurrently or is not generated; refusing overwrite")
        os.replace(tmp, destination)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)
    return len(items), destination


def export(state_path: Path, destination: Path, replace: bool) -> tuple[int, Path]:
    # Two separate exports to the same path are serialized even when their
    # inputs are different state files. The state lock ensures a consistent
    # snapshot relative to concurrent ingest/review operations.
    with _state_guard(destination):
        with _state_guard(state_path):
            return _export_unlocked(state_path, destination, replace)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    p_in = sub.add_parser("ingest", help="Read a copied liked.txt and append new entries to local state")
    p_in.add_argument("--source", type=Path, required=True)
    p_in.add_argument("--state", type=Path, required=True)
    p_out = sub.add_parser("export", help="Export offline report .html or .json (no network)")
    p_out.add_argument("--state", type=Path, required=True)
    p_out.add_argument("--output", type=Path, required=True)
    p_out.add_argument("--replace", action="store_true", help="Replace a previously generated report")
    for verb in ("confirm", "skip"):
        subcmd = sub.add_parser(verb, help="Manual review; never calls Spotify API")
        subcmd.add_argument("--state", type=Path, required=True)
        subcmd.add_argument("--id", required=True)
        if verb == "confirm":
            subcmd.add_argument("--url", required=True)
    args = parser.parse_args(argv)
    try:
        if args.action == "ingest":
            new, total = ingest(args.source, args.state)
            print(f"Imported {new} new likes; local state contains {total}.")
        elif args.action == "export":
            count, dest = export(args.state, args.output, args.replace)
            print(f"Exported {count} likes: {dest}")
        else:
            item = review(args.state, args.id, args.action, getattr(args, "url", None))
            print(f"Like {item['id']} marked {item['status']}.")
        return 0
    except (OSError, BridgeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
