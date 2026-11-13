#!/usr/bin/env python3
"""Build a small, sourced offline jazz-note TSV from MusicBrainz core data.

No text generation, scraping, Wikipedia excerpts, user tags, or private API keys.
MusicBrainz lookups are throttled, and every emitted album is verified against
the requested artist MBID. The output itself requires no network to use.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import time
import unicodedata
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "data" / "artists-input.csv"
DEFAULT_OUTPUT = ROOT / "data" / "artist-notes.tsv"
DEFAULT_INDEX = ROOT / "data" / "artist-notes.idx"
API = "https://musicbrainz.org/ws/2"
USER_AGENT = "PocketJazzRadio-ArtistNotes/0.1 (https://github.com/lavalava45)"
LICENSE = "MusicBrainz core: CC0-1.0"
ATTRIBUTION = "MusicBrainz; album picked by Pocket Jazz editor"
UUID = re.compile(r"^[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}$")
FIELDS = ("artist_mbid", "artist_name", "album_mbid", "album_title")
FIELD_LIMITS = (48, 108, 110, 100, 110, 90, 40)
MAX_INPUT_ROWS = 50
MAX_RESPONSE_BYTES = 1024 * 1024
INDEX_MAGIC = "#PJ-NOTES-IDX"
INDEX_VERSION = "1"
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619


class InvalidRecord(ValueError):
    """Data is unsupported or cannot be authenticated against MusicBrainz."""


def clean(value: str, *, max_bytes: int, label: str) -> str:
    """Require one safe UTF-8 field; do not silently truncate source claims."""
    if not isinstance(value, str):
        raise InvalidRecord(f"{label}: not a string")
    # Strip only ordinary padding spaces. strip() would silently erase CR/LF,
    # TAB and other controls at a field boundary, hiding invalid input.
    value = unicodedata.normalize("NFC", value.strip(" "))
    if not value or len(value.encode("utf-8")) > max_bytes:
        raise InvalidRecord(f"{label}: empty or exceeds {max_bytes} UTF-8 bytes")
    if any(unicodedata.category(c) in {"Cc", "Cf", "Cs", "Zl", "Zp"} for c in value):
        raise InvalidRecord(f"{label}: control, format, or line separator")
    # Safe for TSV, text UIs, and accidental opening in spreadsheet software.
    if value[0] in "=+-@":
        raise InvalidRecord(f"{label}: spreadsheet-formula-leading character")
    return value


def require_uuid(value: str, label: str) -> str:
    if not isinstance(value, str):
        raise InvalidRecord(f"{label}: not a string")
    value = value.strip(" ").lower()
    if not UUID.fullmatch(value):
        raise InvalidRecord(f"{label}: invalid MBID")
    return value


def artist_key(name: str) -> str:
    decomposed = unicodedata.normalize("NFKD", name)
    ascii_name = decomposed.encode("ascii", "ignore").decode("ascii")
    key = re.sub(r"[^a-z0-9]+", "-", ascii_name.lower()).strip("-")
    return clean(key, max_bytes=FIELD_LIMITS[0], label="artist_key")


def verified_year(value: object) -> str | None:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9]{4}(?:-[0-9]{2}(?:-[0-9]{2})?)?", value):
        return None
    return value[:4]


def artist_note(artist: dict) -> str:
    """Only core artist lifespan + core instrument relationships, not tags."""
    instruments = sorted({
        rel["instrument"]["name"] for rel in artist.get("relations", [])
        if isinstance(rel, dict) and rel.get("target-type") == "instrument"
        and isinstance(rel.get("instrument"), dict)
        and isinstance(rel["instrument"].get("name"), str)
        and rel["instrument"]["name"]
    })
    # Artist disambiguation is MusicBrainz *core* data (CC0), unlike user tags.
    # Prefer the source wording verbatim: avoid extrapolating instruments/genre.
    description = artist.get("disambiguation", "")
    if instruments:
        detail = "instrument: " + clean(instruments[0], max_bytes=52, label="instrument")
    elif description:
        detail = "MB description: " + clean(description, max_bytes=65, label="disambiguation")
    else:
        detail = "instrument not recorded"
    life = artist.get("life-span", {}) or {}
    begin = verified_year(life.get("begin"))
    end = verified_year(life.get("end"))
    if artist.get("type") == "Person":
        dates = (f"born {begin}" if begin else "birth unverified")
        dates += (f", died {end}" if end else "")
    else:
        dates = (f"formed {begin}" if begin else "formation unverified")
        dates += (f", ended {end}" if end else "")
    return clean(f"{detail}; {dates}", max_bytes=FIELD_LIMITS[1], label="instrument_era")


def artist_credit_ids(group: dict) -> set[str]:
    credits = group.get("artist-credit", [])
    if not isinstance(credits, list):
        return set()
    return {
        part["artist"]["id"].lower() for part in credits
        if isinstance(part, dict) and isinstance(part.get("artist"), dict)
        and isinstance(part["artist"].get("id"), str)
    }


def verify(row: dict[str, str], fetch) -> tuple[str, ...]:
    artist_id = require_uuid(row["artist_mbid"], "artist_mbid")
    group_id = require_uuid(row["album_mbid"], "album_mbid")
    expected_artist = clean(row["artist_name"], max_bytes=100, label="artist_name")
    expected_album = clean(row["album_title"], max_bytes=FIELD_LIMITS[2], label="album_title")
    artist = fetch("artist", artist_id, "instrument-rels")
    group = fetch("release-group", group_id, "artist-credits")
    if artist.get("id", "").lower() != artist_id or artist.get("name") != expected_artist:
        raise InvalidRecord("artist name/MBID does not match official record")
    if group.get("id", "").lower() != group_id or group.get("title") != expected_album:
        raise InvalidRecord("recommended album name/MBID does not match official record")
    if group.get("primary-type") != "Album":
        raise InvalidRecord("recommendation must be a release-group of type Album")
    if artist_id not in artist_credit_ids(group):
        raise InvalidRecord("recommended album does not credit the selected artist")
    fields = (
        artist_key(expected_artist), artist_note(artist), expected_album,
        f"https://musicbrainz.org/artist/{artist_id}",
        f"https://musicbrainz.org/release-group/{group_id}", ATTRIBUTION, LICENSE,
    )
    checked = tuple(clean(field, max_bytes=limit, label=label) for field, limit, label
                    in zip(fields, FIELD_LIMITS,
                           ("artist_key", "instrument_era", "album", "artist_source_url",
                            "album_source_url", "attribution", "license")))
    # v0.3's small SD parser is ASCII-only. Do not transliterate or silently
    # corrupt the source's names/claims; reject records requiring Unicode.
    if any(not field.isascii() for field in checked):
        raise InvalidRecord("SD export requires ASCII source fields; no lossy transliteration")
    return checked


class MusicBrainzClient:
    """One request per >=1.2 s, bounded payload, HTTPS, limited 503/429 retries."""

    def __init__(self, user_agent: str, timeout: float = 15.0):
        self.user_agent = clean(user_agent, max_bytes=190, label="user_agent")
        self.timeout = timeout
        self.last_request_at = 0.0

    def __call__(self, entity: str, mbid: str, includes: str) -> dict:
        if entity not in {"artist", "release-group"} or includes not in {"instrument-rels", "artist-credits"}:
            raise InvalidRecord("unsupported MusicBrainz endpoint")
        url = f"{API}/{entity}/{require_uuid(mbid, 'mbid')}?inc={includes}&fmt=json"
        for attempt in range(3):
            remaining = 1.2 - (time.monotonic() - self.last_request_at)
            if remaining > 0:
                time.sleep(remaining)
            self.last_request_at = time.monotonic()
            request = Request(url, headers={"User-Agent": self.user_agent, "Accept": "application/json"})
            try:
                with urlopen(request, timeout=self.timeout) as response:
                    payload = response.read(MAX_RESPONSE_BYTES + 1)
                if len(payload) > MAX_RESPONSE_BYTES:
                    raise InvalidRecord("MusicBrainz API response exceeds safety limit")
                data = json.loads(payload)
                if not isinstance(data, dict):
                    raise InvalidRecord("unexpected API response")
                return data
            except HTTPError as error:
                if error.code in (429, 503) and attempt < 2:
                    time.sleep(min(20.0, 4.0 * (attempt + 1)))
                    continue
                raise InvalidRecord(f"MusicBrainz HTTP {error.code}") from error
            except (URLError, TimeoutError, json.JSONDecodeError) as error:
                raise InvalidRecord(f"MusicBrainz unavailable/invalid response: {type(error).__name__}") from error
        raise InvalidRecord("MusicBrainz retry budget exceeded")


def load_input(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file, strict=True)
        if tuple(reader.fieldnames or ()) != FIELDS:
            raise InvalidRecord("CSV header must be: " + ",".join(FIELDS))
        rows = []
        for number, row in enumerate(reader, start=2):
            if number > MAX_INPUT_ROWS + 1:
                raise InvalidRecord(f"maximum {MAX_INPUT_ROWS} curated rows")
            if None in row or any(value is None for value in row.values()):
                raise InvalidRecord(f"CSV row {number}: malformed number of fields")
            rows.append(row)
        return rows


def generate(rows: list[dict[str, str]], fetch) -> tuple[list[tuple[str, ...]], list[str]]:
    entries, rejected = [], []
    used_keys, used_ids = set(), set()
    for index, row in enumerate(rows, start=2):
        try:
            aid = require_uuid(row["artist_mbid"], "artist_mbid")
            if aid in used_ids:
                raise InvalidRecord("duplicate artist MBID")
            entry = verify(row, fetch)
            if entry[0] in used_keys:
                raise InvalidRecord("duplicate artist key")
            entries.append(entry)
            used_ids.add(aid)
            used_keys.add(entry[0])
        except (InvalidRecord, KeyError, TypeError) as error:
            rejected.append(f"CSV row {index}: {str(error)}")
    return sorted(entries), rejected


def render(entries: list[tuple[str, ...]]) -> bytes:
    # No header: firmware reads exactly seven tab-separated fields per line.
    # Re-validate at the final serialization boundary (not only via verify()).
    lines = []
    labels = ("artist_key", "instrument_era", "album", "artist_source_url",
              "album_source_url", "attribution", "license")
    for row in entries:
        if len(row) != len(FIELD_LIMITS):
            raise InvalidRecord("output record must have exactly seven fields")
        fields = [clean(value, max_bytes=limit, label=label)
                  for value, limit, label in zip(row, FIELD_LIMITS, labels)]
        if fields != list(row):
            raise InvalidRecord("output record must already be normalized")
        if any(not field.isascii() for field in fields):
            raise InvalidRecord("SD export requires ASCII fields")
        lines.append("\t".join(fields) + "\n")
    return "".join(lines).encode("ascii")


def fnv1a32(key: str) -> int:
    """FNV-1a 32-bit of the exact ASCII artist_key bytes (not Unicode text)."""
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", key, flags=re.ASCII):
        raise InvalidRecord("FNV key must be the exact lowercase ASCII artist slug")
    value = FNV_OFFSET
    for byte in key.encode("ascii"):
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFF
    return value


def render_index(tsv: bytes) -> bytes:
    """Sorted hash+offset+key index; SHA256 header detects mixed/stale file pairs."""
    if not tsv or not tsv.endswith(b"\n") or b"\r" in tsv:
        raise InvalidRecord("index requires complete LF-terminated TSV records")
    try:
        tsv.decode("ascii")
    except UnicodeDecodeError as error:
        raise InvalidRecord("index requires ASCII TSV") from error
    offsets = []
    position = 0
    seen = set()
    for raw in tsv.splitlines(keepends=True):
        if not raw.endswith(b"\n"):
            raise InvalidRecord("index requires LF-terminated records")
        columns = raw[:-1].split(b"\t")
        if len(columns) != len(FIELD_LIMITS):
            raise InvalidRecord("index requires exactly seven TSV columns")
        key = columns[0].decode("ascii")
        if key in seen:
            raise InvalidRecord("duplicate artist_key in index source")
        seen.add(key)
        # Validate every row, including all original field byte limits.
        render([tuple(column.decode("ascii") for column in columns)])
        offsets.append((fnv1a32(key), key, position))
        position += len(raw)
    offsets.sort(key=lambda item: (item[0], item[1]))
    digest = hashlib.sha256(tsv).hexdigest()
    lines = [f"{INDEX_MAGIC}\t{INDEX_VERSION}\tfnv1a32\t{digest}\t{len(offsets)}\n"]
    lines.extend(f"{hashed:08x}\t{offset}\t{key}\n" for hashed, key, offset in offsets)
    return "".join(lines).encode("ascii")


def validate_index(tsv: bytes, index: bytes) -> None:
    """Offline integrity check; validates hash, byte offsets and exact collision key."""
    try:
        lines = index.decode("ascii").splitlines(keepends=True)
    except UnicodeDecodeError as error:
        raise InvalidRecord("index must be ASCII") from error
    if not lines or any(not line.endswith("\n") for line in lines):
        raise InvalidRecord("index missing header or LF terminator")
    header = lines[0][:-1].split("\t")
    expected = [INDEX_MAGIC, INDEX_VERSION, "fnv1a32", hashlib.sha256(tsv).hexdigest(), str(len(lines) - 1)]
    if header != expected:
        raise InvalidRecord("index version/digest/count mismatch; scan TSV instead")
    found = []
    seen = set()
    for line in lines[1:]:
        parts = line[:-1].split("\t")
        if len(parts) != 3 or not re.fullmatch(r"[0-9a-f]{8}", parts[0]) or not re.fullmatch(r"0|[1-9][0-9]*", parts[1]):
            raise InvalidRecord("invalid index entry")
        key = parts[2]
        hashed, offset = int(parts[0], 16), int(parts[1])
        if hashed != fnv1a32(key) or key in seen:
            raise InvalidRecord("hash mismatch or duplicate key")
        seen.add(key)
        if offset >= len(tsv) or (offset != 0 and tsv[offset - 1:offset] != b"\n"):
            raise InvalidRecord("index byte offset is not at a TSV record boundary")
        record = tsv[offset:].split(b"\n", 1)[0]
        if record.split(b"\t", 1)[0] != key.encode("ascii") or len(record.split(b"\t")) != 7:
            raise InvalidRecord("index key/offset does not match TSV record")
        found.append((hashed, key, offset))
    if found != sorted(found, key=lambda item: (item[0], item[1])):
        raise InvalidRecord("index not sorted by hash then exact key")
    if len(found) != len(tsv.splitlines()):
        raise InvalidRecord("index does not cover every TSV row")


def save_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="wb", dir=path.parent, prefix=".artist-notes-",
                                         suffix=".tmp", delete=False) as file:
            temporary = Path(file.name)
            file.write(data)
        os.replace(temporary, path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def save_pair(tsv_path: Path, index_path: Path, tsv: bytes, index: bytes) -> None:
    """Stage both; publish TSV first, index last (checksum guards torn pairs)."""
    validate_index(tsv, index)
    if tsv_path.parent.resolve() != index_path.parent.resolve():
        raise InvalidRecord("TSV and index must share the same directory")
    if tsv_path.name != "artist-notes.tsv" or index_path.name != "artist-notes.idx":
        raise InvalidRecord("unexpected output names")
    tsv_path.parent.mkdir(parents=True, exist_ok=True)
    staged = []
    try:
        for content in (tsv, index):
            with tempfile.NamedTemporaryFile(mode="wb", dir=tsv_path.parent,
                                             prefix=".artist-notes-", suffix=".tmp", delete=False) as file:
                staged.append(Path(file.name))
                file.write(content)
        os.replace(staged[0], tsv_path)
        os.replace(staged[1], index_path)
    finally:
        for temporary in staged:
            if temporary.exists():
                temporary.unlink()


def self_test() -> None:
    artist_id = "561d854a-6a28-4aa7-8c99-323e6ce46c2a"
    group_id = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"
    row = {"artist_mbid": artist_id, "artist_name": "Miles Davis", "album_mbid": group_id,
           "album_title": "Kind of Blue"}
    def fake(entity, mbid, includes):
        if entity == "artist":
            return {"id": artist_id, "name": "Miles Davis", "type": "Person",
                    "life-span": {"begin": "1926-05-26", "end": "1991-09-28"},
                    "relations": [{"target-type": "instrument", "instrument": {"name": "trumpet"}}]}
        return {"id": group_id, "title": "Kind of Blue", "primary-type": "Album",
                "artist-credit": [{"artist": {"id": artist_id}}]}
    valid, bad = generate([row], fake)
    assert len(valid) == 1 and not bad
    assert valid[0][0] == "miles-davis" and "trumpet" in valid[0][1]
    assert render(valid).count(b"\t") == 6 and render(valid).endswith(b"\n")
    validate_index(render(valid), render_index(render(valid)))
    assert generate([{**row, "album_title": "Made-up record"}], fake)[0] == []
    assert generate([{**row, "artist_name": "Bad\tActor"}], fake)[0] == []
    assert generate([{**row, "artist_name": "Bad\nActor"}], fake)[0] == []
    assert generate([{**row, "album_title": "=HYPERLINK(1)"}], fake)[0] == []
    assert generate([{**row, "album_mbid": "../../secret"}], fake)[0] == []
    assert generate([row, row], fake)[1]  # second entry rejected as duplicate
    assert not generate([row], lambda entity, mbid, includes: {} if entity == "artist" else fake(entity, mbid, includes))[0]
    print("PASS: offline self-test, 7-column TSV, artist/album cross-check, no control injection, no duplicate")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--index", type=Path, default=None, help="Default: artist-notes.idx next to the TSV")
    parser.add_argument("--user-agent", default=USER_AGENT, help="Meaningful MusicBrainz User-Agent with project contact URL")
    parser.add_argument("--verify-only", action="store_true", help="Check current API and print summary; do not write output")
    parser.add_argument("--self-test", action="store_true", help="Use fixtures; no API calls, no files written")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    try:
        if args.output.name != "artist-notes.tsv":
            raise InvalidRecord("output filename must be artist-notes.tsv (avoid clobbering unrelated files)")
        index_path = args.index or args.output.with_name("artist-notes.idx")
        if index_path.name != "artist-notes.idx" or index_path.parent.resolve() != args.output.parent.resolve():
            raise InvalidRecord("index must be artist-notes.idx beside the TSV")
        rows = load_input(args.input)
        if not rows:
            raise InvalidRecord("CSV has no curated entries")
        entries, rejected = generate(rows, MusicBrainzClient(args.user_agent))
        for problem in rejected:
            print("SKIP " + problem, file=sys.stderr)
        if not entries:
            raise InvalidRecord("zero verified entries; existing output not modified")
        data = render(entries)
        index_data = render_index(data)
        validate_index(data, index_data)
        if not args.verify_only:
            save_pair(args.output, index_path, data, index_data)
        print(f"Verified {len(entries)}/{len(rows)}; rejected {len(rejected)}; "
              f"TSV {len(data)} bytes; IDX {len(index_data)} bytes; "
              + ("no output written" if args.verify_only else f"{args.output} + {index_path}"))
        return 0 if not rejected else 2
    except (OSError, csv.Error, InvalidRecord) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
