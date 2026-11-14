"""Host-side offline validation: python tests/test_playlists.py."""
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
FILES = sorted((ROOT / "playlists").glob("*.m3u"))
assert len(FILES) >= 4, "Need three curated jazz playlists and a user template"
MAX_GROUPS, MAX_STATIONS, MAX_NAME, MAX_URL = 12, 64, 55, 191
assert len(FILES) <= MAX_GROUPS
total = 0
seen_urls = set()
for filename in FILES:
    text = filename.read_text("utf-8-sig")
    assert text.splitlines()[0].strip() == "#EXTM3U", filename
    pending_name = None
    stations = 0
    for raw in text.splitlines()[1:]:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#EXTINF:"):
            assert "," in line, (filename, line)
            pending_name = line.split(",", 1)[1]
            assert 0 < len(pending_name.encode()) <= MAX_NAME, (filename, pending_name)
        elif line.startswith("#"):
            continue
        else:
            parsed = urlparse(line)
            assert parsed.scheme in {"http", "https"} and parsed.netloc, (filename, line)
            assert len(line.encode()) <= MAX_URL, (filename, line)
            assert line not in seen_urls, f"Duplicate stream URL: {line}"
            assert pending_name, (filename, line)
            stations += 1
            total += 1
            seen_urls.add(line)
            pending_name = None
    if "my-stations" not in filename.name:
        assert stations >= 2, filename
    print(f"PASS {filename.name}: {stations} stations")
assert total > 8 and total <= MAX_STATIONS, total
assert total == 51, f"Current editable curated catalog drifted: {total} != 51"
assert MAX_STATIONS - total >= 8, "Keep at least eight station slots free for personal additions"
assert not any("blues" in u.lower() for u in seen_urls), "Dedicated hard-blues stations belong outside curated jazz"
assert not (ROOT / "playlists" / "06-latin-soul-blues.m3u").exists(), "Legacy SD playlist must not ship in current archive"
print(f"PASS {total} unique stream URLs across {len(FILES)} editable M3U playlists")
