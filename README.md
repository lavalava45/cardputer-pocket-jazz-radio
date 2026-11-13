<div align="center">

# Cardputer Pocket Jazz · Station Catalog

**A carefully curated jazz radio directory for M5Stack Cardputer ADV**

[![Catalog](https://img.shields.io/badge/catalog-2026.11.13.1-D0AB72)](manifest.json)
[![Stations](https://img.shields.io/badge/stations-56-3A9188)](catalog/)
[![Streams checked](https://img.shields.io/badge/PC_stream_checks-62%2F62_pass-3A9188)](checks/pc-stream-check-20261113.json)
[![Validate](https://github.com/lavalava45/cardputer-pocket-jazz-stations/actions/workflows/validate.yml/badge.svg)](https://github.com/lavalava45/cardputer-pocket-jazz-stations/actions/workflows/validate.yml)

</div>

---

## Listen first. Keep the catalog simple.

This repository distributes the **editorial, default station catalog** used by
**Pocket Jazz Radio**, an internet-radio project for the **M5Stack Cardputer
ADV (ESP32-S3)**. It is **not** firmware, a launcher, or an ESP32 flashing
utility.

Each station is a direct streaming URL in an extended M3U playlist. The
current collection contains **56 jazz and jazz-adjacent stations** across
five categories, plus **six optional backup stream URLs**.

The [November 13, 2026 PC report](checks/pc-stream-check-20261113.json)
records **62/62 successful HTTPS / MP3-frame checks**, using the actual
Arduino-ESP32 3.3.9 HTTP headers (including `Icy-MetaData: 1`) and
certificate verification. **This is not a promise of continuous playback on
Cardputer or of permanent availability**: broadcasters can change URLs,
geoblock streams, or go offline at any time.

## Included playlists

| File | Theme | Stations |
| --- | --- | ---: |
| [01-essentials.m3u](catalog/01-essentials.m3u) | Jazz essentials and public radio | 15 |
| [02-modern-jazz.m3u](catalog/02-modern-jazz.m3u) | Modern, fusion and experimental | 14 |
| [03-mellow-bossa.m3u](catalog/03-mellow-bossa.m3u) | Mellow jazz, bossa and adjacent styles | 13 |
| [05-swing-vocals.m3u](catalog/05-swing-vocals.m3u) | Swing, voices, manouche and soundtracks | 10 |
| [06-latin-soul-jazz.m3u](catalog/06-latin-soul-jazz.m3u) | Latin jazz and soul | 4 |
| **Total** | | **56** |

**Personal stations stay personal.** The file `04-my-stations.m3u` is
intentionally **not included**. Neither this repository nor a future updater
should overwrite, import, or publish a listener's own stations.

## How to use the catalog today

The automatic update feature **has not yet been implemented in firmware**.
Until then, updates are manual:

1. Back up your Cardputer SD card's `/PocketJazz/` directory.
2. Download the five `.m3u` files from [`catalog/`](catalog/) and replace
   **only** their corresponding default files in `/PocketJazz/` on the SD card.
3. Keep `/PocketJazz/04-my-stations.m3u` and all other personal files as
   they are. Restart Pocket Jazz Radio or use its playlist reload command.

**Capacity note:** the current Cardputer app supports **64 stations across
12 playlists**. These defaults occupy 56 slots; personal stations may need
additional capacity. A future in-app updater must check this and refuse
unsafe replacements. Do **not** flash a merged image, repartition the device,
or replace M5Launcher to use these files.

## Versioned and reproducible

The machine-readable [`manifest.json`](manifest.json) contains the
catalog version, each playlist's exact byte length and SHA-256 hash, station
counts, and the hash of its dated verification report. It is built
deterministically from the checked-in files:

```bash
python scripts/catalog_manifest.py --check
python -m unittest discover -s tests -v
```

To publish a **new version** after editing stations, the maintainer should
first run a fresh online probe, review failures, update the report and
catalog version, regenerate the manifest, then commit the results:

```bash
python -m pip install requests
python scripts/verify_streams.py --report checks/pc-stream-check-YYYYMMDD.json
python scripts/catalog_manifest.py --write
```

Change the `REPORT` and `CATALOG_VERSION` constants in
`scripts/catalog_manifest.py` for each release. GitHub Actions checks
the committed manifest **offline**, without contacting radio stations.
The online check is a separate, explicit editorial step.

**Security:** SHA-256 detects unexpected file changes; **a hash published
alongside a file is not by itself a digital signature**. A future
automatic updater must download over HTTPS with certificate/hostname
verification and validate the manifest and files before activating them.
No HTTP downgrade, partial installation or silent modification of personal
playlists.

## Station policy

- Direct MP3/ICY streams; current default URLs must use HTTPS.
- No AAC, HLS, DASH or audio recordings in the curated list.
- No dedicated blues station in the defaults; users can add any genre to
  their own `04-my-stations.m3u`.
- Station and broadcaster names belong to their respective owners.
- We do not host, record, relay or redistribute any station's audio.

To suggest a fix or new stream, see [CONTRIBUTING.md](CONTRIBUTING.md).
For the upcoming safe in-app catalog downloader, see
[ROADMAP.md](ROADMAP.md). **Downloading from GitHub inside the radio is not
available in the current Pocket Jazz firmware**; do not mistake this catalog
repository for an ESP32 update service.

**License:** original scripts and curated metadata in this repository are
MIT-licensed; stream content, station marks and third-party websites are
not covered by that license.
