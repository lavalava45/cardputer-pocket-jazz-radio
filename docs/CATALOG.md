<div align="center">

# Cardputer Pocket Jazz · Station Catalog

**A carefully curated jazz radio directory for M5Stack Cardputer ADV**

[![Catalog](https://img.shields.io/badge/catalog-2026.11.14.1-D0AB72)](../manifest.json)
[![Stations](https://img.shields.io/badge/stations-51-3A9188)](../catalog/)
[![Streams checked](https://img.shields.io/badge/PC_stream_checks-55%2F55_pass-3A9188)](../checks/pc-stream-check-20261113.json)
[![Validate](https://github.com/lavalava45/cardputer-pocket-jazz-radio/actions/workflows/validate.yml/badge.svg)](https://github.com/lavalava45/cardputer-pocket-jazz-radio/actions/workflows/validate.yml)

</div>

---

## Listen first. Keep the catalog simple.

The **catalog/** directory supplies the editorial default stations for
**Pocket Jazz Radio**, an internet-radio project for the **M5Stack Cardputer
ADV (ESP32-S3)**. It is **not** firmware, a launcher, or an ESP32 flashing
utility.

Each station is a direct streaming URL in an extended M3U playlist. The
current collection contains **51 jazz and jazz-adjacent stations** across
five categories, plus **four optional backup stream URLs**.

The [November 13, 2026 PC report](../checks/pc-stream-check-20261113.json)
retains **55/55 successful HTTPS / MP3-frame checks** among unchanged URLs, originally checked November 13. Sonic Universe, Groove Salad and three 181.fm entries were removed editorially; the remaining streams were not re-tested during these changes. The original observations used the actual
Arduino-ESP32 3.3.9 HTTP headers (including `Icy-MetaData: 1`) and
certificate verification. **This is not a promise of continuous playback on
Cardputer or of permanent availability**: broadcasters can change URLs,
geoblock streams, or go offline at any time.

## Included playlists

| File | Theme | Stations |
| --- | --- | ---: |
| [01-essentials.m3u](../catalog/01-essentials.m3u) | Jazz essentials and public radio | 15 |
| [02-modern-jazz.m3u](../catalog/02-modern-jazz.m3u) | Modern, fusion and experimental | 10 |
| [03-mellow-bossa.m3u](../catalog/03-mellow-bossa.m3u) | Mellow jazz, bossa and adjacent styles | 12 |
| [05-swing-vocals.m3u](../catalog/05-swing-vocals.m3u) | Swing, voices, manouche and soundtracks | 10 |
| [06-latin-soul-jazz.m3u](../catalog/06-latin-soul-jazz.m3u) | Latin jazz and soul | 4 |
| **Total** | | **51** |

**Personal stations stay personal.** The file `04-my-stations.m3u` is
intentionally **not included**. Neither this repository nor a future updater
should overwrite, import, or publish a listener's own stations.

## How to update the catalog

The **v0.7z15 diagnostic preview** includes a **manual-only** in-app
update: stop the radio and offline player, press **D** from the radio
screen, review the prompt, then press **Enter**. The device fetches the
fixed `manifest.json` and five listed editorial files from this same
GitHub repository using certificate-verified HTTPS. SHA-256, sizes,
contents and 64-station/12-playlist capacity are checked. The former
files remain recoverable until the updated lists are **actually loaded
from SD**; a failed activation rolls back or leaves a recovery journal.
No personal files are selected for replacement.

Older preview firmware still requires updating the default files on a PC:

1. Back up your Cardputer SD card's `/PocketJazz/` directory.
2. Download the five `.m3u` files from [`catalog/`](../catalog/) and replace
   **only** their corresponding default files in `/PocketJazz/` on the SD card.
3. Keep `/PocketJazz/04-my-stations.m3u` and all other personal files as
   they are. Restart Pocket Jazz Radio or use its playlist reload command.

**Capacity note:** the current Cardputer app supports **64 stations across
12 playlists**. These defaults occupy 51 slots; personal stations may need
additional capacity. The in-app updater checks this and refuses
unsafe replacements. Do **not** flash a merged image, repartition the device,
or replace M5Launcher to use these files.

## Versioned and reproducible

The machine-readable [`manifest.json`](../manifest.json) contains the
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
alongside a file is not by itself a digital signature**. The manual
updater uses verified HTTPS with certificate/hostname checks and validates
the manifest and files before activation. It does not auto-update, disable
TLS verification or downgrade HTTPS to HTTP.
No HTTP downgrade, partial installation or silent modification of personal
playlists.

## Station policy

- Direct MP3/ICY streams; current default URLs must use HTTPS.
- No AAC, HLS, DASH or audio recordings in the curated list.
- No dedicated blues station in the defaults; users can add any genre to
  their own `04-my-stations.m3u`.
- Station and broadcaster names belong to their respective owners.
- We do not host, record, relay or redistribute any station's audio.

To suggest a fix or new stream, see [CONTRIBUTING.md](../CONTRIBUTING.md).
For the upcoming safe in-app catalog downloader, see
the development plan (maintained locally). **Downloading from GitHub inside the radio is not
available in the current Pocket Jazz firmware**; do not mistake this catalog
repository for an ESP32 update service.

**License:** original scripts and curated metadata in this repository are
MIT-licensed; stream content, station marks and third-party websites are
not covered by that license.
