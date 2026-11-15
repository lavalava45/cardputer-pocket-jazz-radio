#!/usr/bin/env python3
"""Reproducible Wikidata claim snapshot for offline Jazz Notes biographies.

Fetch is opt-in. The normal rendering path is strictly offline. Existing editorial
biographies and MusicBrainz album recommendations remain authoritative.
"""
import argparse
import json
import re
import time
from pathlib import Path

import requests

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "data/jazz-encyclopedia-seed.tsv"
SNAPSHOT = ROOT / "data/jazz-structured-facts.json"
API = "https://www.wikidata.org/w/api.php"
USER_AGENT = "PocketJazzRadio/0.8 (educational offline jazz directory; github.com/lavalava45)"
PROPS = ("P569", "P570", "P19", "P27", "P1303", "P800")
LABEL_PROPS = ("P19", "P27", "P1303", "P800")
# Wikidata P800 means "notable work", not "notable musical work".
# For these seven cross-disciplinary subjects the P800 display pick is a
# movie, television series, biography or novel. Preserve its pinned Wikidata
# statement in JSON for audit but do not waste music-encyclopedia LCD space
# on an unrelated work. This is a *display* exclusion, not a claim about the
# artist's musical contribution (or the completeness of Wikidata).
NONMUSICAL_P800_ARTISTS = frozenset((
    "barbra-streisand",     # musical feature film "Yentl", not a jazz work
    "boris-vian",            # "Froth on the Daydream" / novels
    "humphrey-carpenter",    # biography of J. R. R. Tolkien
    "lili-haydn",            # film-score commission "DriverX"
    "michael-snow",          # experimental film "Wavelength"
    "michelle-dockery",      # television series "Downton Abbey"
    "viggo-mortensen",       # feature film "Far from Men"
))


def rows():
    return [line.split("\t") for line in CATALOG.read_text("ascii").splitlines()]


def request(session, params):
    for attempt in range(5):
        try:
            response = session.get(API, params={"action": "wbgetentities", "format": "json", **params},
                                   headers={"User-Agent": USER_AGENT}, timeout=35)
            if response.status_code == 429 and attempt < 4:
                retry = response.headers.get("Retry-After", "")
                delay = int(retry) if retry.isdigit() else 3 * (attempt + 1)
                time.sleep(min(25, max(3, delay)))
                continue
            response.raise_for_status()
            return response.json()["entities"]
        except (requests.RequestException, ValueError):
            if attempt == 4:
                raise
            time.sleep(attempt + 1)


def claim_values(entity, prop):
    result = []
    claims = entity.get("claims", {}).get(prop, [])
    ranked = [c for c in claims if c.get("rank") == "preferred"] or [
        c for c in claims if c.get("rank") == "normal"]
    for claim in ranked:
        value = claim.get("mainsnak", {}).get("datavalue", {}).get("value")
        if prop in ("P569", "P570") and isinstance(value, dict):
            date = value.get("time", "")
            # A year is the minimum precision for an honest compact life date.
            if value.get("precision", 0) >= 9 and re.match(r"^\+[0-9]{4,}-", date):
                result.append({"year": int(date.split("-")[0][1:]), "statement": claim["id"]})
        elif isinstance(value, dict) and re.fullmatch(r"Q[1-9][0-9]*", value.get("id", "")):
            result.append({"qid": value["id"], "statement": claim["id"]})
    return result[:3]


def fetch(limit=None):
    people = [(r[0], r[4].rsplit("/", 1)[-1]) for r in rows()
              if r[4].startswith("https://www.wikidata.org/wiki/Q")]
    if limit is not None:
        people = people[:limit]
    prior = json.loads(SNAPSHOT.read_text("utf-8")) if SNAPSHOT.exists() else {}
    session = requests.Session()
    missing = [(key, qid) for key, qid in people if key not in prior]
    print(f"Available cached: {len(prior)}; requested: {len(people)}; missing: {len(missing)}", flush=True)
    for offset in range(0, len(missing), 25):
        batch = missing[offset:offset + 25]
        entities = request(session, {"ids": "|".join(q for _, q in batch), "props": "claims"})
        labels = set()
        extracted = {}
        for key, qid in batch:
            entity = entities.get(qid, {})
            if entity.get("missing") is not None or not entity.get("claims"):
                continue
            facts = {prop: claim_values(entity, prop) for prop in PROPS}
            extracted[key] = {"qid": qid, "facts": facts}
            for prop in LABEL_PROPS:
                labels.update(v["qid"] for v in facts[prop])
        resolved = {}
        for label_batch_start in range(0, len(labels), 50):
            subset = sorted(labels)[label_batch_start:label_batch_start + 50]
            associated = request(session, {"ids": "|".join(subset), "props": "labels", "languages": "en"})
            for qid, obj in associated.items():
                resolved[qid] = obj.get("labels", {}).get("en", {}).get("value", "")
        for key, record in extracted.items():
            for prop in LABEL_PROPS:
                for v in record["facts"][prop]:
                    v["label"] = resolved.get(v["qid"], "")
            prior[key] = record
        # Crash safe: complete batches are saved atomically; no partial JSON.
        tmp = SNAPSHOT.with_suffix(".json.tmp")
        tmp.write_text(json.dumps(prior, ensure_ascii=True, sort_keys=True, indent=1) + "\n", "utf-8")
        tmp.replace(SNAPSHOT)
        print(f"Cached {len(prior)} ({min(offset + 25, len(missing))}/{len(missing)} fetched)", flush=True)
        time.sleep(2)  # Respect Wikidata interactive API traffic limits.


def clean_label(value):
    if not value or not value.isascii() or len(value) > 58 or any(ord(c) < 32 for c in value):
        return ""
    return " ".join(value.split())


def facts_to_bio(original, record, limit=180, music=None, *,
                 prioritize_music=True, artist_key=None):
    """Keep the source description and append only evidence-bearing facts.

    Every appended field is linked to a concrete Wikidata statement in the JSON.
    Never invent instruments, relationships, albums, dates or birthplaces.
    """
    facts = record["facts"]
    def value(prop):
        for item in facts.get(prop, []):
            label = clean_label(item.get("label", ""))
            if label:
                return label
        return ""
    birth = next((v["year"] for v in facts.get("P569", []) if 1600 <= v["year"] <= 2100), None)
    death = next((v["year"] for v in facts.get("P570", []) if 1600 <= v["year"] <= 2100), None)
    segments = []
    secondary = []
    has_birth = bool(birth and str(birth) in original)
    has_death = bool(death and str(death) in original)
    # A source label may contain a birth year but not a recently added death.
    if birth and death and birth <= death and not has_birth and not has_death:
        secondary.append(f"{birth}-{death}")
    else:
        if birth and not has_birth:
            secondary.append(f"born {birth}")
        if death and not has_death:
            secondary.append(f"died {death}")
    # Wikidata's statement order does not imply a *primary* instrument.
    instruments = []
    for item in facts.get("P1303", []):
        instrument = clean_label(item.get("label", ""))
        if instrument and instrument.lower() not in [v.lower() for v in instruments]:
            instruments.append(instrument)
    if instruments and not all(v.lower() in original.lower() for v in instruments):
        secondary.append("Instruments: " + ", ".join(instruments))
    if music and music.get("status") == "checked_mb":
        # Two direct MusicBrainz artist->recording performer credits on the
        # SAME recording support 'co-recorded', not shared stage or friendship.
        collaborator = next((r for r in music.get("collaborators", [])
                             if r.get("recording_mbid") and clean_label(r.get("name", ""))), None)
        if collaborator:
            name = clean_label(collaborator["name"])
            if name.lower() not in original.lower():
                segments.append("Co-recorded with " + name)
        venue = next((r for r in music.get("performance_venues", [])
                      if r.get("event_mbid") and clean_label(r.get("place_name", ""))), None)
        if venue:
            name = clean_label(venue["place_name"])
            if name.lower() not in original.lower():
                segments.append("Played at " + name)
        # A MusicBrainz instrument/vocal/performer -> recording relationship
        # establishes a credited performance, not the track's importance.
        performed = next((r for r in music.get("recordings", [])
                          if r.get("relation_type") in ("instrument", "vocal", "performer")
                          and clean_label(r.get("title", ""))), None)
        if performed:
            title = clean_label(performed["title"])
            if title.lower() not in original.lower():
                segments.append("Recorded on: " + title)
        group = next((r for r in music.get("ensembles", []) if clean_label(r.get("name", ""))), None)
        if group:
            name = clean_label(group["name"])
            if name.lower() not in original.lower():
                segments.append("Member of " + name)
    # Musical evidence is scarce and meaningful to Jazz Notes; let it occupy
    # screen space before dates, instruments, birthplace and citizenship.
    priority = ("Recorded on: ", "Member of ", "Co-recorded with ", "Played at ")
    if prioritize_music:
        segments.sort(key=lambda clause: next((i for i, prefix in enumerate(priority)
                                              if clause.startswith(prefix)), len(priority)))
    else:
        # Diagnostic baseline reproduces the previous dates/instruments-first
        # order on the SAME evidence snapshot for an apples-to-apples audit.
        segments = secondary + segments
    # P800 is sparse and should fit before lower-priority geographic context.
    # It means notable work; it must never be presented as an album recording.
    work = "" if artist_key in NONMUSICAL_P800_ARTISTS else value("P800")
    if work and work.lower() not in original.lower():
        segments.append("Notable work: " + work)
    if prioritize_music:
        segments.extend(secondary)
    place = value("P19")
    if place and place.lower() not in original.lower():
        segments.append("Born in " + place)
    citizenship = value("P27")
    if citizenship and citizenship.lower() not in original.lower():
        segments.append("Citizenship: " + citizenship)
    result = original.rstrip(" .")
    for segment in segments:
        candidate = result + "; " + segment
        if len(candidate) <= limit:
            result = candidate
    return result + ("." if result != original and not result.endswith(".") and len(result) < limit else "")


def report():
    from collections import Counter
    # Compare the immutable pre-enrichment source against exactly one rendering
    # pass; never enrich the current output again to measure the delta.
    catalog = rows()
    published = {r[0]: r for r in [line.split("\t") for line in
                 (ROOT / "data/jazz-encyclopedia.tsv").read_text("ascii").splitlines()]}
    records = json.loads(SNAPSHOT.read_text("utf-8")) if SNAPSHOT.exists() else {}
    music_file = ROOT / "data/jazz-music-evidence.json"
    music = json.loads(music_file.read_text("utf-8")) if music_file.exists() else {}
    researched_file = ROOT / "data/jazz-researched-context.tsv"
    researched = {}
    if researched_file.exists():
        for line in researched_file.read_text("ascii").splitlines():
            key, summary, source_url = line.split("\t")
            researched[key] = (summary, source_url)
    editorial = {line.split("\t")[0]: line.split("\t")[1:] for line in
                 (ROOT / "data/jazz-editorial-bios.tsv").read_text("ascii").splitlines()}
    baseline = {r[0]: r for r in catalog}
    if set(baseline) != set(published):
        raise ValueError("Generated catalog differs from immutable seed key set")
    changed = []
    eligible = []
    final_changed = 0
    clause_prefixes = ("Recorded on: ", "Member of ", "Co-recorded with ", "Played at ")
    old_display = Counter()
    new_display = Counter()
    for row in catalog:
        key, name, bio, album, source = row
        if key in editorial:
            expected_bio, expected_source = editorial[key]
        else:
            expected_bio, expected_source = bio, source
        facts = records.get(key)
        if facts and key not in editorial and source == "https://www.wikidata.org/wiki/" + facts.get("qid", ""):
            eligible.append(facts)
            mb = music.get(key)
            if mb and (mb.get("qid") != facts.get("qid") or
                       mb.get("status") != "checked_mb" or
                       mb.get("identity_link_strength") not in ("reciprocal", "wikidata_p434_only")):
                mb = None
            expected_bio = facts_to_bio(expected_bio, facts, music=mb, artist_key=key)
            if mb and key not in researched:
                old_bio = facts_to_bio(bio, facts, music=mb, prioritize_music=False, artist_key=key)
                for prefix in clause_prefixes:
                    old_display[prefix.strip()] += prefix in old_bio
                    new_display[prefix.strip()] += prefix in expected_bio
            if expected_bio != bio:
                changed.append({"name": name, "bio": expected_bio})
        if key in researched:
            expected_bio, expected_source = researched[key]
        if expected_bio != bio:
            final_changed += 1
        if (published[key][2], published[key][4]) != (expected_bio, expected_source):
            raise ValueError(f"Generated TSV does not match one-pass immutable source for {key}; rebuild via --facts-only")
    counts = {p: sum(bool(f["facts"].get(p)) for f in eligible) for p in PROPS}
    source_album_count = sum(row[3] != "-" for row in published.values())
    trusted_music = [v for v in music.values() if v.get("status") == "checked_mb"
                     and v.get("identity_link_strength") in ("reciprocal", "wikidata_p434_only")]
    stats = {
        "total": len(catalog),
        "snapshot_records": len(records),
        "editorial_preserved": len(editorial),
        "independently_researched_capsules": len(researched),
        "wikidata_eligible": len(eligible),
        "changed_once_from_immutable_seed": len(changed),
        "final_changed_once_including_researched_capsules": final_changed,
        "claim_presence": counts,
        "sourced_repertoire": {
            "musicbrainz_album_recommendations": source_album_count,
            "explicit_musicbrainz_performance_recordings": sum(len(v.get("recordings", [])) for v in trusted_music),
            "explicit_musicbrainz_member_of_band": sum(len(v.get("ensembles", [])) for v in trusted_music),
            "explicit_musicbrainz_collaboration_project_links": sum(len(v.get("collaboration_projects", [])) for v in trusted_music),
            "named_coperformer_pairs_verified": sum(len(v.get("collaborators", [])) for v in trusted_music),
            "verified_performance_venues": sum(len(v.get("performance_venues", [])) for v in trusted_music),
            "note": "P800 is not a recording. MusicBrainz performance relations are explicit credits; groups and collaboration projects do not prove named co-performers. Birthplaces are not venues."
        },
        "quality": {
            "bios_at_least_70_chars": sum(len(row[2]) >= 70 for row in published.values()),
            "bios_under_60_chars": sum(len(row[2]) < 60 for row in published.values()),
            "duplicate_instrument_segments": sum(
                row[2].count("Instruments:") > 1 or
                ("Instrument:" in row[2] and "Instruments:" in row[2])
                for row in published.values()),
            "machine_music_clauses_displayed_old_dates_first": dict(old_display),
            "machine_music_clauses_displayed_music_first": dict(new_display),
            "machine_music_clause_gain": sum(new_display.values()) - sum(old_display.values())
        },
        "sample": changed[:8]
    }
    print(json.dumps(stats, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true", help="explicitly fetch Wikidata claims and label snapshots")
    parser.add_argument("--limit", type=int, default=None, help="fetch only the first N Wikidata catalog entries")
    args = parser.parse_args()
    if args.fetch:
        fetch(args.limit)
    report()
