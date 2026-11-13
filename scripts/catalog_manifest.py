#!/usr/bin/env python3
"""Deterministic Pocket Jazz catalog manifest and offline integrity validation.

Write: python scripts/catalog_manifest.py --write
Check: python scripts/catalog_manifest.py --check
The pinned PC probe report documents prior validation; --check NEVER connects
to the internet or makes claims about current station uptime.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]
CATALOG_VERSION = "2026.11.13.1"
REPO = "lavalava45/cardputer-pocket-jazz-radio"
FILES = (
    "01-essentials.m3u",
    "02-modern-jazz.m3u",
    "03-mellow-bossa.m3u",
    "05-swing-vocals.m3u",
    "06-latin-soul-jazz.m3u",
)
REPORT = "checks/pc-stream-check-20261113.json"


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_playlist(path: Path) -> list[dict]:
    """Validate the subset supported by Cardputer; no personal stations."""
    text = path.read_text(encoding="utf-8-sig")
    lines = text.splitlines()
    if not lines or lines[0].strip() != "#EXTM3U":
        raise ValueError(f"{path.name}: missing #EXTM3U")
    pending_name = None
    pending_alt = None
    stations = []
    for raw in lines[1:]:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#EXTINF:"):
            if pending_name is not None:
                raise ValueError(f"{path.name}: station has no URL")
            if "," not in line:
                raise ValueError(f"{path.name}: malformed EXTINF")
            pending_name = line.split(",", 1)[1].strip()
            if not pending_name or len(pending_name.encode("utf-8")) > 55:
                raise ValueError(f"{path.name}: name too long/empty")
            pending_alt = None
        elif line.startswith("#PJ-ALT:"):
            if pending_name is None or pending_alt is not None:
                raise ValueError(f"{path.name}: unexpected alternate URL")
            pending_alt = line.split(":", 1)[1].strip()
            validate_url(pending_alt)
        elif line.startswith("#"):
            # Other metadata/comment lines are retained verbatim in the M3U.
            continue
        else:
            if pending_name is None:
                raise ValueError(f"{path.name}: URL without EXTINF")
            validate_url(line)
            stations.append({"name": pending_name, "url": line, "alternate": pending_alt})
            pending_name = pending_alt = None
    if pending_name is not None:
        raise ValueError(f"{path.name}: trailing EXTINF without URL")
    return stations


def validate_url(url: str) -> None:
    if len(url.encode("utf-8")) > 191 or any(c in url for c in "\r\n\\ "):
        raise ValueError("oversize or unsafe station URL")
    target = urlsplit(url)
    if target.scheme != "https" or not target.hostname or target.username or target.password:
        raise ValueError("editorial catalog requires HTTPS without credentials")


def expected_manifest() -> dict:
    catalog = ROOT / "catalog"
    names = sorted(x.name for x in catalog.glob("*.m3u"))
    if names != list(FILES):
        raise ValueError(f"only five curated playlists are allowed: {names!r}")
    manifest_files = []
    primary_keys: set[str] = set()
    all_urls: set[str] = set()
    primary_count = backup_count = 0
    stream_keys = set()
    for name in FILES:
        path = catalog / name
        stations = parse_playlist(path)
        for station in stations:
            if station["url"] in primary_keys or station["url"] in all_urls:
                raise ValueError(f"duplicate primary: {station['url']}")
            primary_keys.add(station["url"])
            all_urls.add(station["url"])
            stream_keys.add((name, station["name"], "primary", station["url"]))
            primary_count += 1
            if station["alternate"]:
                if station["alternate"] in all_urls:
                    raise ValueError(f"duplicate alternate: {station['alternate']}")
                all_urls.add(station["alternate"])
                stream_keys.add((name, station["name"], "backup", station["alternate"]))
                backup_count += 1
        manifest_files.append({
            "path": f"catalog/{name}",
            "sha256": file_sha256(path),
            "bytes": path.stat().st_size,
            "stations": len(stations),
        })
    if primary_count > 64:
        raise ValueError("current Cardputer catalog capacity is 64 stations")
    report_path = ROOT / REPORT
    report = json.loads(report_path.read_text(encoding="utf-8"))
    checks = report["streams"]
    observed = {(row["playlist"], row["station"], row["kind"], row["url"]) for row in checks}
    if (len(observed) != len(checks) or observed != stream_keys
            or any(row["status"] != "PASS" for row in checks)):
        raise ValueError("PC verification report must cover every URL with PASS")
    if report["counts"] != {
            "primary": primary_count, "backup": backup_count,
            "passed": primary_count + backup_count, "failed": 0}:
        raise ValueError("PC verification counts inconsistent")
    if not report.get("observed_utc"):
        raise ValueError("report has no observation date")
    return {
        "schema_version": 1,
        "catalog_version": CATALOG_VERSION,
        "repository": REPO,
        "playlists": len(manifest_files),
        "stations": primary_count,
        "alternate_urls": backup_count,
        "files": manifest_files,
        "pc_verification": {
            "observed_utc": report["observed_utc"],
            "passed": report["counts"]["passed"],
            "failed": 0,
            "report": REPORT,
            "report_sha256": file_sha256(report_path),
            "limitation": "PC network/MP3 framing only; no Cardputer playback guarantee",
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = json.dumps(expected_manifest(), indent=2, ensure_ascii=False) + "\n"
    path = ROOT / "manifest.json"
    if args.write:
        # Exact LF bytes on both Windows and Linux: SHA-256 and git diff
        # must not depend on the maintainer's platform line endings.
        path.write_bytes(expected.encode("utf-8"))
        print(f"WROTE {path}")
    elif not path.exists() or path.read_text(encoding="utf-8") != expected:
        raise SystemExit("manifest.json does not match curated playlists/report; regenerate with --write")
    else:
        print("PASS manifest, checksums, playlist capacity and 62 PC observations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
