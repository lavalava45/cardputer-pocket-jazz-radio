#!/usr/bin/env python3
"""Prioritize missing *editorial* work, not automatically claim artistic importance.

Run offline after a stable MusicBrainz snapshot:
  python tools/audit_jazz_editorial_gaps.py --output out/editorial-next/triage.json

Ranking is a reproducible research queue. Its score represents the
combination of weak seed prose and available typed evidence; it is *not* a
measure of musical greatness and does not select famous recordings.
"""
import argparse
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
DISPLAY_CLAUSES = ("Recorded on: ", "Member of ", "Co-recorded with ", "Played at ")


def read_tsv(name, columns):
    path = DATA / name
    rows = [line.split("\t") for line in path.read_text("ascii").splitlines()]
    if any(len(row) != columns for row in rows):
        raise ValueError(f"{name}: malformed TSV")
    if len({row[0] for row in rows}) != len(rows):
        raise ValueError(f"{name}: repeated key")
    return {row[0]: row for row in rows}


def weak_seed(bio):
    """Very conservative signal, not a linguistic quality verdict."""
    return (len(bio) <= 45 or
            bool(re.fullmatch(
                r"(?:[A-Za-z-]+ )?(?:American|British|French|German|Italian|"
                r"Japanese|Canadian|Australian|Swiss|Danish|Dutch|Norwegian|"
                r"Belgian|Austrian|South African) (?:jazz )?(?:musician|"
                r"pianist|saxophonist|guitarist|bassist|trumpeter|drummer|"
                r"singer|composer)(?: \(\d{4}-\d{4}\))?",
                bio)))


def audit():
    seed = read_tsv("jazz-encyclopedia-seed.tsv", 5)
    current = read_tsv("jazz-encyclopedia.tsv", 5)
    old_editorial = read_tsv("jazz-editorial-bios.tsv", 3)
    researched = read_tsv("jazz-researched-context.tsv", 3)
    if seed.keys() != current.keys() or old_editorial.keys() & researched.keys():
        raise ValueError("An original key is missing or original/expanded editorial overlap")
    evidence = json.loads((DATA / "jazz-music-evidence.json").read_text("utf-8"))
    reviewed = set(old_editorial) | set(researched)
    pending = []
    for key, (key0, name, original, _album, _url) in seed.items():
        if key in reviewed:
            continue
        display = current[key][2]
        mb = evidence.get(key, {})
        identity_ok = (mb.get("status") == "checked_mb" and
                       mb.get("identity_link_strength") in
                       ("reciprocal", "wikidata_p434_only"))
        recordings = len(mb.get("recordings", [])) if identity_ok else 0
        groups = len(mb.get("ensembles", [])) if identity_ok else 0
        collaborators = len(mb.get("collaborators", [])) if identity_ok else 0
        venues = len(mb.get("performance_venues", [])) if identity_ok else 0
        bland = weak_seed(original)
        display_music = sum(marker in display for marker in DISPLAY_CLAUSES)
        # Weight *editorial research opportunity* rather than celebrity:
        # machine-derived credits make an entry findable, but a generic
        # source line and absence of a researched capsule are the gap.
        score = (8 if bland else 0) + (2 if len(original) < 70 else 0)
        score += (3 if identity_ok and recordings else 0)
        score += (2 if identity_ok and groups else 0)
        score += (1 if identity_ok and collaborators else 0)
        score += (1 if identity_ok and venues else 0)
        score += (2 if not display_music else 0)
        pending.append({
            "key": key, "name": name,
            "seed_description": original,
            "current_machine_description": display,
            "original_generic_or_short": bland,
            "source_identity_checked": identity_ok,
            "typed_recordings_sampled": recordings,
            "typed_ensemble_memberships_sampled": groups,
            "typed_coperformers_sampled": collaborators,
            "typed_venues_sampled": venues,
            "music_clause_types_on_display": display_music,
            "research_opportunity_score": score,
        })
    pending.sort(key=lambda r: (-r["research_opportunity_score"],
                                -r["typed_coperformers_sampled"], r["key"]))
    counts = Counter()
    for r in pending:
        counts["generic_or_short_seed"] += r["original_generic_or_short"]
        counts["machine_recording_samples"] += bool(r["typed_recordings_sampled"])
        counts["visible_machine_music_clauses"] += bool(r["music_clause_types_on_display"])
        counts["without_direct_music_on_display"] += not bool(r["music_clause_types_on_display"])
        counts["source_identity_checked"] += r["source_identity_checked"]
    return {
        "basis": "Frozen jazz-encyclopedia-seed.tsv and existing typed MusicBrainz evidence",
        "warning": "This is a triage score for source research, not proof of fame, jazz significance, authenticity or a recommended song",
        "catalog_count": len(seed),
        "protected_original_editorials": len(old_editorial),
        "source_researched_individual_editorials": len(researched),
        "unreviewed_count": len(pending),
        "unreviewed_flags": dict(counts),
        "priority_queue": pending,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path, help="Write a reproducible JSON research queue")
    args = ap.parse_args()
    result = audit()
    print("EDITORIAL_GAPS", result["unreviewed_count"],
          "low_information_originals", result["unreviewed_flags"]["generic_or_short_seed"])
    print("SOURCE_CHECKED", result["unreviewed_flags"]["source_identity_checked"],
          "MUSIC_CLAUSES_ON_DISPLAY",
          result["unreviewed_flags"]["visible_machine_music_clauses"])
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(
            (json.dumps(result, indent=2, ensure_ascii=True, sort_keys=True) + "\n").encode("ascii"))
        print("TRIAGE_SAVED", args.output)


if __name__ == "__main__":
    main()
