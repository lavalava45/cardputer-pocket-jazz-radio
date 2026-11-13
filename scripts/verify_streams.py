"""Bounded HTTPS MP3 livestream checks for Pocket Jazz M3U playlists.

Network-only: extracts up to 8192 audio bytes (skipping requested ICY metadata)
per URL into volatile RAM to verify three consecutive MPEG Layer III frames.
NEVER writes, records, relays or decodes music.
Does not prove sound, TLS timekeeping, or stability on an actual ESP32/Cardputer.

Usage: python scripts/verify_streams.py --report checks/pc-stream-check-YYYYMMDD.json
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
from urllib.parse import urlsplit

try:
    import requests
except ImportError:
    requests = None

ROOT = Path(__file__).resolve().parents[1]
SAMPLE_BYTES = 8192
RAW_LIMIT_BYTES = 32768  # Bound on total received bytes including ICY metadata.
READ_BYTES = 2048
CONTENT_TYPES = frozenset({"audio/mpeg", "audio/mp3", "audio/x-mpeg", "application/octet-stream"})

# Match the *actual* HTTPClient request in Arduino-ESP32 3.3.9:
# SecureICYSource::startRequest adds only Icy-MetaData=1, disables reuse and
# keeps the SDK defaults. There is NO PocketJazz/vX User-Agent on the device.
# 'Accept': None suppresses Requests' otherwise automatic Accept: */* header.
DEVICE_HEADERS = {
    "User-Agent": "ESP32HTTPClient",
    "Icy-MetaData": "1",
    "Accept-Encoding": "identity;q=1,chunked;q=0.1,*;q=0",
    "Connection": "close",
    "Accept": None,
}


def parse_icy_interval(raw):
    """Match PocketJazzRadio/IcyMetadataText.h (0 or digits <= 1048576)."""
    if raw is None or raw == "":
        return 0
    if not isinstance(raw, str) or not raw.isascii() or not raw.isdecimal():
        raise ValueError("Invalid icy-metaint header")
    parsed = 0
    for char in raw:
        digit = ord(char) - ord("0")
        if parsed > (1_048_576 - digit) // 10:
            raise ValueError("Invalid icy-metaint header")
        parsed = parsed * 10 + digit
    return parsed


class IcyAudioSample:
    """Strip bounded ICY metadata as an incremental parser, preserving MP3 bytes.

    Every 'icy-metaint' audio bytes, the source emits a one-byte length,
    followed by length*16 metadata bytes. Splits may occur at ANY byte.
    Only extracted audio bytes are retained; title text is discarded.
    """

    def __init__(self, interval):
        self.interval = interval
        self.until_metadata = interval
        self.metadata_left = 0
        self.length_due = False
        self.audio = bytearray()

    def feed(self, raw):
        at = 0
        while at < len(raw) and len(self.audio) < SAMPLE_BYTES:
            if not self.interval:
                n = min(len(raw) - at, SAMPLE_BYTES - len(self.audio))
                self.audio.extend(raw[at:at + n])
                at += n
            elif self.metadata_left:
                n = min(self.metadata_left, len(raw) - at)
                self.metadata_left -= n
                at += n
                if not self.metadata_left:
                    self.until_metadata = self.interval
            elif self.length_due:
                self.metadata_left = raw[at] * 16
                at += 1
                self.length_due = False
                if not self.metadata_left:
                    self.until_metadata = self.interval
            else:
                n = min(len(raw) - at, self.until_metadata, SAMPLE_BYTES - len(self.audio))
                self.audio.extend(raw[at:at + n])
                at += n
                self.until_metadata -= n
                if not self.until_metadata:
                    self.length_due = True

    @property
    def complete(self):
        return len(self.audio) == SAMPLE_BYTES


def mp3_frame(data: bytes, at: int):
    """Return (frame length, sample rate, bitrate, MPEG version) or None."""
    if at < 0 or at + 4 > len(data) or data[at] != 0xff or (data[at + 1] & 0xe0) != 0xe0:
        return None
    version = (data[at + 1] >> 3) & 3
    layer = (data[at + 1] >> 1) & 3
    bitrate_id = data[at + 2] >> 4
    rate_id = (data[at + 2] >> 2) & 3
    padding = (data[at + 2] >> 1) & 1
    if version == 1 or layer != 1 or bitrate_id in (0, 15) or rate_id == 3:
        return None
    rate = (44100, 48000, 32000)[rate_id]
    rate //= 1 if version == 3 else 2 if version == 2 else 4
    bitrates = (
        (0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320),
        (0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160),
    )
    kbps = bitrates[0 if version == 3 else 1][bitrate_id]
    length = ((144000 if version == 3 else 72000) * kbps // rate) + padding
    return (length, rate, kbps, version) if length >= 24 else None


def mp3_signature(data: bytes):
    """Three aligned MP3 frame headers, with matching MPEG version and rate."""
    for at in range(min(max(0, len(data) - 12), 4096)):
        first = mp3_frame(data, at)
        if not first:
            continue
        second_at = at + first[0]
        second = mp3_frame(data, second_at)
        if not second or (first[1], first[3]) != (second[1], second[3]):
            continue
        third = mp3_frame(data, second_at + second[0])
        if third and (first[1], first[3]) == (third[1], third[3]):
            return {"mpeg": {3: "1", 2: "2", 0: "2.5"}[first[3]],
                    "sample_rate_hz": first[1], "bitrate_kbps_first_frame": first[2],
                    "first_frame_offset": at}
    return None


def playlist_items(directory: Path):
    items = []
    seen = set()
    for path in sorted(directory.glob("*.m3u")):
        name, alt = "", None
        for raw in path.read_text(encoding="utf-8-sig").splitlines():
            line = raw.strip()
            if line.startswith("#EXTINF:"):
                name = line.split(",", 1)[-1].strip()
                alt = None
            elif line.startswith("#PJ-ALT:"):
                alt = line[len("#PJ-ALT:"):].strip()
            elif line.startswith(("https://", "http://")):
                if not name or line in seen:
                    raise ValueError(f"Missing station name or duplicate URL: {path.name} {line}")
                seen.add(line)
                items.append({"playlist": path.name, "station": name, "kind": "primary", "url": line})
                if alt:
                    items.append({"playlist": path.name, "station": name, "kind": "backup", "url": alt})
                name, alt = "", None
    return items


def request_once(url: str):
    if requests is None:
        raise RuntimeError("Install 'requests' to verify streams")
    if urlsplit(url).scheme != "https":
        return {"status": "FAIL", "reason": "non-HTTPS candidate"}
    with requests.get(url, headers=DEVICE_HEADERS,
                      stream=True, timeout=(5, 8), allow_redirects=True, verify=True) as response:
        if urlsplit(response.url).scheme != "https" or any(
                urlsplit(h.url).scheme != "https" for h in response.history):
            return {"status": "FAIL", "reason": "HTTPS downgrade"}
        content_type = response.headers.get("Content-Type", "").split(";", 1)[0].strip().lower()
        if response.status_code != 200:
            return {"status": "FAIL", "reason": f"HTTP {response.status_code}",
                    "content_type": content_type}
        if content_type not in CONTENT_TYPES:
            return {"status": "FAIL", "reason": "not a declared MP3 stream",
                    "content_type": content_type}
        try:
            interval = parse_icy_interval(response.headers.get("icy-metaint"))
        except ValueError:
            return {"status": "FAIL", "reason": "invalid icy-metaint header",
                    "content_type": content_type}
        response.raw.decode_content = False
        extractor = IcyAudioSample(interval)
        received = 0
        while not extractor.complete and received < RAW_LIMIT_BYTES:
            chunk = response.raw.read(min(READ_BYTES, RAW_LIMIT_BYTES - received))
            if not chunk:
                break
            received += len(chunk)
            extractor.feed(chunk)
        sample = bytes(extractor.audio)
        signature = mp3_signature(sample)
        if not signature:
            return {"status": "FAIL", "reason": "three aligned MPEG Layer III frames not detected",
                    "bytes_examined": len(sample), "raw_bytes_received": received,
                    "icy_interval": interval, "content_type": content_type}
        return {"status": "PASS", "http": response.status_code,
                "content_type": content_type, "bytes_examined": len(sample),
                "raw_bytes_received": received, "icy_interval": interval,
                "final_url": response.url, "mp3": signature}


def verify_item(item, retries=2):
    result = {"playlist": item["playlist"], "station": item["station"],
              "kind": item["kind"], "url": item["url"]}
    for attempt in range(1, retries + 1):
        try:
            outcome = request_once(item["url"])
        except Exception as error:
            outcome = {"status": "FAIL", "reason": type(error).__name__,
                       "detail": str(error)[:160]}
        result.update(outcome)
        result["attempts"] = attempt
        if result["status"] == "PASS":
            break
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, help="Save JSON metadata only (NO AUDIO)")
    parser.add_argument("--workers", type=int, default=10)
    args = parser.parse_args(argv)
    if not 1 <= args.workers <= 16:
        parser.error("--workers must be between 1 and 16")
    if requests is None:
        parser.error("requests package not installed")
    items = playlist_items(ROOT / "catalog")
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        rows = list(executor.map(verify_item, items))
    for item in rows:
        print(f"{item['status']:4} {item['kind']:7} "
              f"{item['station']:<43} {item.get('reason', item.get('mp3', {}).get('sample_rate_hz', ''))}")
    counts = {"primary": sum(x["kind"] == "primary" for x in rows),
              "backup": sum(x["kind"] == "backup" for x in rows),
              "passed": sum(x["status"] == "PASS" for x in rows),
              "failed": sum(x["status"] != "PASS" for x in rows)}
    report = {
        "observed_utc": datetime.now(timezone.utc).isoformat(),
        "method": "HTTPS GET, standard CA+hostname, no HTTP downgrade; Arduino-ESP32 3.3.9 "
                  "HTTPClient User-Agent/Accept-Encoding/Connection, Icy-MetaData: 1; "
                  "bounded ICY metadata removal; 8192 volatile MP3 audio bytes; "
                  "three aligned MPEG Layer III headers. No audio retained.",
        "limitation": "Only network+MP3 framing from this Windows host; not actual Cardputer playback.",
        "counts": counts, "streams": rows,
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_bytes((json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print("SUMMARY", counts)
    return 1 if counts["failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
