#!/usr/bin/env python3
"""Strict Wikidata P434 -> MusicBrainz evidence snapshot (offline by default).

MusicBrainz artist release-group credit is not evidence that the artist played
on the recording. Explicit artist collaboration and member-of-band relations
are stored under different keys. Birthplace never implies performance venue.
"""
import argparse
from contextlib import contextmanager, nullcontext
import hashlib
import json
import os
import re
import time
import unicodedata
from urllib.parse import unquote
from pathlib import Path

import requests

ROOT = Path(__file__).resolve().parents[1]
SEED = ROOT / "data/jazz-encyclopedia-seed.tsv"
OUTPUT = ROOT / "data/jazz-music-evidence.json"
WD = "https://www.wikidata.org/w/api.php"
MB = "https://musicbrainz.org/ws/2"
AGENT = "PocketJazzRadio/0.8 (offline educational jazz encyclopedia; github.com/lavalava45)"
UUID = re.compile(r"[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}\Z")


def source_rows():
    return [s.split("\t") for s in SEED.read_text("ascii").splitlines()]


def save(records):
    ordered = {key: records[key] for key in sorted(records)}
    # A unique per-process name also prevents a concurrent or older writer
    # from replacing our temp file while preparing an atomic snapshot.
    tmp = OUTPUT.with_name(f"{OUTPUT.name}.{os.getpid()}.tmp")
    # Pin LF bytes independent of Windows text-mode newline translation.
    tmp.write_bytes((json.dumps(ordered, ensure_ascii=True, sort_keys=True, indent=1) + "\n").encode("utf-8"))
    # Windows may briefly deny replacement while an antivirus/indexer or a
    # simultaneous read-only audit has a handle open. Keep the source JSON
    # intact until replacement succeeds. The CLI writer lock prevents stale
    # read-modify-write snapshots from replacing each other, while the
    # per-process temp name avoids clobbering recoverable work.
    for attempt in range(20):
        try:
            tmp.replace(OUTPUT)
            return
        except PermissionError:
            if attempt == 19:
                raise
            time.sleep(min(0.5, 0.05 * (attempt + 1)))


@contextmanager
def exclusive_writer():
    """Non-blocking OS-held single-writer lock covering read through last save.

    Do not delete the lock path on release: unlinking it while another process
    is opening it could create two independent lock files. The OS releases
    the actual byte/file lock when a process exits, even after a crash.
    """
    lockpath = OUTPUT.with_name(OUTPUT.name + ".lock")
    fd = os.open(lockpath, os.O_CREAT | os.O_RDWR, 0o600)
    held = False
    try:
        try:
            if os.name == "nt":
                import msvcrt
                if os.fstat(fd).st_size == 0:
                    os.write(fd, b"0")
                os.lseek(fd, 0, os.SEEK_SET)
                msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            held = True
        except (BlockingIOError, PermissionError, OSError) as exc:
            raise RuntimeError(
                f"Another MusicBrainz evidence writer is active for {OUTPUT}; "
                "do not run concurrent fetch/normalize/link jobs"
            ) from exc
        yield
    finally:
        if held:
            if os.name == "nt":
                os.lseek(fd, 0, os.SEEK_SET)
                msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(fd, fcntl.LOCK_UN)
        os.close(fd)


def get(session, url, params, retries=5):
    for i in range(retries):
        try:
            response = session.get(url, params=params, headers={"User-Agent": AGENT}, timeout=35)
            if response.status_code in (429, 502, 503, 504) and i + 1 < retries:
                hint = response.headers.get("Retry-After", "")
                time.sleep(min(35, max(2, int(hint) if hint.isdigit() else (i + 1) * 3)))
                continue
            response.raise_for_status()
            return response.json()
        except (requests.RequestException, ValueError):
            if i + 1 == retries:
                raise
            time.sleep(i + 1)


def p434_statements(entity):
    candidates = []
    claims = entity.get("claims", {}).get("P434", [])
    preferred = [s for s in claims if s.get("rank") == "preferred"]
    claims = preferred or [s for s in claims if s.get("rank") == "normal"]
    for statement in claims:
        mbid = statement.get("mainsnak", {}).get("datavalue", {}).get("value")
        if isinstance(mbid, str) and UUID.fullmatch(mbid.lower()):
            candidates.append((mbid.lower(), statement["id"]))
    # Multiple distinct MBIDs are ambiguous; do not choose by similarity.
    return list(dict.fromkeys(candidates))


def parse_mb_artist(payload, mbid):
    if payload.get("id", "").lower() != mbid:
        raise ValueError("MusicBrainz responded with the wrong artist ID")
    result = {"ensembles": [], "collaborators": [], "collaboration_projects": [],
              "release_groups": [], "recordings": [], "performance_events": [],
              "performance_venues": []}
    seen = set()
    recordings = []
    performance_events = []
    for rel in payload.get("relations", []):
        typ = rel.get("type")
        if rel.get("target-type") == "event" and typ in ("main performer", "support act"):
            event = rel.get("event", {})
            event_id = event.get("id", "").lower()
            event_name = ascii_label(event.get("name", ""), 95)
            if UUID.fullmatch(event_id) and event_name:
                performance_events.append({"mbid": event_id, "name": event_name,
                                           "relation_type": typ,
                                           "source_url": f"https://musicbrainz.org/event/{event_id}"})
            continue
        if rel.get("target-type") == "recording" and typ in ("instrument", "vocal", "performer"):
            recording = rel.get("recording", {})
            recording_id = recording.get("id", "").lower()
            title = ascii_label(recording.get("title", ""), 85)
            if UUID.fullmatch(recording_id) and title and rel.get("direction") == "forward":
                recordings.append({"mbid": recording_id, "title": title, "relation_type": typ,
                                   "source_url": f"https://musicbrainz.org/recording/{recording_id}"})
            continue
        artist = rel.get("artist", {})
        other_id = artist.get("id", "").lower()
        name = ascii_label(artist.get("name", ""), 70)
        if not UUID.fullmatch(other_id) or not name:
            continue
        if typ == "member of band" and rel.get("direction") == "forward":
            field = "ensembles"
        elif typ == "collaboration" and rel.get("direction") == "forward":
            # MB 'collaboration' often targets the *joint project entity*, not
            # an individual who can independently be called a collaborator.
            field = "collaboration_projects"
        else:
            continue
        # No claim of joint recordings, shared stage, or contemporaneous work.
        if (field, other_id) in seen:
            continue
        seen.add((field, other_id))
        result[field].append({"mbid": other_id, "name": name,
                              "relation_type": typ,
                              "source_url": f"https://musicbrainz.org/artist/{mbid}"})
    for field in ("ensembles", "collaborators", "collaboration_projects"):
        result[field].sort(key=lambda r: (r["name"].lower(), r["mbid"]))
    # A sample is selected deterministically by title, never advertised as a
    # ranking of artistic significance. Keep a capped local proof snapshot.
    deduped = {record["mbid"]: record for record in recordings}
    all_recordings = sorted(deduped.values(), key=lambda r: (r["title"].lower(), r["mbid"]))
    # Preserve raw counts and examples for audit. Do not surface an unrelated
    # spoken stage cue as if it were a noteworthy musical recording.
    result["raw_recording_relationship_count"] = len(deduped)
    result["raw_recording_examples"] = all_recordings[:5]
    result["excluded_nonmusical_title_count"] = sum(not is_musical_recording_title(r["title"])
                                                      for r in all_recordings)
    result["recordings"] = [r for r in all_recordings if is_musical_recording_title(r["title"])][:5]
    unique_events = {event["mbid"]: event for event in performance_events}
    result["performance_events"] = sorted(unique_events.values(), key=lambda e: (e["name"].lower(), e["mbid"]))[:8]
    return result


def ascii_label(value, limit):
    value = unicodedata.normalize("NFKD", value or "")
    value = "".join(ch for ch in value if not unicodedata.combining(ch))
    value = value.replace("\u2019", "'").replace("\u2013", "-")
    value = " ".join(value.split())
    if not value or not value.isascii() or len(value) > limit or any(ord(c) < 32 for c in value):
        return ""
    return value


def is_musical_recording_title(title):
    text = (title or "").strip()
    if (len(re.sub(r"[^A-Za-z0-9]", "", text)) < 3 or
            re.match(r"^[\(\[]", text) or not re.search(r"[A-Za-z]", text)):
        return False
    # Filter announcements, interviews and apparent stage directions; these
    # remain accounted for in raw_recording_relationship_count.
    if re.search(r"\b(applause|announcement|introductions?|interview|spoken word|"
                 r"talking|soundcheck|crowd noise|stage banter|radio spot|"
                 r"band enters|audience reaction)\b", text, flags=re.IGNORECASE):
        return False
    # MusicBrainz also contains explicit spoken-performance tags embedded
    # *inside* apparently valid titles (e.g. Boris Vian's declamation).
    # Quarantine these obvious non-musical credits without discarding their
    # identity/source records or guessing a replacement famous track.
    if re.search(r"\((?:declamation|recitation|spoken word|speech|narration|"
                 r"interview|poetry reading)\)", text, flags=re.IGNORECASE):
        return False
    return True


def reciprocal_wikidata_qids(payload):
    """Read only MusicBrainz artist->Wikidata URL relationships (no fuzzy IDs)."""
    result = set()
    for rel in payload.get("relations", []):
        if rel.get("type") != "wikidata" or rel.get("target-type") != "url":
            continue
        url = rel.get("url", {}).get("resource", "")
        match = re.fullmatch(r"https?://www\.wikidata\.org/wiki/(Q[1-9][0-9]*)/?", url)
        if match:
            result.add(match.group(1))
    return sorted(result)


def fetch_wikidata(records, limit=None):
    catalog = [(r[0], r[4].rsplit("/", 1)[-1]) for r in source_rows()
               if r[4].startswith("https://www.wikidata.org/wiki/Q")]
    if limit is not None:
        catalog = catalog[:limit]
    pending = [(key, qid) for key, qid in catalog if key not in records]
    print(f"Wikidata P434: {len(catalog)} QIDs, {len(pending)} pending", flush=True)
    session = requests.Session()
    for offset in range(0, len(pending), 20):
        group = pending[offset:offset + 20]
        entities = get(session, WD, {"action": "wbgetentities", "format": "json",
                                     "ids": "|".join(q for _, q in group), "props": "claims"})["entities"]
        for key, qid in group:
            values = p434_statements(entities.get(qid, {}))
            ids = {value[0] for value in values}
            records[key] = {"qid": qid, "wikidata_url": f"https://www.wikidata.org/wiki/{qid}",
                            "mbid": values[0][0] if len(ids) == 1 and values else None,
                            "wikidata_statement": values[0][1] if len(ids) == 1 and values else None,
                            "ambiguous_mbids": sorted(ids) if len(ids) > 1 else [],
                            "musicbrainz_url": f"https://musicbrainz.org/artist/{values[0][0]}" if len(ids) == 1 and values else None,
                            "status": "pending_mb" if len(ids) == 1 and values else "ambiguous_p434" if len(ids) > 1 else "missing_p434",
                            "release_groups": [], "recordings": [], "ensembles": [], "collaborators": [], "performance_venues": []}
        save(records)
        print(f"P434 completed {min(offset + len(group), len(pending))}/{len(pending)}", flush=True)
        time.sleep(2)


def fetch_editorial_sitelinks(records):
    # Exact enwiki sitelinks are the ONLY way to attach a new Wikidata QID
    # to Wikipedia-editorial rows, including the historic 36 MB albums.
    editorial = [(r[0], unquote(r[4].rsplit("/", 1)[-1]).replace("_", " "))
                 for r in source_rows() if r[4].startswith("https://en.wikipedia.org/wiki/")]
    pending = [(key, title) for key, title in editorial if key not in records]
    print(f"Editorial enwiki->QID: {len(pending)} pending", flush=True)
    session = requests.Session()
    for offset in range(0, len(pending), 20):
        group = pending[offset:offset+20]
        entities = get(session, WD, {"action": "wbgetentities", "format": "json",
                                     "sites": "enwiki", "titles": "|".join(t for _, t in group),
                                     "props": "claims|sitelinks"})["entities"]
        by_title = {}
        for entity in entities.values():
            title = entity.get("sitelinks", {}).get("enwiki", {}).get("title", "")
            if title:
                by_title[title] = entity
        for key, title in group:
            obj = by_title.get(title)
            if not obj or not re.fullmatch(r"Q[1-9][0-9]*", obj.get("id", "")):
                continue
            qid = obj["id"]
            values = p434_statements(obj)
            ids = {value[0] for value in values}
            records[key] = {"qid": qid,
                            "wikidata_url": f"https://www.wikidata.org/wiki/{qid}",
                            "source_sitelink": f"https://en.wikipedia.org/wiki/{title.replace(' ', '_')}",
                            "mbid": values[0][0] if len(ids) == 1 and values else None,
                            "wikidata_statement": values[0][1] if len(ids) == 1 and values else None,
                            "ambiguous_mbids": sorted(ids) if len(ids) > 1 else [],
                            "musicbrainz_url": f"https://musicbrainz.org/artist/{values[0][0]}" if len(ids) == 1 and values else None,
                            "status": "pending_mb" if len(ids) == 1 and values else "ambiguous_p434" if len(ids) > 1 else "missing_p434",
                            "release_groups": [], "recordings": [], "performance_events": [],
                            "ensembles": [], "collaborators": [], "collaboration_projects": [],
                            "performance_venues": []}
        save(records)
        print(f"Editorial sitelinks {min(offset + len(group), len(pending))}/{len(pending)}", flush=True)
        time.sleep(2)


def resolve_editorial_redirects(records):
    """Recover ONLY exact Wikipedia redirect->canonical pageprops Wikidata IDs.

    Wikipedia redirects are first-party metadata, not name-based/fuzzy joins.
    """
    pending = [(r[0], unquote(r[4].rsplit("/", 1)[-1]).replace("_", " "), r[4])
               for r in source_rows() if r[4].startswith("https://en.wikipedia.org/wiki/")
               and r[0] not in records]
    session = requests.Session()
    for key, old_title, original_url in pending:
        wiki = get(session, "https://en.wikipedia.org/w/api.php", {
            "action": "query", "format": "json", "redirects": "1", "titles": old_title,
            "prop": "pageprops", "ppprop": "wikibase_item"})
        redirects = wiki.get("query", {}).get("redirects", [])
        pages = list(wiki.get("query", {}).get("pages", {}).values())
        if len(redirects) != 1 or redirects[0].get("from") != old_title or len(pages) != 1:
            continue
        new_title = redirects[0].get("to", "")
        page = pages[0]
        qid = page.get("pageprops", {}).get("wikibase_item", "")
        if page.get("title") != new_title or not re.fullmatch(r"Q[1-9][0-9]*", qid):
            continue
        obj = get(session, WD, {"action": "wbgetentities", "format": "json",
                                "ids": qid, "props": "claims|sitelinks"})["entities"].get(qid, {})
        if obj.get("sitelinks", {}).get("enwiki", {}).get("title") != new_title:
            continue
        values = p434_statements(obj)
        ids = {x[0] for x in values}
        records[key] = {"qid": qid, "wikidata_url": f"https://www.wikidata.org/wiki/{qid}",
                        "source_redirect_from": original_url, "source_canonical_title": new_title,
                        "source_sitelink": f"https://en.wikipedia.org/wiki/{new_title.replace(' ', '_')}",
                        "mbid": values[0][0] if len(ids) == 1 and values else None,
                        "wikidata_statement": values[0][1] if len(ids) == 1 and values else None,
                        "ambiguous_mbids": sorted(ids) if len(ids) > 1 else [],
                        "musicbrainz_url": f"https://musicbrainz.org/artist/{values[0][0]}" if len(ids) == 1 and values else None,
                        "status": "pending_mb" if len(ids) == 1 and values else "ambiguous_p434" if len(ids) > 1 else "missing_p434",
                        "release_groups": [], "recordings": [], "performance_events": [],
                        "ensembles": [], "collaborators": [], "collaboration_projects": [],
                        "performance_venues": []}
        save(records)
        print(f"Official redirect: {key}, {old_title} -> {new_title} -> {qid}", flush=True)
        time.sleep(2)


def prioritized_keys(records):
    journey = set()
    journey_file = ROOT / "data/jazz-journey.tsv"
    if journey_file.exists():
        for line in journey_file.read_text("ascii").splitlines():
            fields = line.split("\t")
            if len(fields) > 6:
                journey.add(fields[6])
    editorial = {line.split("\t", 1)[0] for line in
                 (ROOT / "data/jazz-editorial-bios.tsv").read_text("ascii").splitlines()}
    curated = {line.split("\t", 1)[0] for line in
               (ROOT / "data/artist-notes.tsv").read_text("ascii").splitlines()}
    structured_path = ROOT / "data/jazz-structured-facts.json"
    structured = json.loads(structured_path.read_text("utf-8")) if structured_path.exists() else {}
    def priority(key):
        # Curated editor topics are the most useful first; then other artist
        # rows with a verifiable 'notable work' statement; lexical tie break.
        tier = (0 if key in journey else 1 if key in editorial else
                2 if key in curated else 3 if structured.get(key, {}).get("facts", {}).get("P800") else 4)
        return (tier, key)
    return sorted(records, key=priority)


def fetch_musicbrainz(records, limit=None, refresh_identity=False):
    candidates = [(key, records[key]) for key in prioritized_keys(records)
                  if (records[key]["status"] == "pending_mb" if not refresh_identity else
                      records[key]["status"] == "checked_mb" and not records[key].get("identity_link_strength"))]
    if limit is not None:
        candidates = candidates[:limit]
    print(f"MusicBrainz artist relations: {len(candidates)} pending", flush=True)
    session = requests.Session()
    for index, (key, value) in enumerate(candidates, 1):
        mbid = value["mbid"]
        try:
            inc = "url-rels" if refresh_identity else "artist-rels+recording-rels+event-rels+url-rels"
            data = get(session, f"{MB}/artist/{mbid}", {"fmt": "json", "inc": inc})
            if data.get("id", "").lower() != mbid:
                raise ValueError("MusicBrainz artist ID mismatch")
            crosslinks = reciprocal_wikidata_qids(data)
            value["mb_reciprocal_qids"] = crosslinks
            if any(qid != value["qid"] for qid in crosslinks):
                # A contrary identity cannot quietly inherit performance facts.
                value["identity_link_strength"] = "conflicting_reciprocal"
                value["status"] = "identity_conflict"
                for field in ("recordings", "ensembles", "collaborators",
                              "collaboration_projects", "performance_events", "performance_venues"):
                    value[field] = []
                print(f"CONFLICT: {key} Wikidata {value['qid']} vs MB reciprocal {crosslinks}", flush=True)
                save(records)
                time.sleep(1.25)
                continue
            value["identity_link_strength"] = "reciprocal" if crosslinks else "wikidata_p434_only"
            if refresh_identity:
                # Previously fetched data came from an older parser: discard
                # stage cues from the display sample, retain their raw witness.
                original = value.get("recordings", [])
                value["raw_recording_examples"] = original
                value["recordings"] = [r for r in original
                                       if is_musical_recording_title(r.get("title", ""))]
                value["recording_sampling_policy"] = "legacy_5_sample_filtered"
            else:
                value.update(parse_mb_artist(data, mbid))
                value["recording_sampling_policy"] = "complete_artist_relations_filtered"
            value["status"] = "checked_mb"
        except (requests.RequestException, ValueError) as exc:
            # Keep retryable: never silently convert API failures to no facts.
            print(f"RETRYABLE {key}: {type(exc).__name__}", flush=True)
        save(records)
        print(f"MB artist {index}/{len(candidates)}: {key}: {value['status']}", flush=True)
        time.sleep(1.25)  # MusicBrainz public API: at most one request per second.


def fetch_venues(records, limit=None):
    jobs = []
    # Spread bounded requests across priority artists before trying more
    # events by the same person.
    for event_index in range(8):
        for key in prioritized_keys(records):
            value = records[key]
            if (value.get("status") != "checked_mb" or
                    value.get("identity_link_strength") not in ("reciprocal", "wikidata_p434_only")):
                continue
            events = value.get("performance_events", [])
            if event_index < len(events):
                event = events[event_index]
                if event["mbid"] not in set(value.get("venues_checked_events", [])):
                    jobs.append((key, event))
    if limit is not None:
        jobs = jobs[:limit]
    print(f"Venue evidence: {len(jobs)} event pages pending", flush=True)
    session = requests.Session()
    for n, (key, event) in enumerate(jobs, 1):
        eid = event["mbid"]
        try:
            data = get(session, f"{MB}/event/{eid}", {"fmt": "json", "inc": "place-rels"})
            places = parse_event_venues(data, event)
            # One event with multiple held-at places does not prove at which
            # specific site this artist performed. Keep the source evidence,
            # but never turn those places into a categorical LCD sentence.
            if len({place["place_mbid"] for place in places}) == 1:
                records[key]["performance_venues"].extend(places[:1])
            elif places:
                records[key].setdefault("ambiguous_event_places", []).extend(places)
            # Track checked events explicitly even when there is no venue.
            records[key].setdefault("venues_checked_events", []).append(eid)
            save(records)
        except (requests.RequestException, ValueError) as exc:
            print(f"RETRYABLE venue {key}/{eid}: {type(exc).__name__}", flush=True)
        print(f"Venue event {n}/{len(jobs)}", flush=True)
        time.sleep(1.25)


def parse_event_venues(payload, event):
    event_id = event["mbid"]
    if payload.get("id", "").lower() != event_id:
        raise ValueError("Event ID mismatch")
    if event.get("relation_type") not in ("main performer", "support act"):
        return []
    result = []
    for rel in payload.get("relations", []):
        if rel.get("type") != "held at" or rel.get("direction") != "forward":
            continue
        place = rel.get("place", {})
        place_id = place.get("id", "").lower()
        place_name = ascii_label(place.get("name", ""), 70)
        if UUID.fullmatch(place_id) and place_name:
            result.append({"event_mbid": event_id, "event_name": event["name"],
                           "event_relation_type": event["relation_type"],
                           "place_mbid": place_id, "place_name": place_name,
                           "source_url": f"https://musicbrainz.org/event/{event_id}",
                           "place_url": f"https://musicbrainz.org/place/{place_id}"})
    return result


def quarantine_ambiguous_event_venues(records):
    """Upgrade older snapshots: multiple venues for one event are uncertain."""
    removed = 0
    for row in records.values():
        by_event = {}
        for place in row.get("performance_venues", []):
            by_event.setdefault(place["event_mbid"], []).append(place)
        safe = []
        for places in by_event.values():
            if len({place["place_mbid"] for place in places}) == 1:
                safe.append(places[0])
            else:
                row.setdefault("ambiguous_event_places", []).extend(places)
                removed += len(places)
        row["performance_venues"] = sorted(safe, key=lambda x: (x["event_mbid"], x["place_mbid"]))
    save(records)
    print(f"Quarantined {removed} unspecific multi-venue event links", flush=True)


def parse_recording_coperformers(payload, owner_mbid, recording_mbid, recording_title):
    """Only count two named artist performance links on SAME MusicBrainz recording.

    Neither shared album credit nor ensemble membership establishes this fact.
    """
    if payload.get("id", "").lower() != recording_mbid:
        raise ValueError("Recording ID mismatch")
    performers = {}
    for rel in payload.get("relations", []):
        if rel.get("target-type") != "artist" or rel.get("type") not in ("instrument", "vocal", "performer"):
            continue
        performer = rel.get("artist", {})
        mid = performer.get("id", "").lower()
        name = ascii_label(performer.get("name", ""), 70)
        if UUID.fullmatch(mid) and name:
            performers[mid] = (name, rel.get("type"))
    # Owner must be explicitly credited on this recording in this API view.
    if owner_mbid not in performers:
        return []
    result = []
    for mid, (name, typ) in sorted(performers.items(), key=lambda t: (t[1][0].lower(), t[0])):
        if mid != owner_mbid:
            result.append({"mbid": mid, "name": name, "relation_type": typ,
                           "recording_mbid": recording_mbid, "recording_title": recording_title,
                           "source_url": f"https://musicbrainz.org/recording/{recording_mbid}"})
    return result[:5]


def coperformer_priority_keys(records, prefer_display=False):
    priority = prioritized_keys(records)
    if prefer_display:
        editorial = {line.split("\t", 1)[0] for line in
                     (ROOT / "data/jazz-editorial-bios.tsv").read_text("ascii").splitlines()}
        researched_file = ROOT / "data/jazz-researched-context.tsv"
        researched = ({line.split("\t", 1)[0] for line in researched_file.read_text("ascii").splitlines()}
                      if researched_file.exists() else set())
        # These two layers override facts_to_bio() in the final TSV. New
        # MusicBrainz credits for them are worth retaining as source evidence,
        # but do not become visible. Prioritize entries whose displayed text
        # can actually improve; stable sort preserves original journey
        # relevance within that tier.
        priority.sort(key=lambda key: key in editorial or key in researched)
    return priority


def fetch_coperformers(records, limit=None, prefer_display=False):
    jobs = []
    priority = coperformer_priority_keys(records, prefer_display)
    for recording_index in range(2):
        for key in priority:
            value = records[key]
            if (value.get("status") != "checked_mb" or
                    value.get("identity_link_strength") not in ("reciprocal", "wikidata_p434_only")):
                continue
            recordings = value.get("recordings", [])
            if recording_index < len(recordings):
                recording = recordings[recording_index]
                if recording["mbid"] not in set(value.get("coperformers_checked_recordings", [])):
                    jobs.append((key, recording))
    if limit is not None:
        jobs = jobs[:limit]
    print(f"Explicit co-performer recordings: {len(jobs)} pages pending", flush=True)
    session = requests.Session()
    for n, (key, recording) in enumerate(jobs, 1):
        rid = recording["mbid"]
        try:
            payload = get(session, f"{MB}/recording/{rid}", {"fmt": "json", "inc": "artist-rels"})
            confirmed = parse_recording_coperformers(payload, records[key]["mbid"], rid, recording["title"])
            current = records[key].setdefault("collaborators", [])
            known = {x["mbid"] for x in current}
            current.extend(x for x in confirmed if x["mbid"] not in known)
            records[key].setdefault("coperformers_checked_recordings", []).append(rid)
            save(records)
        except (requests.RequestException, ValueError) as exc:
            print(f"RETRYABLE co-performer {key}/{rid}: {type(exc).__name__}", flush=True)
        print(f"Co-performer recording {n}/{len(jobs)}", flush=True)
        time.sleep(1.25)


def attach_curated_album_references(records):
    """Link ONLY existing human-selected MusicBrainz release-group IDs.

    Album selection is editorial, not evidence that every artist credit means
    the person physically performed on each recording of that album.
    """
    total = linked = mismatches = 0
    for line in (ROOT / "data/artist-notes.tsv").read_text("ascii").splitlines():
        cells = line.split("\t")
        if len(cells) != 7:
            raise ValueError("Malformed trusted artist-notes TSV")
        key, _, album, artist_url, release_url, _, _ = cells
        total += 1
        obj = records.get(key)
        artist_id = artist_url.rsplit("/", 1)[-1].lower()
        release_id = release_url.rsplit("/", 1)[-1].lower()
        if (obj is None or obj.get("status") != "checked_mb" or
                obj.get("identity_link_strength") not in ("reciprocal", "wikidata_p434_only") or
                obj.get("mbid") != artist_id or
                not UUID.fullmatch(release_id)):
            mismatches += 1
            continue
        obj["release_groups"] = [{"mbid": release_id, "title": album,
                                  "source_url": release_url,
                                  "artist_source_url": artist_url,
                                  "selection": "human_curated_musicbrainz_release_group",
                                  "performance_proof": False}]
        linked += 1
    save(records)
    print(f"Curated MB albums: {linked}/{total} linked via exact P434 MBID; {mismatches} skipped", flush=True)


def normalize_display_samples(records):
    """Reapply conservative musical-title filtering without erasing raw proof."""
    changed = 0
    for value in records.values():
        if value.get("status") != "checked_mb":
            continue
        old = value.get("recordings", [])
        value.setdefault("raw_recording_examples", list(old))
        new = [r for r in old if is_musical_recording_title(r.get("title", ""))]
        if len(new) != len(old):
            value["recordings"] = new
            changed += 1
    save(records)
    print(f"Reapplied musical-title filter to {changed} artist display samples; raw proof retained", flush=True)


def report(records):
    from collections import Counter
    counts = Counter(r["status"] for r in records.values())
    trusted = [v for v in records.values() if v.get("status") == "checked_mb" and
               v.get("identity_link_strength") in ("reciprocal", "wikidata_p434_only")]
    quality = Counter()
    for row in records.values():
        musical = bool(row.get("recordings") or row.get("ensembles") or
                       row.get("collaborators") or row.get("performance_venues"))
        if row.get("status") == "checked_mb" and musical:
            if row.get("identity_link_strength") == "reciprocal":
                quality["A_reciprocal_identity_plus_explicit_music_links"] += 1
            elif row.get("identity_link_strength") == "wikidata_p434_only":
                quality["B_one_way_identity_plus_explicit_music_links"] += 1
            else:
                quality["C_music_links_identity_pending_recheck"] += 1
        elif row.get("status") == "checked_mb":
            quality["C_identity_checked_no_explicit_music_links"] += 1
        else:
            quality["D_unchecked_missing_ambiguous_or_conflicting"] += 1
    print(json.dumps({"source_seed_sha256": hashlib.sha256(SEED.read_bytes()).hexdigest(),
                      "qid_rows": len(records), "statuses": dict(sorted(counts.items())),
                      "mb_artist_pages_checked": counts.get("checked_mb", 0),
                      "mb_identity_reciprocal_confirmed": sum(v.get("status") == "checked_mb" and
                                                               v.get("identity_link_strength") == "reciprocal"
                                                               for v in records.values()),
                      "mb_identity_one_way_only": sum(v.get("status") == "checked_mb" and
                                                       v.get("identity_link_strength") == "wikidata_p434_only"
                                                       for v in records.values()),
                      "mb_artist_pages_identity_not_rechecked": sum(v.get("status") == "checked_mb" and
                                                                      not v.get("identity_link_strength")
                                                                      for v in records.values()),
                      "identity_link_strength": dict(sorted(Counter(r.get("identity_link_strength", "unverified")
                                                                for r in records.values()).items())),
                      "quality_tiers": dict(sorted(quality.items())),
                      "curated_album_source_links": sum(len(v.get("release_groups", [])) for v in trusted),
                      "verified_ensemble_member_relations": sum(len(v["ensembles"]) for v in trusted),
                      "artists_with_ensembles": sum(bool(v["ensembles"]) for v in trusted),
                      "verified_explicit_collaboration_relations": sum(len(v["collaborators"]) for v in trusted),
                      "artists_with_explicit_coperformers": sum(bool(v["collaborators"]) for v in trusted),
                      "explicit_collaboration_project_links": sum(len(v.get("collaboration_projects", [])) for v in trusted),
                      "verified_recording_performances": sum(len(v["recordings"]) for v in trusted),
                      "sampled_explicit_recording_links_not_all_recordings": True,
                      "musicbrainz_artist_recording_relation_total_where_loaded": sum(
                          v.get("raw_recording_relationship_count", 0) for v in trusted),
                      "artists_with_verified_recordings": sum(bool(v["recordings"]) for v in trusted),
                      "verified_performance_venues": sum(len(v["performance_venues"]) for v in trusted),
                      "ambiguous_event_venue_links_quarantined": sum(len(v.get("ambiguous_event_places", [])) for v in records.values()),
                      "identity_unchecked_raw_music_relations_excluded": sum(
                          len(v.get("recordings", [])) + len(v.get("ensembles", []))
                          for v in records.values() if v.get("status") == "checked_mb"
                          and v.get("identity_link_strength") not in ("reciprocal", "wikidata_p434_only"))}, indent=2))


def validate(records, *, check_snapshot_bytes=True):
    """Refuse orphaned citations and relationships that lack their source edge."""
    seed = {r[0]: r for r in source_rows()}
    if check_snapshot_bytes and OUTPUT.exists():
        expected = (json.dumps({k: records[k] for k in sorted(records)},
                               ensure_ascii=True, sort_keys=True, indent=1) + "\n").encode("utf-8")
        # Legacy CRLF snapshots are accepted for an in-progress fetch; a final
        # canonical `save()` normalizes to LF without changing factual content.
        if OUTPUT.read_bytes().replace(b"\r\n", b"\n") != expected:
            raise ValueError("Music snapshot not deterministic canonical sorted JSON")
    for key, row in records.items():
        if key not in seed:
            raise ValueError(f"Music snapshot has a key absent from immutable seed: {key}")
        qid = row.get("qid", "")
        if not re.fullmatch(r"Q[1-9][0-9]*", qid):
            raise ValueError(f"Invalid Wikidata QID on {key}")
        if seed[key][4].startswith("https://www.wikidata.org/wiki/"):
            if seed[key][4].rsplit("/", 1)[-1] != qid:
                raise ValueError(f"Misattached Wikidata item: {key}")
        elif row.get("source_redirect_from"):
            if (row["source_redirect_from"] != seed[key][4] or
                    not row.get("source_canonical_title") or
                    row.get("source_sitelink") != "https://en.wikipedia.org/wiki/" + row["source_canonical_title"].replace(" ", "_")):
                raise ValueError(f"Unverified official Wikipedia redirect mapping: {key}")
        else:
            page_url = row.get("source_sitelink")
            if not page_url or unquote(page_url).replace("_", " ") != unquote(seed[key][4]).replace("_", " "):
                raise ValueError(f"Unverified editorial page->QID sitelink: {key}")
        mid = row.get("mbid")
        if row["status"] == "ambiguous_p434":
            if mid is not None or len(set(row.get("ambiguous_mbids", []))) < 2:
                raise ValueError(f"Invalid unresolved multiple P434 links: {key}")
        if row["status"] == "missing_p434" and mid is not None:
            raise ValueError(f"Claimed MusicBrainz ID despite absent P434: {key}")
        if mid is not None and not UUID.fullmatch(mid):
            raise ValueError(f"Invalid linked MusicBrainz artist ID: {key}")
        if mid and row.get("musicbrainz_url") != f"https://musicbrainz.org/artist/{mid}":
            raise ValueError(f"Invalid MusicBrainz artist source URL: {key}")
        statement = row.get("wikidata_statement")
        if mid and (not statement or not statement.upper().startswith(qid + "$")):
            raise ValueError(f"Missing Wikidata P434 statement ID: {key}")
        strength = row.get("identity_link_strength")
        reciprocal = row.get("mb_reciprocal_qids", [])
        if strength == "reciprocal" and reciprocal != [qid]:
            raise ValueError(f"Non-matching reciprocal identity: {key}")
        if strength == "wikidata_p434_only" and reciprocal:
            raise ValueError(f"One-way identity unexpectedly has a reciprocal: {key}")
        if row["status"] == "identity_conflict":
            if not reciprocal or all(other == qid for other in reciprocal):
                raise ValueError(f"Unsubstantiated identity conflict: {key}")
        if row["status"] == "checked_mb" and strength not in (
                None, "reciprocal", "wikidata_p434_only"):
            raise ValueError(f"Checked artist carries invalid identity strength: {key}")
        if row["status"] != "checked_mb":
            if row.get("recordings") or row.get("collaborators") or row.get("performance_venues"):
                raise ValueError(f"Unverified artist identity has music evidence: {key}")
            continue
        for recording in row.get("recordings", []):
            rid = recording.get("mbid", "")
            if (not UUID.fullmatch(rid) or recording.get("relation_type") not in
                    ("instrument", "vocal", "performer") or
                    recording.get("source_url") != f"https://musicbrainz.org/recording/{rid}"):
                raise ValueError(f"Unproven recording performance: {key}")
        for band in row.get("ensembles", []):
            if (band.get("relation_type") != "member of band" or
                    not UUID.fullmatch(band.get("mbid", "")) or
                    band.get("source_url") != f"https://musicbrainz.org/artist/{mid}"):
                raise ValueError(f"Non-member relationship used as ensemble proof: {key}")
        for project in row.get("collaboration_projects", []):
            if (project.get("relation_type") != "collaboration" or
                    not UUID.fullmatch(project.get("mbid", "")) or
                    project.get("source_url") != f"https://musicbrainz.org/artist/{mid}"):
                raise ValueError(f"Non-collaboration project relation: {key}")
        performance_ids = {r["mbid"] for r in (row.get("recordings", []) +
                                               row.get("raw_recording_examples", []))}
        event_ids = set()
        for event in row.get("performance_events", []):
            event_id = event.get("mbid", "")
            if (not UUID.fullmatch(event_id) or
                    event.get("relation_type") not in ("main performer", "support act") or
                    event.get("source_url") != f"https://musicbrainz.org/event/{event_id}"):
                raise ValueError(f"Event lacks direct MusicBrainz artist performance link: {key}")
            event_ids.add(event_id)
        for coworker in row.get("collaborators", []):
            recording_id = coworker.get("recording_mbid", "")
            target_id = coworker.get("mbid", "")
            if (recording_id not in performance_ids or
                    not UUID.fullmatch(target_id) or target_id == mid or
                    coworker.get("relation_type") not in ("instrument", "vocal", "performer") or
                    coworker.get("source_url") != f"https://musicbrainz.org/recording/{recording_id}"):
                raise ValueError(f"Collaborator without shared recording proof: {key}")
        for place in row.get("performance_venues", []):
            event_id = place.get("event_mbid", "")
            place_id = place.get("place_mbid", "")
            if (event_id not in event_ids or not UUID.fullmatch(place_id) or
                    place.get("event_relation_type") not in ("main performer", "support act") or
                    place.get("source_url") != f"https://musicbrainz.org/event/{event_id}" or
                    place.get("place_url") != f"https://musicbrainz.org/place/{place_id}"):
                raise ValueError(f"Venue without artist-performed event link: {key}")
        grouped_venues = {}
        for place in row.get("performance_venues", []):
            grouped_venues.setdefault(place["event_mbid"], set()).add(place["place_mbid"])
        if any(len(places) > 1 for places in grouped_venues.values()):
            raise ValueError(f"Ambiguous multi-venue event improperly treated as specific performance venue: {key}")



def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--fetch-wikidata", action="store_true", help="explicitly request pinned P434 statements")
    ap.add_argument("--fetch-editorial", action="store_true", help="match exact Wikipedia editorial sitelinks to Wikidata QIDs")
    ap.add_argument("--resolve-editorial-redirects", action="store_true", help="use official Wikipedia redirects to resolve remaining editorial QIDs")
    ap.add_argument("--fetch-musicbrainz", action="store_true", help="explicitly request artist relationship records")
    ap.add_argument("--refresh-identity", action="store_true", help="refresh only previously checked rows lacking MusicBrainz reciprocal identity validation")
    ap.add_argument("--fetch-venues", action="store_true", help="resolve explicit performed events to named held-at places")
    ap.add_argument("--fetch-coperformers", action="store_true", help="verify two named performers on the same MusicBrainz recording")
    ap.add_argument("--prefer-display", action="store_true", help="for co-performer lookups prioritize biographies not protected by editorial overlays")
    ap.add_argument("--link-curated-albums", action="store_true", help="link existing 36 editor picks only if P434 MBID agrees exactly")
    ap.add_argument("--normalize-recordings", action="store_true", help="re-filter existing display samples while retaining original MB evidence")
    ap.add_argument("--quarantine-multivenue", action="store_true", help="archive uncertain multi-venue event links without projecting them into biographies")
    ap.add_argument("--validate", action="store_true", help="offline evidence graph/source integrity checks")
    ap.add_argument("--limit", type=int, help="limit the number of QIDs or MB artist calls this run")
    args = ap.parse_args()
    mutating = any((args.fetch_wikidata, args.fetch_editorial,
                    args.resolve_editorial_redirects, args.fetch_musicbrainz,
                    args.fetch_venues, args.fetch_coperformers,
                    args.link_curated_albums, args.normalize_recordings,
                    args.quarantine_multivenue))
    # Take the lock BEFORE reading the snapshot. A lock acquired only during
    # save() cannot prevent two writers with stale in-memory copies.
    with exclusive_writer() if mutating else nullcontext():
        records = json.loads(OUTPUT.read_text("utf-8")) if OUTPUT.exists() else {}
        if args.fetch_wikidata:
            fetch_wikidata(records, args.limit)
        if args.fetch_editorial:
            fetch_editorial_sitelinks(records)
        if args.resolve_editorial_redirects:
            resolve_editorial_redirects(records)
        if args.fetch_musicbrainz:
            fetch_musicbrainz(records, args.limit, refresh_identity=args.refresh_identity)
        if args.fetch_venues:
            fetch_venues(records, args.limit)
        if args.fetch_coperformers:
            fetch_coperformers(records, args.limit, prefer_display=args.prefer_display)
        if args.link_curated_albums:
            attach_curated_album_references(records)
        if args.normalize_recordings:
            normalize_display_samples(records)
        if args.quarantine_multivenue:
            quarantine_ambiguous_event_venues(records)
        if args.validate:
            validate(records)
            print(f"PASS: {len(records)} pinned source-key identities validated", flush=True)
        report(records)


if __name__ == "__main__":
    main()
