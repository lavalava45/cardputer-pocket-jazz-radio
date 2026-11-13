#!/usr/bin/env python3
"""Verified CC0 Wikidata jazz musicians -> compact ASCII SD corpus.
Sources are archived in TSV, never occupy precious LCD pixels.
"""
from datetime import datetime, timezone
from pathlib import Path
import argparse
import hashlib
import re
import unicodedata
import requests

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "https://query.wikidata.org/sparql"
QUERY = """SELECT DISTINCT ?item ?name ?description WHERE {
 ?item wdt:P106 wd:Q15981151.
 ?item rdfs:label ?name FILTER(LANG(?name)="en")
 OPTIONAL {?item schema:description ?description FILTER(LANG(?description)="en")}
} LIMIT 1600"""
USER_AGENT = "PocketJazzRadio/0.8 educational offline editor (github.com/lavalava45)"
DATA = ROOT / "data"

# Short independently verified originals. No paraphrase is passed off as quote.
# Attributions/source evidence archived OFF SCREEN.
QUOTES = (
    ("All musicians are subconsciously mathematicians.", "Thelonious Monk", "https://en.wikiquote.org/wiki/Thelonious_Monk"),
    ("My band is my instrument more than the piano.", "Duke Ellington", "https://en.wikiquote.org/wiki/Duke_Ellington"),
    ("The piano ain't got no wrong notes.", "Thelonious Monk", "https://en.wikiquote.org/wiki/Thelonious_Monk"),
    ("My ego only needs a good rhythm section.", "Miles Davis", "https://en.wikiquote.org/wiki/Miles_Davis"),
    ("So go on and play, and if you make a mistake, make it loud so you won't make it next time.", "Art Blakey", "https://en.wikiquote.org/wiki/Art_Blakey"),
)
HISTORY = (
    ("Jazz emerged from African-American musical communities in New Orleans around the turn of the 20th century.", "Origins", "https://en.wikipedia.org/wiki/Jazz"),
    ("Ragtime, blues, spirituals, marches and dance music all contributed to early jazz.", "Origins", "https://en.wikipedia.org/wiki/Jazz"),
    ("Early New Orleans ensembles often improvised several independent melodies together.", "New Orleans jazz", "https://en.wikipedia.org/wiki/Jazz"),
    ("Arranged swing big bands became prominent dance music during the 1930s.", "Swing", "https://en.wikipedia.org/wiki/Jazz"),
    ("Bebop developed in the 1940s, favoring small ensembles and challenging, fast improvisation.", "Bebop", "https://en.wikipedia.org/wiki/Jazz"),
    ("Cool jazz explored restrained textures and longer melodic lines from the late 1940s.", "Cool jazz", "https://en.wikipedia.org/wiki/Jazz"),
    ("Hard bop emerged in the 1950s and drew on gospel, blues and rhythm-and-blues.", "Hard bop", "https://en.wikipedia.org/wiki/Jazz"),
    ("Modal jazz builds improvisation around modes, rather than only chord sequences.", "Modal jazz", "https://en.wikipedia.org/wiki/Jazz"),
    ("Free jazz questioned fixed chord progressions, meter and conventional ensemble roles.", "Free jazz", "https://en.wikipedia.org/wiki/Jazz"),
    ("Jazz fusion combined jazz improvisation with rock rhythms and electric instruments in the late 1960s.", "Jazz fusion", "https://en.wikipedia.org/wiki/Jazz"),
    ("A jazz standard is a composition repeatedly reinterpreted by jazz performers.", "Jazz standard", "https://en.wikipedia.org/wiki/Jazz_standard"),
    ("Jazz evolved differently across countries as musicians adopted local musical traditions.", "Global jazz", "https://en.wikipedia.org/wiki/Jazz"),
    ("Chicago was a key center for hot jazz during the 1920s Jazz Age.", "Jazz Age", "https://en.wikipedia.org/wiki/Jazz_Age"),
    ("Louis Armstrong joined Fletcher Henderson's orchestra as a featured soloist in 1924.", "Jazz Age", "https://en.wikipedia.org/wiki/Jazz_Age"),
    ("Kid Ory's group recorded in Los Angeles during 1922.", "Jazz Age", "https://en.wikipedia.org/wiki/Jazz_Age"),
)


def clean(value: str, limit: int) -> str:
    value = value.replace("\u2013", "-").replace("\u2014", "-").replace("\u2019", "'")
    value = "".join(ch for ch in unicodedata.normalize("NFKD", value) if not unicodedata.combining(ch))
    if not value.isascii() or any(ord(ch) < 32 for ch in value):
        raise ValueError("non-displayable text")
    value = " ".join(value.split())
    if not value or len(value) > limit:
        raise ValueError("empty or oversized text")
    return value


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--facts-only", action="store_true",
                        help="Freeze the existing verified artist TSV; only regenerate curated Spark")
    args = parser.parse_args()
    if args.facts_only:
        # Preserve the already captured Wikidata snapshot byte-for-byte.
        existing = (DATA / "jazz-encyclopedia.tsv").read_bytes()
        if not existing.endswith(b"\n"):
            raise ValueError("Existing encyclopedia missing final LF")
        records = [tuple(row.split("\t")) for row in existing.decode("ascii").splitlines()]
        if any(len(row) != 5 for row in records):
            raise ValueError("Existing encyclopedia has malformed rows")
    else:
        response = requests.get(SOURCE, params={"query": QUERY, "format": "json"},
                                headers={"User-Agent": USER_AGENT}, timeout=55)
        response.raise_for_status()
        original = response.json()["results"]["bindings"]
        entries = {}
        for record in original:
            try:
                uri = record["item"]["value"]
                qid = uri.rsplit("/", 1)[-1]
                if not re.fullmatch(r"Q[1-9][0-9]*", qid):
                    continue
                raw_name = record["name"]["value"]
                if not raw_name.isascii():
                    continue
                name = clean(raw_name, 96)
                key = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
                if not key or len(key) > 64 or key in entries:
                    continue
                bio = clean(record.get("description", {}).get("value") or "jazz musician", 180)
                if not any(s in bio.lower() for s in ("jazz", "music", "sing", "pian", "sax", "trump",
                                                       "bass", "drum", "composer", "clarinet",
                                                       "trombon", "vocal", "guitar", "violin")):
                    bio = clean(bio + "; jazz musician", 180)
                entries[key] = (key, name, bio, "-", "https://www.wikidata.org/wiki/" + qid)
            except (ValueError, KeyError, TypeError):
                continue

        # Preserve prior 36 source-verified MusicBrainz album selections verbatim.
        curated = {}
        for row in (DATA / "artist-notes.tsv").read_text("ascii").splitlines():
            cells = row.split("\t")
            assert len(cells) == 7
            curated[cells[0]] = cells
        for key, cells in curated.items():
            if key in entries:
                prior = entries[key]
                entries[key] = (prior[0], prior[1], prior[2], cells[2], prior[4])
            else:
                name = " ".join(w.capitalize() for w in key.split("-"))
                entries[key] = (key, name, clean(cells[1], 180), clean(cells[2], 110), cells[3])
        records = sorted(entries.values())
    # A compact editorial layer upgrades selected catalog labels to
    # informative, original prose. It also adds absent curated musicians.
    # Source URLs stay exclusively in the TSV and source documentation.
    by_key = {row[0]: row for row in records}
    editorial_bios = (DATA / "jazz-editorial-bios.tsv").read_text("ascii").splitlines()
    added_bios = 0
    for row in editorial_bios:
        fields = row.split("\t")
        if len(fields) != 3 or not fields[2].startswith("https://en.wikipedia.org/wiki/"):
            raise ValueError("Malformed editorial biography")
        key, biography, citation = fields
        if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", key):
            raise ValueError("Malformed editorial musician key")
        biography = clean(biography, 180)
        citation = clean(citation, 224)
        if key in by_key:
            previous = by_key[key]
            by_key[key] = (previous[0], previous[1], biography, previous[3], citation)
        else:
            pretty_name = " ".join(w.capitalize() for w in key.split("-"))
            by_key[key] = (key, clean(pretty_name, 96), biography, "-", citation)
            added_bios += 1
    if len(by_key) < len(records) or len(by_key) > len(records) + len(editorial_bios):
        raise ValueError("Unexpected editorial biography merge")
    records = sorted(by_key.values())
    if len(records) < 360:
        raise ValueError(f"Not enough source-verified jazz artists: {len(records)}")
    biography = ("\n".join("\t".join(row) for row in records) + "\n").encode("ascii")
    assert len(biography) < 3 * 1024 * 1024
    facts = [("FACT",) + record for record in HISTORY]
    editorial = (DATA / "jazz-curated-facts.tsv").read_text("ascii").splitlines()
    for row in editorial:
        fields = row.split("\t")
        if len(fields) != 3 or not fields[2].startswith("https://en.wikipedia.org/wiki/"):
            raise ValueError("Malformed offline curated jazz fact")
        text, subject, source = fields
        facts.append(("FACT", clean(text, 190), clean(subject, 96), clean(source, 224)))
    if len(set(f[1].lower() for f in facts)) != len(facts):
        raise ValueError("Duplicate jazz fact detected")
    facts += [("QUOTE",) + item for item in QUOTES]
    if len(facts) < 150:
        raise ValueError(f"Not enough sourced jazz facts: {len(facts)}")
    facts_bytes = ("\n".join("\t".join(row) for row in facts) + "\n").encode("ascii")
    assert len(facts_bytes) < 512 * 1024
    (DATA / "jazz-encyclopedia.tsv").write_bytes(biography)
    (DATA / "jazz-facts.tsv").write_bytes(facts_bytes)
    digest1 = hashlib.sha256(biography).hexdigest()
    digest2 = hashlib.sha256(facts_bytes).hexdigest()
    (DATA / "JAZZ-ENCYCLOPEDIA-PROVENANCE.md").write_text(
        "# Offline jazz encyclopedia provenance\n\n"
        + "Generated UTC: " + datetime.now(timezone.utc).isoformat() + "\n\n"
        + f"- {len(records)} musician entries; the original 1487-row catalog was imported from Wikidata occupation Q15981151.\n"
        + f"- {len(editorial_bios)} original editorial musician summaries; {added_bios} keys were absent from the input corpus in this regeneration.\n"
        + f"- {len(facts)} FACT/QUOTE records (including {len(editorial)} original editorial facts and {len(QUOTES)} attributed short quotes).\n"
        + "- Source columns reside in TSV on SD and are not shown on LCD.\n"
        + "- Names and short biographies are from Wikidata CC0 structured descriptions;\n"
        + "  albums for 36 original artists remain independently verified MusicBrainz records.\n"
        + "- HISTORICAL FACTS are original concise summaries, see:\n"
        + "  https://en.wikipedia.org/wiki/Jazz\n"
        + "  https://en.wikipedia.org/wiki/Jazz_Age\n"
        + "  https://en.wikipedia.org/wiki/Jazz_standard\n"
        + "- Individual editorial facts have article references in data/jazz-curated-facts.tsv;\n"
        + "- Curated biographies are in data/jazz-editorial-bios.tsv (one topic URL per card).\n"
        + "  no encyclopedia thumbnail or text claims a deep biography for every name.\n"
        + "- Short quotations, with attributions in their respective Wikiquote pages:\n"
        + "  https://en.wikiquote.org/wiki/Duke_Ellington\n"
        + "  https://en.wikiquote.org/wiki/Thelonious_Monk\n"
        + "  https://en.wikiquote.org/wiki/Miles_Davis\n"
        + "  https://en.wikiquote.org/wiki/Art_Blakey\n"
        + "- Some musician cards are deliberately brief: they are not fabricated narrative biographies.\n"
        + "- Monk's 'wrong notes' quotation is a secondary attribution by radio host Phil Schaap,\n"
        + "  not a surviving transcript of Monk directly saying those words.\n"
        + "- A dash album means no verified recommendation; do not claim otherwise.\n"
        + "- Query:\n\n    " + QUERY.replace("\n", "\n    ") + "\n\n"
        + "- SHA-256 jazz-encyclopedia.tsv: " + digest1 + "\n"
        + "- SHA-256 jazz-facts.tsv: " + digest2 + "\n", encoding="utf-8")
    print(f"PASS: {len(records)} performers, {len(facts)} facts/quotes;"
          f" biographies={len(biography)} B, facts={len(facts_bytes)} B")


if __name__ == "__main__":
    main()
