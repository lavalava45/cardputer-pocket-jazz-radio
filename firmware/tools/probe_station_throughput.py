#!/usr/bin/env python3
"""Bounded HTTPS MP3/ICY transfer timing probe; DOES NOT PLAY OR SAVE AUDIO.

Uses only public station URLs, strict HTTPS redirects and OS CA verification.
Measures received body bytes (including ICY metadata, excluding HTTP chunk
framing), 4-KiB read intervals and 500-ms+ waits. A PC measurement is NEVER
an ESP32 decoding/ES8311 audio-quality measurement.
"""
from __future__ import annotations

import argparse
import json
import statistics
import time
from pathlib import Path
from urllib import request
from urllib.parse import urlsplit

STATIONS = {
    "Bossa Beyond ice5": "https://ice5.somafm.com/bossa-128-mp3",
    "Bossa Beyond ice6": "https://ice6.somafm.com/bossa-128-mp3",
    "Groove Salad ice5": "https://ice5.somafm.com/groovesalad-128-mp3",
    "Groove Salad ice6": "https://ice6.somafm.com/groovesalad-128-mp3",
    "Groove Salad Classic ice5": "https://ice5.somafm.com/gsclassic-128-mp3",
    "Fluid ice5": "https://ice5.somafm.com/fluid-128-mp3",
    "Sonic Universe ice5": "https://ice5.somafm.com/sonicuniverse-128-mp3",
    "Secret Agent ice5": "https://ice5.somafm.com/secretagent-128-mp3",
    "FIP Jazz": "https://icecast.radiofrance.fr/fipjazz-midfi.mp3",
    "Jazz24": "https://knkx-live-a.edge.audiocdn.com/6285_128k",
    "Manouche": "https://jazz-wr02.ice.infomaniak.ch/jazz-wr02-128.mp3",
}
CAP_BYTES = 2_000_000


class OnlyHttpsRedirects(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urlsplit(newurl).scheme != "https":
            raise ValueError("Station attempted insecure HTTP redirect")
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def probe(label: str, url: str, seconds: float) -> dict:
    started = time.monotonic()
    rec = {"station": label, "hostname": urlsplit(url).hostname,
           "requested_seconds": seconds, "bytes": 0, "http": None,
           "mime": None, "icy_metaint": None, "icy_bitrate": None,
           "chunked": False, "first_byte_ms": None, "read_gap_max_ms": 0,
           "read_gaps_over_500_ms": 0, "body_kib_per_s": None}
    opener = request.build_opener(OnlyHttpsRedirects)
    reads: list[float] = []
    try:
        req = request.Request(
            url, headers={"User-Agent": "PocketJazz-PC-HTTPS-Measurement/1.0",
                          "Icy-MetaData": "1"}
        )
        with opener.open(req, timeout=7) as response:
            rec["http"] = response.status
            rec["mime"] = response.headers.get("content-type")
            rec["icy_metaint"] = response.headers.get("icy-metaint")
            rec["icy_bitrate"] = response.headers.get("icy-br")
            rec["chunked"] = "chunked" in response.headers.get(
                "transfer-encoding", "").lower()
            while time.monotonic() - started < seconds and rec["bytes"] < CAP_BYTES:
                before = time.monotonic()
                body = response.read(min(4096, CAP_BYTES - rec["bytes"]))
                after = time.monotonic()
                if not body:
                    break
                rec["bytes"] += len(body)
                if rec["first_byte_ms"] is None:
                    rec["first_byte_ms"] = round((after - started) * 1000)
                gap = (after - before) * 1000
                reads.append(gap)
                rec["read_gap_max_ms"] = max(rec["read_gap_max_ms"], round(gap))
                if gap > 500:
                    rec["read_gaps_over_500_ms"] += 1
        duration = time.monotonic() - started
        rec["elapsed_ms"] = round(duration * 1000)
        if duration:
            rec["body_kib_per_s"] = round(rec["bytes"] / 1024 / duration, 1)
        if reads:
            rec["median_read_gap_ms"] = round(statistics.median(reads))
    except (OSError, ValueError) as exc:
        rec["error"] = f"{type(exc).__name__}: {str(exc)[:160]}"
        rec["elapsed_ms"] = round((time.monotonic() - started) * 1000)
    return rec


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=8)
    parser.add_argument("--output", type=Path, help="new JSON file only; no audio output")
    parser.add_argument("--station", choices=tuple(STATIONS) + ("all",), default="all")
    args = parser.parse_args(argv)
    if args.seconds < 2 or args.seconds > 45:
        parser.error("--seconds must be between 2 and 45")
    records = []
    for label, url in STATIONS.items():
        if args.station != "all" and label != args.station:
            continue
        result = probe(label, url, args.seconds)
        records.append(result)
        print(json.dumps(result, ensure_ascii=False), flush=True)
    if args.output:
        # Never erase another diagnostic or a user's file accidentally.
        with args.output.open("x", encoding="utf-8") as out:
            json.dump({"measurement": "Windows HTTPS body throughput (not audible audio)",
                       "entries": records}, out, ensure_ascii=False, indent=2)
            out.write("\n")


if __name__ == "__main__":
    main()
