#!/usr/bin/env python3
"""Bounded, read-only PC network probe for Pocket Jazz MP3/ICY streams.

This diagnoses server/TLS/HTTP/ICY delivery on the *PC*, NOT playback on
Cardputer. All audio bytes remain transient in RAM; no recordings are made.
No NVS, firmware, station M3U or SD data is changed.
"""
from __future__ import annotations

import argparse
import json
import math
import ssl
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path

ENDPOINTS = (
    ("Groove Salad ice5", "https://ice5.somafm.com/groovesalad-128-mp3"),
    ("Groove Salad ice6", "https://ice6.somafm.com/groovesalad-128-mp3"),
    ("FIP Jazz", "https://icecast.radiofrance.fr/fipjazz-midfi.mp3"),
    ("Jazz24", "https://knkx-live-a.edge.audiocdn.com/6285_128k"),
    ("Jazz Radio Manouche", "https://jazz-wr02.ice.infomaniak.ch/jazz-wr02-128.mp3"),
)
CHUNK_BYTES = 4096
MAX_BYTES = 2 * 1024 * 1024


class IcyDemux:
    """Only count audio bytes; drop ICY metadata without storing broadcaster text.

    The server supplies every N MP3 bytes: one length byte then length*16
    metadata bytes. Input may be cut at any byte; this parser carries state
    across reads. A missing icy-metaint is plain MP3 with no metadata.
    """

    def __init__(self, interval: int):
        if interval < 0 or interval > 1_000_000:
            raise ValueError("invalid ICY metadata interval")
        self.interval = interval
        self.audio_until_metadata = interval
        self.metadata_left = 0
        self.expect_length = False
        self.audio_bytes = 0
        self.metadata_blocks = 0
        self.metadata_bytes = 0

    def feed(self, data: bytes) -> int:
        if self.interval == 0:
            self.audio_bytes += len(data)
            return len(data)
        pos, audio = 0, 0
        while pos < len(data):
            if self.metadata_left:
                n = min(len(data) - pos, self.metadata_left)
                pos += n
                self.metadata_left -= n
                self.metadata_bytes += n
                if self.metadata_left == 0:
                    self.audio_until_metadata = self.interval
                continue
            if self.expect_length:
                self.expect_length = False
                self.metadata_left = data[pos] * 16
                self.metadata_blocks += 1
                pos += 1
                if not self.metadata_left:
                    self.audio_until_metadata = self.interval
                continue
            n = min(len(data) - pos, self.audio_until_metadata)
            pos += n
            audio += n
            self.audio_bytes += n
            self.audio_until_metadata -= n
            if self.audio_until_metadata == 0:
                self.expect_length = True
        return audio


def bounded_interval(raw: str | None) -> int:
    if raw is None or not raw.strip():
        return 0
    if not raw.strip().isdigit():
        raise ValueError("invalid icy-metaint header")
    value = int(raw)
    if value < 1 or value > 1_000_000:
        raise ValueError("out-of-range icy-metaint header")
    return value


def probe_one(name: str, url: str, duration: float = 10.0) -> dict:
    if duration < 0.25 or duration > 90:
        raise ValueError("duration must be 0.25–90 seconds")
    # These URLs are hardcoded from the current playlists; the tool does not
    # accept arbitrary untrusted file:// or local network URLs.
    if not (url.startswith("https://") and
            any(url == safe_url for _, safe_url in ENDPOINTS)):
        raise ValueError("probe only supports the five known public HTTPS endpoints")
    started = time.monotonic()
    result = {"station": name, "url_host": urllib.request.urlparse(url).hostname,
              "requested_seconds": duration, "http_status": None,
              "type": None, "transfer": None, "icy_metaint": None,
              "icy_br": None, "audio_bytes": 0, "metadata_blocks": 0,
              "metadata_bytes": 0, "reads": 0, "first_read_ms": None,
              "largest_interread_ms": None, "observed_seconds": 0, "audio_kbps": 0,
              "server_underruns": None, "hardware_reconnects": None,
              "error": None}
    req = urllib.request.Request(
        url, headers={"User-Agent": "PocketJazz-ReadOnly-NetworkProbe/1",
                      "Icy-MetaData": "1"})
    try:
        with urllib.request.urlopen(req, timeout=6, context=ssl.create_default_context()) as resp:
            result["http_status"] = resp.status
            result["type"] = resp.headers.get("Content-Type")
            result["transfer"] = resp.headers.get("Transfer-Encoding")
            result["icy_br"] = resp.headers.get("icy-br")
            interval = bounded_interval(resp.headers.get("icy-metaint"))
            result["icy_metaint"] = interval or None
            demux = IcyDemux(interval)
            last_received = None
            # For 128-kbps streams, 2 MiB is >120 s of MP3; ensures bounded
            # memory/network usage even if a server ignores duration.
            fetched = 0
            while time.monotonic() - started < duration and fetched < MAX_BYTES:
                payload = resp.read1(min(CHUNK_BYTES, MAX_BYTES - fetched))
                now = time.monotonic()
                if not payload:
                    break
                fetched += len(payload)
                result["reads"] += 1
                if result["first_read_ms"] is None:
                    result["first_read_ms"] = round((now - started) * 1000)
                if last_received is not None:
                    gap = (now - last_received) * 1000
                    result["largest_interread_ms"] = round(
                        max(result["largest_interread_ms"] or 0, gap))
                last_received = now
                demux.feed(payload)
            result["audio_bytes"] = demux.audio_bytes
            result["metadata_blocks"] = demux.metadata_blocks
            result["metadata_bytes"] = demux.metadata_bytes
    except (OSError, urllib.error.URLError, ValueError) as exc:
        result["error"] = type(exc).__name__ + ": " + str(exc)[:140]
    observed = time.monotonic() - started
    result["observed_seconds"] = round(observed, 2)
    result["audio_kbps"] = round(8 * result["audio_bytes"] / max(0.001, observed) / 1000, 1)
    return result


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--seconds", type=float, default=10.0,
                    help="seconds per station, 0.25–90 (default 10)")
    ap.add_argument("--station", choices=[n for n, _ in ENDPOINTS],
                    help="one of five endpoints; otherwise all five")
    ap.add_argument("--output", type=Path, help="optional NEW JSON report path")
    args = ap.parse_args(argv)
    selected = [(n, u) for n, u in ENDPOINTS if not args.station or n == args.station]
    report = {"purpose": "PC HTTPS/ICY delivery only; no Cardputer audio or buffer",
              "stations": [probe_one(n, u, args.seconds) for n, u in selected]}
    output = json.dumps(report, ensure_ascii=False, indent=2)
    if args.output:
        if args.output.exists() or args.output.is_symlink():
            ap.error("report output exists; refusing to overwrite")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output + "\n", encoding="utf-8")
    print(output)
    return 0 if all(not x["error"] for x in report["stations"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
