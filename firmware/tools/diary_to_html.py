#!/usr/bin/env python3
"""Offline HTML export of the manually tagged Pocket Jazz Diary.

No network requests. Generated HTML has no JS, remote CSS, or images;
MusicBrainz search links are followed only when the reader clicks.
"""
from __future__ import annotations

import argparse
import csv
from html import escape
from pathlib import Path
from urllib.parse import urlencode

TAGS = {"Explore later", "Beautiful solo", "New favourite"}


def render_diary(raw: str) -> str:
    cards = []
    for row_no, row in enumerate(csv.reader(raw.splitlines(), delimiter="\t"), 1):
        if len(row) != 4:
            raise ValueError(f"Malformed diary TSV row {row_no}: expected 4 columns")
        date, station, track, tag = row
        if not all((date, station, track)) or tag not in TAGS:
            raise ValueError(f"Malformed or unknown diary tag at row {row_no}")
        if any(len(value) > maximum for value, maximum in
               zip(row, (25, 55, 95, 30))):
            raise ValueError(f"Oversized diary value at row {row_no}")
        query = urlencode({"query": track, "type": "recording"})
        cards.append(
            '<article><h2>' + escape(track) + '</h2><p class="tag">' +
            escape(tag) + '</p><p><time>' + escape(date) + '</time> · ' +
            escape(station) + '</p><p><a rel="noreferrer noopener" '
            'target="_blank" href="https://musicbrainz.org/search?' +
            escape(query, quote=True) + '">Search recording on MusicBrainz</a></p></article>'
        )
    content = "\n".join(cards) if cards else "<p>No diary entries yet.</p>"
    return """<!doctype html><html lang="en"><head><meta charset="utf-8">
<title>My Jazz Diary | Pocket Jazz Radio</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font:16px/1.55 system-ui,sans-serif;background:#101a26;color:#e4eaf1;
max-width:780px;margin:2rem auto;padding:0 1.4rem}h1{color:#f4d18c}
article{background:#192a37;border-left:3px solid #d9ae63;padding:1rem 1.3rem;
margin:1.2rem 0;border-radius:5px}h2{font-size:1.2rem;margin:.2rem 0}
.tag{font-weight:bold;color:#f4d18c}a{color:#9ad6df}time{font-variant-numeric:tabular-nums}
</style></head><body><h1>My Jazz Diary</h1>
<p>Personal notes saved from Pocket Jazz Radio. Station names and metadata
are supplied by broadcasters, not audio fingerprinting. A search link is only
opened when you click it; no automatic network calls or external scripts.</p>
""" + content + "</body></html>\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("diary_tsv", type=Path)
    parser.add_argument("output_html", type=Path)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()
    if args.output_html.exists() and not args.overwrite:
        parser.error("Output exists; supply --overwrite to replace your own HTML file")
    source = args.diary_tsv.read_bytes()
    if len(source) > 65536:
        parser.error("Diary exceeds bounded 64 KiB input")
    document = render_diary(source.decode("utf-8"))
    args.output_html.write_text(document, encoding="utf-8")
    print(f"Written {args.output_html}")


if __name__ == "__main__":
    main()