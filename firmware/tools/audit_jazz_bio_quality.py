#!/usr/bin/env python3
"""Read-only, conservative Jazz Notes evidence/coverage audit.

Measuring a birth year and instrument is *not* measuring musical importance.
The report deliberately separates factual identity, explicit musical works,
selected album references, protected editorial prose and still-unknown
recordings / co-performers / concert venues.

Never modifies data, regardless of whether an assertion fails. No network.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
import re
import zlib
from enrich_jazz_bios import NONMUSICAL_P800_ARTISTS


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
FIELDS = ("P569", "P570", "P1303", "P19", "P27", "P800")
QID = re.compile(r"Q[1-9][0-9]*\Z")
KEY = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")
MBID = re.compile(r"[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}\Z")


def read_catalog(path: Path) -> list[dict[str, str]]:
    raw = path.read_bytes()
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise ValueError(f"Catalog must use LF, have trailing newline: {path}")
    rows = []
    keys = set()
    for i, line in enumerate(raw.decode("ascii").splitlines(), 1):
        parts = line.split("\t")
        if len(parts) != 5:
            raise ValueError(f"Catalog row {i} has {len(parts)} columns")
        key, name, summary, album, url = parts
        if not KEY.fullmatch(key) or key in keys or not name or not summary or not url.startswith("https://"):
            raise ValueError(f"Catalog row {i}: bad key/duplicate/required fields")
        if any(len(v) > cap for v, cap in
               zip(parts, (64, 96, 180, 110, 224))):
            raise ValueError(f"Catalog row {i}: field exceeds SD parser size")
        keys.add(key)
        rows.append({"key": key, "name": name, "summary": summary, "album": album, "source": url})
    if [r["key"] for r in rows] != sorted(keys):
        raise ValueError("Catalog is not sorted by unique artist key")
    return rows


def load_editorial(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for i, line in enumerate(path.read_text("ascii").splitlines(), 1):
        parts = line.split("\t")
        if len(parts) != 3 or parts[0] in result:
            raise ValueError(f"Editorial biography row {i} malformed/duplicate")
        result[parts[0]] = parts[1]
    return result


def load_researched(path: Path, known: set[str],
                    protected: set[str]) -> dict[str, tuple[str, str]]:
    """Original, individually cited musician capsules; no invented fallback."""
    result: dict[str, tuple[str, str]] = {}
    if not path.exists():
        return result
    for number, line in enumerate(path.read_text("ascii").splitlines(), 1):
        cells = line.split("\t")
        if len(cells) != 3:
            raise ValueError(f"Researched row {number} is not three-column TSV")
        key, summary, url = cells
        if (key not in known or key in protected or key in result or
                not 80 <= len(summary) <= 180 or not summary.isascii() or
                not url.startswith("https://")):
            raise ValueError(f"Researched row {number} has invalid/unsourced contents")
        result[key] = (summary, url)
    return result


def verify_index(tsv: Path, index: Path) -> dict[str, object]:
    raw = tsv.read_bytes()
    idx = index.read_bytes()
    if b"\r" in idx or not idx.endswith(b"\n"):
        raise ValueError("SD index must use complete LF lines")
    header, *records = idx.decode("ascii").splitlines()
    body = ("\n".join(records) + "\n").encode("ascii")
    expected = (f"#PJ-JAZZ-IDX\t2\t{len(raw)}\t{len(records)}\t"
                f"{zlib.crc32(raw) & 0xffffffff:08x}\t"
                f"{zlib.crc32(body) & 0xffffffff:08x}")
    if header != expected:
        raise ValueError("SD index size/count/checksum/version mismatch")
    offsets = []
    offset = 0
    for row in raw.splitlines(keepends=True):
        key = row.split(b"\t", 1)[0].decode("ascii")
        offsets.append((key, offset))
        offset += len(row)
    actual = []
    for record in records:
        pair = record.split("\t")
        if len(pair) != 2 or not pair[1].isdigit():
            raise ValueError("Index key/offset malformed")
        actual.append((pair[0], int(pair[1])))
    if actual != offsets:
        raise ValueError("Index offsets/keys do not point at TSV rows")
    return {"rows": len(records), "bytes": len(idx),
            "sha256": hashlib.sha256(idx).hexdigest()}


def verify_music_evidence(path: Path, catalog: dict[str, dict[str, str]]) -> dict[str, object]:
    """Only identity-bound source relations count as musical knowledge.

    Do not infer co-performers from group membership, an album-credit from a
    recording performance, or concert venues from country of citizenship.
    """
    if not path.exists():
        return {"identity_bound": 0, "artist_identities_checked": 0,
                "with_recorded_performance": 0, "with_member_of_ensemble": 0,
                "with_album_credit": 0, "with_verified_collaboration": 0,
                "with_explicit_performance_venue": 0,
                "musical_evidence_artist_keys": []}
    evidence = json.loads(path.read_text("utf-8"))
    if not isinstance(evidence, dict):
        raise ValueError("Music evidence must be keyed by original artist key")
    identity_bound = 0
    statuses = Counter()
    with_records, with_groups, with_albums = set(), set(), set()
    with_collaborators, with_venues = set(), set()
    for key, row in evidence.items():
        if key not in catalog or not isinstance(row, dict):
            raise ValueError(f"{key}: evidence for unknown artist")
        status = row.get("status")
        if status not in ("pending_mb", "checked_mb", "missing_p434",
                          "ambiguous_p434", "identity_conflict"):
            raise ValueError(f"{key}: invalid evidence status")
        statuses[status] += 1
        mbid, qid = row.get("mbid"), row.get("qid")
        if not QID.fullmatch(str(qid)):
            raise ValueError(f"{key}: invalid QID")
        if row.get("wikidata_url") != f"https://www.wikidata.org/wiki/{qid}":
            raise ValueError(f"{key}: invalid source QID URL")
        if catalog[key]["source"] == "https://www.wikidata.org/wiki/" + qid:
            identity_bound += 1
        elif catalog[key]["source"].startswith("https://www.wikidata.org/wiki/"):
            raise ValueError(f"{key}: QID contradicts catalog")
        if mbid is None:
            if status in ("pending_mb", "checked_mb"):
                raise ValueError(f"{key}: checked/pending status without artist MBID")
            if any(row.get(field) for field in
                   ("recordings", "release_groups", "ensembles",
                    "collaborators", "performance_venues")):
                raise ValueError(f"{key}: relations with ambiguous or absent MBID")
            continue
        if not isinstance(mbid, str) or not MBID.fullmatch(mbid):
            raise ValueError(f"{key}: malformed artist MBID")
        if row.get("musicbrainz_url") != f"https://musicbrainz.org/artist/{mbid}":
            raise ValueError(f"{key}: artist MBID URL mismatch")
        statement = row.get("wikidata_statement")
        if (not isinstance(statement, str) or
                not statement.upper().startswith(qid.upper()+"$") or
                not re.fullmatch(r"[0-9A-Fa-f-]{36}", statement.split("$", 1)[-1])):
            raise ValueError(f"{key}: missing pinned P434 statement ID")
        if status != "checked_mb" and any(row.get(field) for field in
                ("recordings", "release_groups", "ensembles",
                 "collaborators", "performance_venues")):
            raise ValueError(f"{key}: unpublished/retryable MB relations")
        if status == "identity_conflict":
            conflicting = row.get("mb_reciprocal_qids", [])
            if not conflicting or any(not QID.fullmatch(str(q)) for q in conflicting):
                raise ValueError(f"{key}: conflict status without valid competing QIDs")
            continue
        # Do not report unattributed MB rows as source-verified content:
        # five legacy checked_mb rows may lack a verified identity chain.
        trusted_identity = (
            status == "checked_mb" and
            row.get("identity_link_strength") in ("reciprocal", "wikidata_p434_only")
        )
        if row.get("identity_link_strength") == "reciprocal" and (
                row.get("mb_reciprocal_qids") != [qid]):
            raise ValueError(f"{key}: reciprocal QID mismatch")
        recorded_ids = set()
        recorded_titles = {}
        for record in row.get("recordings", []):
            rid = record.get("mbid")
            if (not isinstance(rid, str) or not MBID.fullmatch(rid) or
                    record.get("relation_type") not in ("instrument", "vocal", "performer") or
                    record.get("source_url") != f"https://musicbrainz.org/recording/{rid}" or
                    not record.get("title")):
                raise ValueError(f"{key}: invalid explicit recording performance proof")
            recorded_ids.add(rid)
            recorded_titles[rid] = record["title"]
            if trusted_identity:
                with_records.add(key)
        for record in row.get("raw_recording_examples", []):
            rid = record.get("mbid")
            if (not isinstance(rid, str) or not MBID.fullmatch(rid) or
                    record.get("relation_type") not in ("instrument", "vocal", "performer") or
                    record.get("source_url") != f"https://musicbrainz.org/recording/{rid}" or
                    not record.get("title")):
                raise ValueError(f"{key}: invalid raw recording-performance proof")
            recorded_ids.add(rid)
            recorded_titles[rid] = record["title"]
        for album in row.get("release_groups", []):
            rid = album.get("mbid")
            if (not isinstance(rid, str) or not MBID.fullmatch(rid) or
                    album.get("source_url") != f"https://musicbrainz.org/release-group/{rid}" or
                    not album.get("title")):
                raise ValueError(f"{key}: invalid credited album ID")
            # This is an ALBUM CREDIT, not proof of instrumental performance.
            if trusted_identity:
                with_albums.add(key)
        for ensemble in row.get("ensembles", []):
            rid = ensemble.get("mbid")
            if (not isinstance(rid, str) or not MBID.fullmatch(rid) or
                    ensemble.get("relation_type") != "member of band" or
                    ensemble.get("source_url") != f"https://musicbrainz.org/artist/{mbid}" or
                    not ensemble.get("name")):
                raise ValueError(f"{key}: invalid ensemble membership")
            if trusted_identity:
                with_groups.add(key)
        for collaborator in row.get("collaborators", []):
            fellow = collaborator.get("mbid")
            rid = collaborator.get("recording_mbid")
            if (not isinstance(fellow, str) or not MBID.fullmatch(fellow) or
                    fellow == mbid or rid not in recorded_ids or
                    rid not in row.get("coperformers_checked_recordings", []) or
                    collaborator.get("relation_type") not in ("instrument", "vocal", "performer") or
                    collaborator.get("source_url") != f"https://musicbrainz.org/recording/{rid}" or
                    collaborator.get("recording_title") != recorded_titles.get(rid) or
                    not collaborator.get("name")):
                raise ValueError(f"{key}: collaborator lacks shared recording proof")
            if trusted_identity:
                with_collaborators.add(key)
        named_events = {e.get("mbid"): e for e in row.get("performance_events", [])
                        if isinstance(e, dict)}
        for venue in row.get("performance_venues", []):
            event = venue.get("event_mbid")
            place = venue.get("place_mbid")
            if (not isinstance(event, str) or not MBID.fullmatch(event) or
                    not isinstance(place, str) or not MBID.fullmatch(place) or
                    not venue.get("place_name") or
                    event not in row.get("venues_checked_events", []) or
                    event not in named_events or
                    named_events[event].get("relation_type") not in ("main performer", "support act") or
                    venue.get("event_relation_type") != named_events[event].get("relation_type") or
                    venue.get("source_url") != f"https://musicbrainz.org/event/{event}" or
                    venue.get("place_url") != f"https://musicbrainz.org/place/{place}"):
                raise ValueError(f"{key}: venue lacks named event and place MBIDs")
            if trusted_identity:
                with_venues.add(key)
    meaningful = with_records | with_groups | with_albums | with_collaborators | with_venues
    return {"identity_bound": identity_bound,
            "artist_identities_checked": statuses["checked_mb"],
            "status_counts": dict(sorted(statuses.items())),
            "with_recorded_performance": len(with_records),
            "with_member_of_ensemble": len(with_groups),
            "with_album_credit": len(with_albums),
            "with_verified_collaboration": len(with_collaborators),
            "with_explicit_performance_venue": len(with_venues),
            "musical_evidence_artist_keys": sorted(meaningful)}


def audit(root: Path = ROOT) -> dict[str, object]:
    data = root / "data"
    rows = read_catalog(data / "jazz-encyclopedia.tsv")
    seed = read_catalog(data / "jazz-encyclopedia-seed.tsv")
    indexed = verify_index(data / "jazz-encyclopedia.tsv", data / "jazz-encyclopedia.idx")
    if [r["key"] for r in seed] != [r["key"] for r in rows]:
        raise ValueError("Enriched corpus key order diverged from immutable seed")
    editorial = load_editorial(data / "jazz-editorial-bios.tsv")
    researched = load_researched(
        data / "jazz-researched-context.tsv",
        {row["key"] for row in rows},
        set(editorial)
    )
    snapshot = json.loads((data / "jazz-structured-facts.json").read_text("utf-8"))
    by_key = {r["key"]: r for r in rows}
    field_coverage = {field: 0 for field in FIELDS}
    affected = 0
    audit_error = []
    for original, rendered in zip(seed, rows):
        key = original["key"]
        if key in editorial:
            if rendered["summary"] != editorial[key]:
                audit_error.append(f"{key}: protected editorial text changed")
            continue
        if original["summary"] != rendered["summary"]:
            affected += 1
        claim = snapshot.get(key)
        if not claim:
            continue
        qid = claim.get("qid")
        if not isinstance(qid, str) or not QID.fullmatch(qid):
            audit_error.append(f"{key}: bad Wikidata QID")
        if original["source"] == "https://www.wikidata.org/wiki/" + str(qid):
            for field in FIELDS:
                statements = claim.get("facts", {}).get(field, [])
                if statements:
                    field_coverage[field] += 1
                for item in statements:
                    if (not isinstance(item.get("statement"), str) or
                            not item["statement"].upper().startswith(qid.upper() + "$") or
                            not re.fullmatch(r"[0-9A-Fa-f-]{36}", item["statement"].split("$", 1)[-1])):
                        audit_error.append(f"{key}/{field}: unverifiable statement ID")
    if audit_error:
        raise ValueError("; ".join(audit_error[:12]))
    for key, (bio, url) in researched.items():
        if by_key[key]["summary"] != bio or by_key[key]["source"] != url:
            raise ValueError(f"{key}: researched biography not integrated verbatim")
    albums = [r["key"] for r in rows if r["album"] != "-"]
    # Identity must be checked against the original Wikidata QID in immutable
    # seed, not the enriched display source (some editorials cite Wikipedia).
    music = verify_music_evidence(
        data / "jazz-music-evidence.json",
        {row["key"]: row for row in seed})
    musical_context_keys = (set(editorial) | set(researched) | set(albums)
                            | set(music["musical_evidence_artist_keys"]))
    # Safe, explicit labels: P800 is a 'notable work', NOT a recording.
    displayed_music = {
        "recorded_on": sum("Recorded on: " in row["summary"] for row in rows),
        "co_recorded_with": sum("Co-recorded with " in row["summary"] for row in rows),
        "member_of": sum("Member of " in row["summary"] for row in rows),
        "played_at": sum("Played at " in row["summary"] for row in rows),
        "notable_work_unclassified": sum("Notable work: " in row["summary"]
                                         for row in rows),
    }
    displayed_dates = {"birth": 0, "death": 0}
    for row in rows:
        facts = snapshot.get(row["key"], {}).get("facts", {})
        for prop, label in (("P569", "birth"), ("P570", "death")):
            years = {str(item.get("year")) for item in facts.get(prop, [])
                     if isinstance(item.get("year"), int)}
            if any(year in row["summary"] for year in years):
                displayed_dates[label] += 1
    report = {
        "schema": 1,
        "basis": "offline pinned evidence; conservative credit and venue policy",
        "all_artist_cards": len(rows),
        "verified_wikidata_artist_snapshots": len(snapshot),
        "descriptions_changed_from_immutable_seed": affected,
        "preserved_editorial_biographies": len(editorial),
        "with_source_album_recommendation": len(albums),
        "individual_album_covered_keys": albums,
        "claims": field_coverage,
        "under_60_chars": sum(len(r["summary"]) < 60 for r in rows),
        "at_least_70_chars": sum(len(r["summary"]) >= 70 for r in rows),
        "over_180_chars": sum(len(r["summary"]) > 180 for r in rows),
        "no_instrument_statement": len(rows) - field_coverage["P1303"],
        # Avoid false-positive 'coverage' from citizenship/place of birth.
        "music_significance_verified_by_editorial_review": len(editorial),
        "source_researched_musical_biographies": len(researched),
        "source_reviewed_musical_biographies_total": len(editorial) + len(researched),
        "musical_facts_actually_displayed": displayed_music,
        "p800_nonmusical_display_exclusions": sum(
            key in snapshot and bool(snapshot[key]["facts"].get("P800")) and
            "Notable work: " not in by_key[key]["summary"]
            for key in NONMUSICAL_P800_ARTISTS
        ),
        "p800_unclassified_claims_retained_in_source": field_coverage["P800"],
        "source_backed_life_years_actually_displayed": displayed_dates,
        "music_relationships_from_pinned_musicbrainz": music,
        "musicians_with_at_least_one_source_music_context": len(musical_context_keys),
        "new_evidenced_recordings": music["with_recorded_performance"],
        "new_evidenced_collaborations": music["with_verified_collaboration"],
        "new_evidenced_performance_venues": music["with_explicit_performance_venue"],
        "music_significance_remains_unknown_or_unreviewed": len(rows) - len(musical_context_keys),
        "index": indexed,
    }
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Machine-readable summary to stdout")
    args = parser.parse_args()
    report = audit()
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        for k, v in report.items():
            if k != "individual_album_covered_keys":
                print(f"{k}: {v}")


if __name__ == "__main__":
    main()
