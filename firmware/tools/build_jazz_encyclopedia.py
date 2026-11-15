#!/usr/bin/env python3
"""Verified CC0 Wikidata jazz musicians -> compact ASCII SD corpus.
Sources are archived in TSV, never occupy precious LCD pixels.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import unicodedata
import zlib
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
SEED_SHA256 = "0a288ccbdff85b32c100137ff53f22b0aacfbe52fd15bb0165bdf4f5a6df2dc6"

# Short independently verified originals. No paraphrase is passed off as quote.
# Attributions/source evidence archived OFF SCREEN.
QUOTES = (
    ("All musicians are subconsciously mathematicians.", "Thelonious Monk", "https://en.wikiquote.org/wiki/Thelonious_Monk"),
    ("My band is my instrument more than the piano.", "Duke Ellington", "https://en.wikiquote.org/wiki/Duke_Ellington"),
    ("The piano ain't got no wrong notes.", "Thelonious Monk", "https://en.wikiquote.org/wiki/Thelonious_Monk"),
    ("My ego only needs a good rhythm section.", "Miles Davis", "https://en.wikiquote.org/wiki/Miles_Davis"),
    ("So go on and play, and if you make a mistake, make it loud so you won't make it next time.", "Art Blakey", "https://en.wikiquote.org/wiki/Art_Blakey"),
    # Brief excerpts only; listed Wikiquote pages cite memoirs, interviews or
    # contemporaneous reporting. These do not include song lyrics. The page
    # references establish an attribution trail, not an original recording.
    ("If you still have to ask, shame on you", "Louis Armstrong", "https://en.wikiquote.org/wiki/Louis_Armstrong"),
    ('Playing "Bop" is like playing Scrabble with all the vowels missing.', "Duke Ellington", "https://en.wikiquote.org/wiki/Duke_Ellington"),
    ("In my music, I'm trying to play the truth of what I am.", "Charles Mingus", "https://en.wikiquote.org/wiki/Charles_Mingus"),
    ("I always wanted to be a spontaneous composer.", "Charles Mingus", "https://en.wikiquote.org/wiki/Charles_Mingus"),
    ("As the painter needs his framework of parchment, the improvising musical group needs its framework in time.", "Bill Evans", "https://en.wikiquote.org/wiki/Bill_Evans"),
    ("Jazz is about being in the moment, at every moment.", "Herbie Hancock", "https://en.wikiquote.org/wiki/Herbie_Hancock"),
    ("Jazz is about trusting yourself to respond on the fly.", "Herbie Hancock", "https://en.wikiquote.org/wiki/Herbie_Hancock"),
    ("To be a musician is really something. It goes very, very deep.", "John Coltrane", "https://en.wikiquote.org/wiki/John_Coltrane"),
    ("My music is the spiritual expression of what I am", "John Coltrane", "https://en.wikiquote.org/wiki/John_Coltrane"),
    ("I'm always making a comeback but nobody ever tells me where I've been.", "Billie Holiday", "https://en.wikiquote.org/wiki/Billie_Holiday"),
    ("Let's play the music and not the background!", "Ornette Coleman", "https://en.wikiquote.org/wiki/Ornette_Coleman"),
    ("I wanted to be heard. That's why I'm broke.", "Ornette Coleman", "https://en.wikiquote.org/wiki/Ornette_Coleman"),
    ("Play like you play. Play like you think, and then you got it, if you're going to get it.", "Count Basie", "https://en.wikiquote.org/wiki/Count_Basie"),
    ("I write music to celebrate life.", "Chick Corea", "https://en.wikiquote.org/wiki/Chick_Corea"),
    ("It's the joy of creating. It's a way of life.", "Chick Corea", "https://en.wikiquote.org/wiki/Chick_Corea"),
    ("I think that virtuosity is the first sign of morality in a musician.", "Wynton Marsalis", "https://en.wikiquote.org/wiki/Wynton_Marsalis"),
    ("Calling me a jazz singer was a way of ignoring my musical background", "Nina Simone", "https://en.wikiquote.org/wiki/Nina_Simone"),
    ("The only thing I'm interested in is the music and the musicians.", "Miles Davis", "https://en.wikiquote.org/wiki/Miles_Davis"),
    ("jazz should have the right to take big chances", "Dave Brubeck", "https://en.wikiquote.org/wiki/Dave_Brubeck"),
    ("the whole idea of jazz is that if you think of a phrase, you should be able to play it", "Oscar Peterson", "https://en.wikiquote.org/wiki/Oscar_Peterson"),
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
    parser.add_argument("--check", action="store_true",
                        help="Offline byte-for-byte verify TSV, index, facts and provenance without writing")
    args = parser.parse_args()
    if args.facts_only or args.check:
        # Always render from the immutable pre-enrichment source snapshot.
        # Reading the generated output as input accumulates duplicated facts.
        existing = (DATA / "jazz-encyclopedia-seed.tsv").read_bytes()
        if hashlib.sha256(existing).hexdigest() != SEED_SHA256:
            raise ValueError("Immutable encyclopedia seed SHA-256 mismatch")
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
    # Optional pinned Wikidata claims enrich only the short imported labels.
    # The editorial summaries above retain their exact, reviewed wording.
    snapshot = DATA / "jazz-structured-facts.json"
    music_file = DATA / "jazz-music-evidence.json"
    music_data = json.loads(music_file.read_text("utf-8")) if music_file.exists() else {}
    snapshot_count = 0
    field_counts = {}
    mb_rec_count = mb_band_count = mb_project_count = 0
    if snapshot.exists():
        from enrich_jazz_bios import facts_to_bio, PROPS
        claims = json.loads(snapshot.read_text("utf-8"))
        protected = {row.split("\t", 1)[0] for row in editorial_bios}
        field_counts = {prop: 0 for prop in PROPS}
        updated = []
        for key, name, bio, album, url in records:
            fact = claims.get(key)
            if fact and key not in protected and url == "https://www.wikidata.org/wiki/" + fact.get("qid", ""):
                snapshot_count += 1
                for prop in PROPS:
                    field_counts[prop] += bool(fact.get("facts", {}).get(prop))
                mb = music_data.get(key)
                if mb and (mb.get("qid") != fact.get("qid") or
                           mb.get("status") != "checked_mb" or
                           mb.get("identity_link_strength") not in ("reciprocal", "wikidata_p434_only")):
                    mb = None
                if mb:
                    mb_rec_count += bool(mb.get("recordings"))
                    mb_band_count += bool(mb.get("ensembles"))
                    mb_project_count += bool(mb.get("collaboration_projects"))
                candidate = clean(facts_to_bio(bio, fact, music=mb, artist_key=key), 180)
                bio = candidate
            updated.append((key, name, bio, album, url))
        records = updated
    # Independent editorial research supplements the structured-fact overlay.
    # It is deliberately sparse: only a source-checked musical contribution
    # warrants replacing a machine-generated capsule. Never touch the 63
    # protected historical editor biographies or 36 curated album choices.
    researched_path = DATA / "jazz-researched-context.tsv"
    researched_count = 0
    if researched_path.exists():
        # The 63 original editorial biographies and the preceding 87 reviewed
        # biographies are immutable inputs. New editorials are not accepted
        # without their separate claim-level sources and exact TSV wording.
        from verify_jazz_editorial_dossiers import audit as verify_editorial_dossiers
        verify_editorial_dossiers(DATA)
        rows_by_key = {record[0]: record for record in records}
        protected = {line.split("\t", 1)[0] for line in editorial_bios}
        seen_research = set()
        for line in researched_path.read_text("ascii").splitlines():
            cells = line.split("\t")
            if len(cells) != 3:
                raise ValueError("Malformed researched jazz context: exactly 3 TSV fields required")
            key, summary, source_url = cells
            if key in seen_research or key in protected or key not in rows_by_key:
                raise ValueError("Duplicate, protected or unknown researched jazz key: " + key)
            seen_research.add(key)
            summary = clean(summary, 180)
            source_url = clean(source_url, 224)
            if not source_url.startswith("https://"):
                raise ValueError("Researched jazz capsule requires an HTTPS citation")
            previous = rows_by_key[key]
            rows_by_key[key] = (key, previous[1], summary, previous[3], source_url)
            researched_count += 1
        records = sorted(rows_by_key.values())
    if len(records) < 360:
        raise ValueError(f"Not enough source-verified jazz artists: {len(records)}")
    display_coverage = {
        "recording": sum("Recorded on: " in row[2] for row in records),
        "co_performer": sum("Co-recorded with " in row[2] for row in records),
        "performance_venue": sum("Played at " in row[2] for row in records),
        "ensemble": sum("Member of " in row[2] for row in records),
    }
    biography = ("\n".join("\t".join(row) for row in records) + "\n").encode("ascii")
    assert len(biography) < 3 * 1024 * 1024
    # Byte offsets refer to the exact LF-terminated source, never text-mode
    # line positions (CRLF or UTF-8 multi-byte names would invalidate them).
    offset = 0
    indexed = []
    for row in biography.splitlines(keepends=True):
        key = row.split(b"\t", 1)[0].decode("ascii")
        indexed.append(f"{key}\t{offset}\n")
        offset += len(row)
    assert offset == len(biography) and len(indexed) == len(records)
    index_rows = "".join(indexed).encode("ascii")
    index_header = (f"#PJ-JAZZ-IDX\t2\t{len(biography)}\t{len(records)}\t"
                    f"{zlib.crc32(biography) & 0xffffffff:08x}\t"
                    f"{zlib.crc32(index_rows) & 0xffffffff:08x}\n").encode("ascii")
    index = index_header + index_rows
    facts = [("FACT",) + record for record in HISTORY]
    editorial = (DATA / "jazz-curated-facts.tsv").read_text("ascii").splitlines()
    for row in editorial:
        fields = row.split("\t")
        if len(fields) != 3 or not fields[2].startswith("https://en.wikipedia.org/wiki/"):
            raise ValueError("Malformed offline curated jazz fact")
        text, subject, source = fields
        facts.append(("FACT", clean(text, 190), clean(subject, 96), clean(source, 224)))
    # A source URL alone is insufficient: each Spark sentence must be the
    # exact text of an individually identified evidence claim, with that
    # claim's own URL. Reused by independent read-only provenance audit.
    from verify_jazz_editorial_dossiers import load_verified_editorial_spark, read_dossiers
    dossiers = read_dossiers(DATA)
    for sentence, topic, source in load_verified_editorial_spark(DATA, dossiers):
        facts.append(("FACT", clean(sentence, 190), clean(topic, 96), clean(source, 224)))
    if len(set(f[1].lower() for f in facts)) != len(facts):
        raise ValueError("Duplicate jazz fact detected")
    facts += [("QUOTE",) + item for item in QUOTES]
    if len(facts) < 150:
        raise ValueError(f"Not enough sourced jazz facts: {len(facts)}")
    facts_bytes = ("\n".join("\t".join(row) for row in facts) + "\n").encode("ascii")
    assert len(facts_bytes) < 512 * 1024
    digest1 = hashlib.sha256(biography).hexdigest()
    digest2 = hashlib.sha256(facts_bytes).hexdigest()
    provenance = (
        "# Offline jazz encyclopedia provenance\n\n"
        + "Reproducible offline snapshot; no generation-time timestamp.\n\n"
        + f"- {len(records)} musician entries; the original 1487-row catalog was imported from Wikidata occupation Q15981151.\n"
        + f"- {len(editorial_bios)} original editorial musician summaries, preserved verbatim.\n"
        + f"- {snapshot_count} non-editorial entries have matching locally archived Wikidata statement snapshots.\n"
        + f"- {researched_count} additional independently cited jazz-contribution capsules override machine descriptions; data/jazz-researched-context.tsv is the source of their exact wording and URLs.\n"
        + "- Newly displayed structured music clauses (non-editorial): "
        + ", ".join(f"{k}={v}" for k, v in display_coverage.items()) + ".\n"
        + "- Available claim counts by field within snapshot-matched entries: "
        + ", ".join(f"{p}={n}" for p, n in field_counts.items()) + ".\n"
        + "- Claim IDs, English labels, and explicit absent properties are preserved in data/jazz-structured-facts.json.\n"
        + "- Properties: P569 birth, P570 death, P19 birthplace, P27 citizenship, P1303 instrument, P800 notable work.\n"
        + "- A Wikidata statement is an auditable database assertion, not independently corroborated primary evidence.\n"
        + "- Birthplaces and citizenship are not evidence of performance geography or collaborator relationships.\n"
        + "- P800 notable works are not necessarily recordings; independent MusicBrainz album picks remain limited to 36.\n"
        + f"- Verified direct MusicBrainz performance-recording relations: {mb_rec_count} artists; member-of-band links: {mb_band_count} artists; collaboration-project links: {mb_project_count} artists.\n"
        + "- MusicBrainz relations are attributed community claims, not independently authenticated session evidence.\n"
        + "- Exact identity chain: Wikidata API wbgetentities P434 -> MusicBrainz artist UUID; optional reciprocal MusicBrainz url-rels -> same Wikidata QID.\n"
        + "- Recording proof: MusicBrainz artist inc=recording-rels (instrument/vocal/performer); named co-performers also require recording inc=artist-rels to credit BOTH artists on the SAME recording UUID.\n"
        + "- Venue proof: MusicBrainz artist inc=event-rels (main performer/support act) plus event inc=place-rels (held at); no geography inferred otherwise.\n"
        + "- A recorded title is a deterministic representative of explicit instrument/vocal/performance links, not an assessment of noteworthiness.\n"
        + "- MusicBrainz collaboration-project links do not establish named co-performers; no venues inferred from birthplaces.\n"
        + "- Unavailable fields are omitted, not guessed; this does not establish recording or collaborator coverage.\n"
        + "- data/jazz-encyclopedia.idx v2 pins CRC32 of exact LF ASCII TSV and index rows separately; rebuild both together.\n"
        + "- data/jazz-encyclopedia-seed.tsv is the immutable original corpus, SHA-256: " + SEED_SHA256 + ".\n"
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
        + "- SHA-256 jazz-encyclopedia.idx: " + hashlib.sha256(index).hexdigest() + "\n"
        + "- SHA-256 jazz-facts.tsv: " + digest2 + "\n"
        + "- SHA-256 jazz-structured-facts.json: "
        + (hashlib.sha256(snapshot.read_bytes()).hexdigest() if snapshot.exists() else "no snapshot") + "\n"
        + "- SHA-256 jazz-music-evidence.json: "
        + (hashlib.sha256(music_file.read_bytes()).hexdigest() if music_file.exists() else "no snapshot") + "\n"
        + "- SHA-256 jazz-researched-context.tsv: "
        + (hashlib.sha256(researched_path.read_bytes()).hexdigest() if researched_path.exists() else "no snapshot") + "\n"
        + "- SHA-256 jazz-editorial-bios.tsv: "
        + hashlib.sha256((DATA / "jazz-editorial-bios.tsv").read_bytes()).hexdigest() + "\n")
    outputs = {"jazz-encyclopedia.tsv": biography,
               "jazz-encyclopedia.idx": index,
               "jazz-facts.tsv": facts_bytes,
               "JAZZ-ENCYCLOPEDIA-PROVENANCE.md": provenance.encode("utf-8")}
    if args.check:
        mismatches = [name for name, blob in outputs.items()
                      if not (DATA / name).exists() or (DATA / name).read_bytes() != blob]
        if mismatches:
            raise SystemExit("CHECK FAILED: outdated or mismatched " + ", ".join(mismatches))
    else:
        for name, blob in outputs.items():
            (DATA / name).write_bytes(blob)
    print(f"PASS: {len(records)} performers, {len(facts)} facts/quotes;"
          f" biographies={len(biography)} B, facts={len(facts_bytes)} B")


if __name__ == "__main__":
    main()
