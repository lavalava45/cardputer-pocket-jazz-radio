#!/usr/bin/env python3
"""Offline, read-only evidence/TSV contract for newly researched jazz notes.

Original 63 editorial mini-biographies are protected separately by the
encyclopedia seed. The previously reviewed 87 capsules are pinned here
by whole-line hashes, including their source URLs. The 2026-11 editorial
wave consists of separately archived, individually attributed dossiers.

Do not treat an artist's MusicBrainz album credit as an actual performance,
or a factual research claim as proof of a musician's cultural importance.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
DOSSIERS = "jazz-editorial-dossiers.json"
BASELINE = "jazz-researched-baseline-20261115.sha256.tsv"
WAVE1_BIOS = "jazz-researched-baseline-20261115-wave1.sha256.tsv"
WAVE1_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave1.sha256.tsv"
SPARK_BASELINE = "jazz-editorial-spark-baseline-20261115.sha256.tsv"
WAVE2_BIOS = "jazz-researched-baseline-20261115-wave2.sha256.tsv"
WAVE2_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave2.sha256.tsv"
WAVE2_SPARK = "jazz-editorial-spark-baseline-20261115-wave2.sha256.tsv"
WAVE3_BIOS = "jazz-researched-baseline-20261115-wave3.sha256.tsv"
WAVE3_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave3.sha256.tsv"
WAVE3_SPARK = "jazz-editorial-spark-baseline-20261115-wave3.sha256.tsv"
WAVE3_FINAL_BIOS = "jazz-researched-baseline-20261115-wave3-final.sha256.tsv"
WAVE3_FINAL_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave3-final.sha256.tsv"
WAVE3_FINAL_SPARK = "jazz-editorial-spark-baseline-20261115-wave3-final.sha256.tsv"
WAVE4_BIOS = "jazz-researched-baseline-20261115-wave4.sha256.tsv"
WAVE4_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave4.sha256.tsv"
WAVE4_SPARK = "jazz-editorial-spark-baseline-20261115-wave4.sha256.tsv"
WAVE5_BIOS = "jazz-researched-baseline-20261115-wave5.sha256.tsv"
WAVE5_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave5.sha256.tsv"
WAVE5_SPARK = "jazz-editorial-spark-baseline-20261115-wave5.sha256.tsv"
WAVE6_BIOS = "jazz-researched-baseline-20261115-wave6.sha256.tsv"
WAVE6_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave6.sha256.tsv"
WAVE6_SPARK = "jazz-editorial-spark-baseline-20261115-wave6.sha256.tsv"
WAVE7_BIOS = "jazz-researched-baseline-20261115-wave7.sha256.tsv"
WAVE7_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave7.sha256.tsv"
WAVE7_SPARK = "jazz-editorial-spark-baseline-20261115-wave7.sha256.tsv"
WAVE8_BIOS = "jazz-researched-baseline-20261115-wave8.sha256.tsv"
WAVE8_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave8.sha256.tsv"
WAVE8_SPARK = "jazz-editorial-spark-baseline-20261115-wave8.sha256.tsv"
WAVE9_BIOS = "jazz-researched-baseline-20261115-wave9.sha256.tsv"
WAVE9_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave9.sha256.tsv"
WAVE9_SPARK = "jazz-editorial-spark-baseline-20261115-wave9.sha256.tsv"
WAVE10_BIOS = "jazz-researched-baseline-20261115-wave10.sha256.tsv"
WAVE10_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave10.sha256.tsv"
WAVE10_SPARK = "jazz-editorial-spark-baseline-20261115-wave10.sha256.tsv"
WAVE11_BIOS = "jazz-researched-baseline-20261115-wave11.sha256.tsv"
WAVE11_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave11.sha256.tsv"
WAVE11_SPARK = "jazz-editorial-spark-baseline-20261115-wave11.sha256.tsv"
WAVE11_AUDITED_BIOS = "jazz-researched-baseline-20261115-wave11-audited.sha256.tsv"
WAVE11_AUDITED_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave11-audited.sha256.tsv"
WAVE11_AUDITED_SPARK = "jazz-editorial-spark-baseline-20261115-wave11-audited.sha256.tsv"
WAVE11_SOURCE_QA_BIOS = "jazz-researched-baseline-20261115-wave11-source-qa.sha256.tsv"
WAVE11_SOURCE_QA_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave11-source-qa.sha256.tsv"
WAVE11_SOURCE_QA_SPARK = "jazz-editorial-spark-baseline-20261115-wave11-source-qa.sha256.tsv"
WAVE12_BIOS = "jazz-researched-baseline-20261115-wave12-research01.sha256.tsv"
WAVE12_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave12-research01.sha256.tsv"
WAVE12_SPARK = "jazz-editorial-spark-baseline-20261115-wave12-research01.sha256.tsv"
WAVE12_RESEARCH02_BIOS = "jazz-researched-baseline-20261115-wave12-research02.sha256.tsv"
WAVE12_RESEARCH02_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave12-research02.sha256.tsv"
WAVE12_RESEARCH02_SPARK = "jazz-editorial-spark-baseline-20261115-wave12-research02.sha256.tsv"
WAVE13_BIOS = "jazz-researched-baseline-20261115-wave13.sha256.tsv"
WAVE13_DOSSIERS = "jazz-editorial-dossiers-baseline-20261115-wave13.sha256.tsv"
WAVE13_SPARK = "jazz-editorial-spark-baseline-20261115-wave13.sha256.tsv"
# The first wave11 frozen checkpoint is preserved intact. These exact keys
# have individually documented subsequent corrections; all other wave11
# biographies/dossiers remain byte-for-byte protected by their original pins.
WAVE11_AUDITED_REVISIONS = frozenset((
    "ann-burton", "carlos-lyra", "joris-teepe", "guem", "sebi-tramontana",
    "guy-warren", "cameron-brown", "judi-silvano", "sahib-shihab"))
WAVE11_AUDITED_NEW = frozenset(("nathan-davis",))
WAVE11_SOURCE_QA_REVISIONS = frozenset(("stan-tracey", "carlo-krahmer"))
WAVE11_SOURCE_QA_NEW = frozenset(("fred-guy", "george-tucker", "othello-molineaux"))
WAVE12_REVISIONS = frozenset(("nathan-davis", "fred-guy"))
WAVE12_NEW = frozenset(("al-lucas", "castor-mccord", "chris-white", "carl-morten-iversen",
                       "peter-beets", "bing-crosby", "china-forbes", "anthony-wilson"))
WAVE12_RESEARCH02_NEW = frozenset(("chris-pyne", "bruce-ditmas", "bert-boeren"))
WAVE13_NEW = frozenset((
    "bruce-barth", "bruce-forman", "barney-mcall", "chad-wackerman",
    "axel-zwingenberger", "candy-dulfer", "dave-stryker", "jacky-terrasson",
    "holly-cole", "champian-fulton", "greg-abate"))
KEY = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")
EVIDENCE_ID = re.compile(r"ev[0-9]{2,3}\Z")


def _unique_json_pairs(pairs):
    """Do not let a duplicate dossier key silently overwrite its predecessor."""
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError(f"Duplicate JSON evidence/dossier property: {key}")
        obj[key] = value
    return obj


def read_dossiers(data: Path) -> dict:
    return json.loads((data / DOSSIERS).read_text("ascii"),
                      object_pairs_hook=_unique_json_pairs)


def read_pinned_rows(data: Path, filename: str) -> dict[str, str]:
    """A manifest pins whole *source* lines or canonical JSON objects."""
    result = {}
    for line in (data / filename).read_text("ascii").splitlines():
        fields = line.split("\t")
        if (len(fields) != 2 or not KEY.fullmatch(fields[0]) or
                fields[0] in result or not re.fullmatch(r"[a-f0-9]{64}", fields[1])):
            raise ValueError(f"Malformed editorial pin manifest: {filename}")
        result[fields[0]] = fields[1]
    return result


def load_verified_editorial_spark(data: Path, dossiers: dict) -> list[tuple[str, str, str]]:
    """Validate 5-column key/id/exact-claim/topic/source against evidence IDs.

    A URL by itself is NOT proof of the asserted sentence. The approved Spark
    text must be identical to the claim owned by the particular evidence ID,
    and the source must be that claim's URL (not another URL in the dossier).
    Exact comparison happens BEFORE any text normalization/truncation.
    """
    path = data / "jazz-editorial-spark.tsv"
    if not path.exists():
        raise ValueError("Editorial Spark source file missing")
    journey = (data / "jazz-journey.tsv").read_text("ascii").splitlines()
    journey_topics = {row.split("\t")[-2] for row in journey}
    if not journey_topics:
        raise ValueError("Editorial Spark: no Jazz Journey topics")
    output = []
    seen_keys, seen_text = set(), set()
    for number, line in enumerate(path.read_text("ascii").splitlines(), 1):
        cells = line.split("\t")
        if len(cells) != 5:
            raise ValueError(f"Editorial Spark row {number}: expected 5 columns")
        key, evidence_id, text, topic, source = cells
        if key not in dossiers or not EVIDENCE_ID.fullmatch(evidence_id):
            raise ValueError(f"Editorial Spark row {number}: unknown artist/evidence ID")
        if key in seen_keys or text.casefold() in seen_text:
            raise ValueError(f"Editorial Spark row {number}: duplicate artist or fact")
        seen_keys.add(key)
        seen_text.add(text.casefold())
        if (not text.isascii() or any(ord(c) < 32 for c in text) or
                not 10 <= len(text) <= 190 or topic not in journey_topics or
                not topic.isascii() or len(topic) > 96):
            raise ValueError(f"Editorial Spark row {number}: invalid text/Journey topic")
        matches = [e for e in dossiers[key]["evidence"] if e["id"] == evidence_id]
        if len(matches) != 1:
            raise ValueError(f"Editorial Spark row {number}: evidence ID not found")
        claim = matches[0]
        if text != claim["claim"]:
            raise ValueError(f"Editorial Spark row {number}: claim text mismatch")
        if source != claim["source_url"]:
            raise ValueError(f"Editorial Spark row {number}: evidence source mismatch")
        output.append((text, topic, source))
    return output


def audit(data: Path = DATA) -> dict:
    original = (data / "jazz-editorial-bios.tsv").read_bytes()
    protected = {line.split(b"\t")[0].decode("ascii") for line in original.splitlines()}
    seed = {line.split(b"\t")[0].decode("ascii") for line in
            (data / "jazz-encyclopedia-seed.tsv").read_bytes().splitlines()}
    rows = {}
    for number, raw in enumerate((data / "jazz-researched-context.tsv").read_bytes().splitlines(), 1):
        cells = raw.decode("ascii").split("\t")
        if len(cells) != 3:
            raise ValueError(f"researched TSV row {number}: needs exactly 3 fields")
        key, biography, url = cells
        if (not KEY.fullmatch(key) or key not in seed or key in rows or
                key in protected or not 80 <= len(biography) <= 180 or
                urlsplit(url).scheme != "https" or len(url) > 224):
            raise ValueError(f"researched TSV row {number}: invalid or protected")
        rows[key] = (biography, url, raw)
    if list(rows) != sorted(rows):
        raise ValueError("researched TSV must be sorted for deterministic regeneration")
    baseline = read_pinned_rows(data, BASELINE)
    if len(baseline) != 87:
        raise ValueError("Baseline must preserve all 87 earlier researched biographies")
    for key, digest in baseline.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: original researched biography or provenance changed")
    wave1 = read_pinned_rows(data, WAVE1_BIOS)
    if len(wave1) != 161 or not set(baseline) <= set(wave1):
        raise ValueError("Pinned 161-bio source wave is malformed")
    for key, digest in wave1.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed biography or provenance changed")
    wave2 = read_pinned_rows(data, WAVE2_BIOS)
    if len(wave2) != 199 or not set(wave1) <= set(wave2):
        raise ValueError("Pinned 199-bio source wave is malformed")
    for key, digest in wave2.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 199-wave biography changed")
    wave3 = read_pinned_rows(data, WAVE3_BIOS)
    if len(wave3) != 213 or not set(wave2) <= set(wave3):
        raise ValueError("Pinned 213-bio wave3 source set is malformed")
    for key, digest in wave3.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 213-wave biography changed")
    wave3_final = read_pinned_rows(data, WAVE3_FINAL_BIOS)
    if len(wave3_final) != 225 or not set(wave3) <= set(wave3_final):
        raise ValueError("Pinned 225-bio final wave3 source set is malformed")
    for key, digest in wave3_final.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed final 225-wave biography changed")
    wave4 = read_pinned_rows(data, WAVE4_BIOS)
    if len(wave4) != 256 or not set(wave3_final) <= set(wave4):
        raise ValueError("Pinned 256-bio wave4 source set is malformed")
    for key, digest in wave4.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 256-wave biography changed")
    wave5 = read_pinned_rows(data, WAVE5_BIOS)
    if len(wave5) != 281 or not set(wave4) <= set(wave5):
        raise ValueError("Pinned 281-bio wave5 source set is malformed")
    for key, digest in wave5.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 281-wave biography changed")
    wave6 = read_pinned_rows(data, WAVE6_BIOS)
    if len(wave6) != 313 or not set(wave5) <= set(wave6):
        raise ValueError("Pinned 313-bio wave6 source set is malformed")
    for key, digest in wave6.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 313-wave biography changed")
    wave7 = read_pinned_rows(data, WAVE7_BIOS)
    if len(wave7) != 352 or not set(wave6) <= set(wave7):
        raise ValueError("Pinned 352-bio wave7 source set is malformed")
    for key, digest in wave7.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 352-wave biography changed")
    wave8 = read_pinned_rows(data, WAVE8_BIOS)
    if len(wave8) != 392 or not set(wave7) <= set(wave8):
        raise ValueError("Pinned 392-bio wave8 source set is malformed")
    for key, digest in wave8.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 392-wave biography changed")
    wave9 = read_pinned_rows(data, WAVE9_BIOS)
    if len(wave9) != 433 or not set(wave8) <= set(wave9):
        raise ValueError("Pinned 433-bio wave9 source set is malformed")
    for key, digest in wave9.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 433-wave biography changed")
    wave10 = read_pinned_rows(data, WAVE10_BIOS)
    if len(wave10) != 468 or not set(wave9) <= set(wave10):
        raise ValueError("Pinned 468-bio wave10 source set is malformed")
    for key, digest in wave10.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: previously reviewed 468-wave biography changed")
    wave11 = read_pinned_rows(data, WAVE11_BIOS)
    if len(wave11) != 506 or not set(wave10) <= set(wave11):
        raise ValueError("Pinned 506-bio wave11 source set is malformed")
    for key, digest in wave11.items():
        if key not in rows or (key not in WAVE11_AUDITED_REVISIONS | WAVE11_SOURCE_QA_REVISIONS | WAVE12_REVISIONS and
                hashlib.sha256(rows[key][2]).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 506-wave biography changed")
    audited_bios = read_pinned_rows(data, WAVE11_AUDITED_BIOS)
    if len(audited_bios) != 507 or set(audited_bios) != set(wave11) | WAVE11_AUDITED_NEW:
        raise ValueError("Pinned 507-bio audited wave11 source set malformed")
    for key, digest in audited_bios.items():
        if key not in rows or (key not in WAVE11_SOURCE_QA_REVISIONS | WAVE12_REVISIONS and
                hashlib.sha256(rows[key][2]).hexdigest() != digest):
            raise ValueError(f"{key}: audited wave11 biography changed")
    source_qa_bios = read_pinned_rows(data, WAVE11_SOURCE_QA_BIOS)
    if len(source_qa_bios) != 510 or set(source_qa_bios) != set(audited_bios) | WAVE11_SOURCE_QA_NEW:
        raise ValueError("Pinned 510 source-QA wave11 biographies incomplete")
    for key, digest in source_qa_bios.items():
        if key not in rows or (key not in WAVE12_REVISIONS and
                hashlib.sha256(rows[key][2]).hexdigest() != digest):
            raise ValueError(f"{key}: source-QA wave11 biography changed")
    wave12_bios = read_pinned_rows(data, WAVE12_BIOS)
    if len(wave12_bios) != 518 or set(wave12_bios) != set(source_qa_bios) | WAVE12_NEW:
        raise ValueError("Pinned 518-bio wave12 source set incomplete")
    for key, digest in wave12_bios.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: wave12 researched biography changed")
    wave12_research02_bios = read_pinned_rows(data, WAVE12_RESEARCH02_BIOS)
    if (len(wave12_research02_bios) != 521 or
            set(wave12_research02_bios) != set(wave12_bios) | WAVE12_RESEARCH02_NEW):
        raise ValueError("Pinned 521-research02 biographies incomplete")
    for key, digest in wave12_research02_bios.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: wave12 research02 biography changed")
    wave13_bios = read_pinned_rows(data, WAVE13_BIOS)
    if len(wave13_bios) != 532 or set(wave13_bios) != set(wave12_research02_bios) | WAVE13_NEW:
        raise ValueError("Pinned 532 wave13 researched biographies incomplete")
    for key, digest in wave13_bios.items():
        if key not in rows or hashlib.sha256(rows[key][2]).hexdigest() != digest:
            raise ValueError(f"{key}: wave13 researched biography changed")

    dossiers = read_dossiers(data)
    if (not isinstance(dossiers, dict) or not dossiers or
            set(dossiers) != set(rows) - set(baseline)):
        raise ValueError("Individual dossier keys do not match new researched TSV keys")
    previous_dossiers = read_pinned_rows(data, WAVE1_DOSSIERS)
    if len(previous_dossiers) != 74 or set(previous_dossiers) != set(wave1) - set(baseline):
        raise ValueError("Pinned 74 editorial dossier records are incomplete")
    for key, digest in previous_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed editorial dossier changed")
    wave2_dossiers = read_pinned_rows(data, WAVE2_DOSSIERS)
    if len(wave2_dossiers) != 112 or not set(previous_dossiers) <= set(wave2_dossiers):
        raise ValueError("Pinned 112 editorial dossiers are incomplete")
    for key, digest in wave2_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 112-wave dossier changed")
    wave3_dossiers = read_pinned_rows(data, WAVE3_DOSSIERS)
    if len(wave3_dossiers) != 126 or not set(wave2_dossiers) <= set(wave3_dossiers):
        raise ValueError("Pinned 126 wave3 dossier records are incomplete")
    for key, digest in wave3_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 126-wave dossier changed")
    wave3_final_dossiers = read_pinned_rows(data, WAVE3_FINAL_DOSSIERS)
    if len(wave3_final_dossiers) != 138 or not set(wave3_dossiers) <= set(wave3_final_dossiers):
        raise ValueError("Pinned 138 final wave3 dossier records are incomplete")
    for key, digest in wave3_final_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed final 138-wave dossier changed")
    wave4_dossiers = read_pinned_rows(data, WAVE4_DOSSIERS)
    if len(wave4_dossiers) != 169 or not set(wave3_final_dossiers) <= set(wave4_dossiers):
        raise ValueError("Pinned 169 wave4 dossier records are incomplete")
    for key, digest in wave4_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 169-wave dossier changed")
    wave5_dossiers = read_pinned_rows(data, WAVE5_DOSSIERS)
    if len(wave5_dossiers) != 194 or not set(wave4_dossiers) <= set(wave5_dossiers):
        raise ValueError("Pinned 194 wave5 dossier records are incomplete")
    for key, digest in wave5_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 194-wave dossier changed")
    wave6_dossiers = read_pinned_rows(data, WAVE6_DOSSIERS)
    if len(wave6_dossiers) != 226 or not set(wave5_dossiers) <= set(wave6_dossiers):
        raise ValueError("Pinned 226 wave6 dossier records are incomplete")
    for key, digest in wave6_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 226-wave dossier changed")
    wave7_dossiers = read_pinned_rows(data, WAVE7_DOSSIERS)
    if len(wave7_dossiers) != 265 or not set(wave6_dossiers) <= set(wave7_dossiers):
        raise ValueError("Pinned 265 wave7 dossier records are incomplete")
    for key, digest in wave7_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 265-wave dossier changed")
    wave8_dossiers = read_pinned_rows(data, WAVE8_DOSSIERS)
    if len(wave8_dossiers) != 305 or not set(wave7_dossiers) <= set(wave8_dossiers):
        raise ValueError("Pinned 305 wave8 dossier records are incomplete")
    for key, digest in wave8_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 305-wave dossier changed")
    wave9_dossiers = read_pinned_rows(data, WAVE9_DOSSIERS)
    if len(wave9_dossiers) != 346 or not set(wave8_dossiers) <= set(wave9_dossiers):
        raise ValueError("Pinned 346 wave9 dossier records are incomplete")
    for key, digest in wave9_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 346-wave dossier changed")
    wave10_dossiers = read_pinned_rows(data, WAVE10_DOSSIERS)
    if len(wave10_dossiers) != 381 or not set(wave9_dossiers) <= set(wave10_dossiers):
        raise ValueError("Pinned 381 wave10 dossier records are incomplete")
    for key, digest in wave10_dossiers.items():
        if (key not in dossiers or
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: previously reviewed 381-wave dossier changed")
    wave11_dossiers = read_pinned_rows(data, WAVE11_DOSSIERS)
    if len(wave11_dossiers) != 419 or not set(wave10_dossiers) <= set(wave11_dossiers):
        raise ValueError("Pinned 419 wave11 dossiers are incomplete")
    for key, digest in wave11_dossiers.items():
        if (key not in dossiers or
                (key not in WAVE11_AUDITED_REVISIONS | WAVE11_SOURCE_QA_REVISIONS | WAVE12_REVISIONS and
                hashlib.sha256(json.dumps(dossiers[key], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=True)
                               .encode("ascii")).hexdigest() != digest)):
            raise ValueError(f"{key}: previously reviewed 419-wave dossier changed")
    audited_dossiers = read_pinned_rows(data, WAVE11_AUDITED_DOSSIERS)
    if len(audited_dossiers) != 420 or set(audited_dossiers) != set(wave11_dossiers) | WAVE11_AUDITED_NEW:
        raise ValueError("Pinned 420 audited wave11 dossiers incomplete")
    for key, digest in audited_dossiers.items():
        if (key not in dossiers or (key not in WAVE11_SOURCE_QA_REVISIONS | WAVE12_REVISIONS and
                hashlib.sha256(json.dumps(dossiers[key],
                    sort_keys=True, separators=(",", ":"), ensure_ascii=True)
                .encode("ascii")).hexdigest() != digest)):
            raise ValueError(f"{key}: audited wave11 dossier changed")
    source_qa_dossiers = read_pinned_rows(data, WAVE11_SOURCE_QA_DOSSIERS)
    if len(source_qa_dossiers) != 423 or set(source_qa_dossiers) != set(audited_dossiers) | WAVE11_SOURCE_QA_NEW:
        raise ValueError("Pinned 423 source-QA wave11 dossiers incomplete")
    for key, digest in source_qa_dossiers.items():
        if (key not in dossiers or (key not in WAVE12_REVISIONS and
                hashlib.sha256(json.dumps(dossiers[key],
                  sort_keys=True, separators=(",", ":"), ensure_ascii=True)
                .encode("ascii")).hexdigest() != digest)):
            raise ValueError(f"{key}: source-QA wave11 dossier changed")
    wave12_dossiers = read_pinned_rows(data, WAVE12_DOSSIERS)
    if len(wave12_dossiers) != 431 or set(wave12_dossiers) != set(source_qa_dossiers) | WAVE12_NEW:
        raise ValueError("Pinned 431 wave12 dossiers incomplete")
    for key, digest in wave12_dossiers.items():
        if (key not in dossiers or hashlib.sha256(json.dumps(dossiers[key],
               sort_keys=True, separators=(",", ":"), ensure_ascii=True)
            .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: wave12 dossier changed")
    wave12_research02_dossiers = read_pinned_rows(data, WAVE12_RESEARCH02_DOSSIERS)
    if (len(wave12_research02_dossiers) != 434 or
            set(wave12_research02_dossiers) != set(wave12_dossiers) | WAVE12_RESEARCH02_NEW):
        raise ValueError("Pinned 434 research02 dossiers incomplete")
    for key, digest in wave12_research02_dossiers.items():
        if (key not in dossiers or hashlib.sha256(json.dumps(dossiers[key],
               sort_keys=True, separators=(",", ":"), ensure_ascii=True)
            .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: wave12 research02 dossier changed")
    wave13_dossiers = read_pinned_rows(data, WAVE13_DOSSIERS)
    if len(wave13_dossiers) != 445 or set(wave13_dossiers) != set(wave12_research02_dossiers) | WAVE13_NEW:
        raise ValueError("Pinned 445 wave13 researched dossiers incomplete")
    for key, digest in wave13_dossiers.items():
        if (key not in dossiers or hashlib.sha256(json.dumps(dossiers[key],
                sort_keys=True, separators=(",", ":"), ensure_ascii=True)
                .encode("ascii")).hexdigest() != digest):
            raise ValueError(f"{key}: wave13 researched dossier changed")
    all_evidence = 0
    for key in sorted(dossiers):
        doc = dossiers[key]
        if (not isinstance(doc, dict) or doc.get("key") != key or
                doc.get("tier") != "individual_editorial_music_research" or
                (doc.get("summary"), doc.get("primary_source_url")) != rows[key][:2] or
                not isinstance(doc.get("why_it_matters"), str) or
                len(doc["why_it_matters"]) < 24 or
                not isinstance(doc.get("limitations"), str) or
                not isinstance(doc.get("evidence"), list) or
                len(doc["evidence"]) < 2):
            raise ValueError(f"{key}: untraceable or underspecified editorial dossier")
        sources = {doc["primary_source_url"]}
        evidence_ids = set()
        for item in doc["evidence"]:
            if (not isinstance(item, dict) or not isinstance(item.get("claim"), str) or
                    len(item["claim"]) < 20 or not isinstance(item.get("source_url"), str) or
                    urlsplit(item["source_url"]).scheme != "https" or
                    not EVIDENCE_ID.fullmatch(str(item.get("id", ""))) or
                    item["id"] in evidence_ids):
                raise ValueError(f"{key}: evidence must have a concrete claim and HTTPS source")
            evidence_ids.add(item["id"])
            sources.add(item["source_url"])
            all_evidence += 1
        if doc["primary_source_url"] not in {x["source_url"] for x in doc["evidence"]}:
            raise ValueError(f"{key}: primary source never supports a specific claim")
    spark = load_verified_editorial_spark(data, dossiers)
    original_spark = read_pinned_rows(data, SPARK_BASELINE)
    if len(original_spark) != 14:
        raise ValueError("Pinned original Jazz Spark facts are incomplete")
    spark_raw = {row.split(b"\t")[0].decode("ascii"): row for row in
                 (data / "jazz-editorial-spark.tsv").read_bytes().splitlines()}
    for key, digest in original_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved Jazz Spark fact changed")
    wave2_spark = read_pinned_rows(data, WAVE2_SPARK)
    if len(wave2_spark) != 46 or not set(original_spark) <= set(wave2_spark):
        raise ValueError("Pinned 46 Spark facts are incomplete")
    for key, digest in wave2_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 46-wave Spark fact changed")
    wave3_spark = read_pinned_rows(data, WAVE3_SPARK)
    if len(wave3_spark) != 56 or not set(wave2_spark) <= set(wave3_spark):
        raise ValueError("Pinned 56 wave3 Spark facts are incomplete")
    for key, digest in wave3_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 56-wave Spark fact changed")
    wave3_final_spark = read_pinned_rows(data, WAVE3_FINAL_SPARK)
    if len(wave3_final_spark) != 68 or not set(wave3_spark) <= set(wave3_final_spark):
        raise ValueError("Pinned 68 final wave3 Spark facts are incomplete")
    for key, digest in wave3_final_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved final 68-wave Spark fact changed")
    wave4_spark = read_pinned_rows(data, WAVE4_SPARK)
    if len(wave4_spark) != 93 or not set(wave3_final_spark) <= set(wave4_spark):
        raise ValueError("Pinned 93 wave4 Spark facts are incomplete")
    for key, digest in wave4_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 93-wave Spark fact changed")
    wave5_spark = read_pinned_rows(data, WAVE5_SPARK)
    if len(wave5_spark) != 112 or not set(wave4_spark) <= set(wave5_spark):
        raise ValueError("Pinned 112 wave5 Spark facts are incomplete")
    for key, digest in wave5_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 112-wave Spark fact changed")
    wave6_spark = read_pinned_rows(data, WAVE6_SPARK)
    if len(wave6_spark) != 120 or not set(wave5_spark) <= set(wave6_spark):
        raise ValueError("Pinned 120 wave6 Spark facts are incomplete")
    for key, digest in wave6_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 120-wave Spark fact changed")
    wave7_spark = read_pinned_rows(data, WAVE7_SPARK)
    if len(wave7_spark) != 125 or not set(wave6_spark) <= set(wave7_spark):
        raise ValueError("Pinned 125 wave7 Spark facts are incomplete")
    for key, digest in wave7_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 125-wave Spark fact changed")
    wave8_spark = read_pinned_rows(data, WAVE8_SPARK)
    if len(wave8_spark) != 133 or not set(wave7_spark) <= set(wave8_spark):
        raise ValueError("Pinned 133 wave8 Spark facts are incomplete")
    for key, digest in wave8_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 133-wave Spark fact changed")
    wave9_spark = read_pinned_rows(data, WAVE9_SPARK)
    if len(wave9_spark) != 140 or not set(wave8_spark) <= set(wave9_spark):
        raise ValueError("Pinned 140 wave9 Spark facts are incomplete")
    for key, digest in wave9_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 140-wave Spark fact changed")
    wave10_spark = read_pinned_rows(data, WAVE10_SPARK)
    if len(wave10_spark) != 147 or not set(wave9_spark) <= set(wave10_spark):
        raise ValueError("Pinned 147 wave10 Spark facts are incomplete")
    for key, digest in wave10_spark.items():
        if (key not in spark_raw or
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 147-wave Spark fact changed")
    wave11_spark = read_pinned_rows(data, WAVE11_SPARK)
    if len(wave11_spark) != 150 or not set(wave10_spark) <= set(wave11_spark):
        raise ValueError("Pinned 150 wave11 Spark facts are incomplete")
    for key, digest in wave11_spark.items():
        if key not in spark_raw or (key not in WAVE11_SOURCE_QA_REVISIONS and
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: previously approved 150-wave Spark fact changed")
    audited_spark = read_pinned_rows(data, WAVE11_AUDITED_SPARK)
    if len(audited_spark) != 150 or set(audited_spark) != set(wave11_spark):
        raise ValueError("Pinned 150 audited wave11 Spark facts incomplete")
    for key, digest in audited_spark.items():
        if key not in spark_raw or (key not in WAVE11_SOURCE_QA_REVISIONS and
                hashlib.sha256(spark_raw[key]).hexdigest() != digest):
            raise ValueError(f"{key}: audited wave11 Spark changed")
    source_qa_spark = read_pinned_rows(data, WAVE11_SOURCE_QA_SPARK)
    if len(source_qa_spark) != 150 or set(source_qa_spark) != set(audited_spark):
        raise ValueError("Pinned 150 source-QA wave11 Spark facts incomplete")
    for key, digest in source_qa_spark.items():
        if key not in spark_raw or hashlib.sha256(spark_raw[key]).hexdigest() != digest:
            raise ValueError(f"{key}: source-QA wave11 Spark changed")
    wave12_spark = read_pinned_rows(data, WAVE12_SPARK)
    if len(wave12_spark) != 150 or set(wave12_spark) != set(source_qa_spark):
        raise ValueError("Pinned 150 wave12 Spark facts incomplete")
    for key, digest in wave12_spark.items():
        if key not in spark_raw or hashlib.sha256(spark_raw[key]).hexdigest() != digest:
            raise ValueError(f"{key}: wave12 Spark changed")
    wave12_research02_spark = read_pinned_rows(data, WAVE12_RESEARCH02_SPARK)
    if len(wave12_research02_spark) != 150 or set(wave12_research02_spark) != set(wave12_spark):
        raise ValueError("Pinned 150 wave12 research02 Spark facts incomplete")
    for key, digest in wave12_research02_spark.items():
        if key not in spark_raw or hashlib.sha256(spark_raw[key]).hexdigest() != digest:
            raise ValueError(f"{key}: wave12 research02 Spark changed")
    wave13_spark = read_pinned_rows(data, WAVE13_SPARK)
    if len(wave13_spark) != 150 or set(wave13_spark) != set(wave12_research02_spark):
        raise ValueError("Pinned 150 wave13 Spark facts incomplete")
    for key, digest in wave13_spark.items():
        if key not in spark_raw or hashlib.sha256(spark_raw[key]).hexdigest() != digest:
            raise ValueError(f"{key}: wave13 Spark changed")
    return {
        "original_editorials_unchanged": len(protected),
        "earlier_researched_whole_lines_unchanged": len(baseline),
        "previous_editorial_wave_unchanged": len(wave1),
        "second_editorial_wave_unchanged": len(wave2),
        "third_editorial_wave_unchanged": len(wave3),
        "third_final_editorial_wave_unchanged": len(wave3_final),
        "fourth_editorial_wave_unchanged": len(wave4),
        "fifth_editorial_wave_unchanged": len(wave5),
        "sixth_editorial_wave_unchanged": len(wave6),
        "seventh_editorial_wave_unchanged": len(wave7),
        "eighth_editorial_wave_unchanged": len(wave8),
        "ninth_editorial_wave_unchanged": len(wave9),
        "tenth_editorial_wave_unchanged": len(wave10),
        "eleventh_editorial_wave_unchanged": len(wave11),
        "eleventh_audited_editorial_wave_unchanged": len(audited_bios),
        "eleventh_source_qa_editorial_wave_unchanged": len(source_qa_bios),
        "twelfth_editorial_wave_unchanged": len(wave12_bios),
        "twelfth_research02_editorial_wave_unchanged": len(wave12_research02_bios),
        "thirteenth_editorial_wave_unchanged": len(wave13_bios),
        "previous_individual_dossiers_unchanged": len(previous_dossiers),
        "second_individual_dossiers_unchanged": len(wave2_dossiers),
        "third_individual_dossiers_unchanged": len(wave3_dossiers),
        "third_final_individual_dossiers_unchanged": len(wave3_final_dossiers),
        "fourth_individual_dossiers_unchanged": len(wave4_dossiers),
        "fifth_individual_dossiers_unchanged": len(wave5_dossiers),
        "sixth_individual_dossiers_unchanged": len(wave6_dossiers),
        "seventh_individual_dossiers_unchanged": len(wave7_dossiers),
        "eighth_individual_dossiers_unchanged": len(wave8_dossiers),
        "ninth_individual_dossiers_unchanged": len(wave9_dossiers),
        "tenth_individual_dossiers_unchanged": len(wave10_dossiers),
        "eleventh_individual_dossiers_unchanged": len(wave11_dossiers),
        "eleventh_audited_dossiers_unchanged": len(audited_dossiers),
        "eleventh_source_qa_dossiers_unchanged": len(source_qa_dossiers),
        "twelfth_dossiers_unchanged": len(wave12_dossiers),
        "twelfth_research02_dossiers_unchanged": len(wave12_research02_dossiers),
        "thirteenth_dossiers_unchanged": len(wave13_dossiers),
        "previous_spark_facts_unchanged": len(original_spark),
        "second_spark_facts_unchanged": len(wave2_spark),
        "third_spark_facts_unchanged": len(wave3_spark),
        "third_final_spark_facts_unchanged": len(wave3_final_spark),
        "fourth_spark_facts_unchanged": len(wave4_spark),
        "fifth_spark_facts_unchanged": len(wave5_spark),
        "sixth_spark_facts_unchanged": len(wave6_spark),
        "seventh_spark_facts_unchanged": len(wave7_spark),
        "eighth_spark_facts_unchanged": len(wave8_spark),
        "ninth_spark_facts_unchanged": len(wave9_spark),
        "tenth_spark_facts_unchanged": len(wave10_spark),
        "eleventh_spark_facts_unchanged": len(wave11_spark),
        "eleventh_audited_spark_unchanged": len(audited_spark),
        "eleventh_source_qa_spark_unchanged": len(source_qa_spark),
        "twelfth_spark_unchanged": len(wave12_spark),
        "twelfth_research02_spark_unchanged": len(wave12_research02_spark),
        "thirteenth_spark_unchanged": len(wave13_spark),
        "new_individual_dossiers": len(dossiers),
        "concrete_attributed_claims": all_evidence,
        "editorial_spark_claims_bound_by_id": len(spark),
        "research_capsules_on_SD_source": len(rows),
        "other_unreviewed_catalog_entries": len(seed) - len(protected) - len(rows),
    }


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--data-dir", type=Path, default=DATA)
    args = p.parse_args()
    print(json.dumps(audit(args.data_dir), sort_keys=True))
