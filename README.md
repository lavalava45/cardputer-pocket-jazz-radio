# Pocket Jazz Radio for Cardputer ADV

**Internet jazz radio for the M5Stack Cardputer ADV (ESP32-S3).**

[![Firmware v0.7z9](https://img.shields.io/badge/firmware-v0.7z9-B79A68)](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z9)
![Jazz stations](https://img.shields.io/badge/stations-56-3A9188)
![PC checks](https://img.shields.io/badge/streams-62%2F62%20checked-3A9188)
[![CI](https://github.com/lavalava45/cardputer-pocket-jazz-radio/actions/workflows/validate.yml/badge.svg)](https://github.com/lavalava45/cardputer-pocket-jazz-radio/actions/workflows/validate.yml)

**One project, one GitHub repository:** application source, firmware
releases, curated station lists, verification tests and roadmap.

Pocket Jazz plays direct MP3/ICY internet-radio streams and includes
metadata, keyboard controls, presets, a VU meter, diagnostics, clock,
offline MP3 playback and several jazz-oriented utilities.

## Firmware

**Latest software-verified archived version: [v0.7z9](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z9).**

Install only the **app-only BIN** via an existing **M5Launcher 3** on
Cardputer ADV. Do not flash a merged/full-chip image, replace the
bootloader, repartition the flash or wipe NVS. Back up your SD card
before copying any bundled files.

The archived firmware source lives in [firmware/](firmware/).
It is a frozen snapshot of v0.7z9, **not** an experimental worktree.
Firmware build, Python and C++ tests passed, but physical audio stability
must be tested separately. The NTP timezone fix was later reported as
working on hardware; newer full-width UI changes are **not** in v0.7z9.

## Verified station catalog

Five editorial M3U playlists live in [catalog/](catalog/).
There are **56 stations** and **six alternate URLs**. The dated
[desktop report](checks/pc-stream-check-20261113.json) recorded
62 successful HTTPS/ICY/MP3 checks on November 13, 2026.
This is **not** a promise of continuous playback on the Cardputer
or of permanent broadcaster availability.

The versioned [manifest](manifest.json) includes exact playlist sizes,
SHA-256 digests and a report checksum. It can be checked offline:

    python scripts/catalog_manifest.py --check
    python -m unittest discover -s tests -v

In-device GitHub catalog updating is **planned but not yet implemented**.
For now users can manually copy the five curated playlists to the SD
card. **Personal stations are never published or overwritten.**
See the [catalog guide](docs/CATALOG.md).

## Repository structure

| Location | Content |
| --- | --- |
| [firmware/](firmware/) | Archived v0.7z9 source, tests, build scripts and docs |
| [GitHub Releases](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases) | App-only BIN, optional SD archive and checksums |
| [catalog/](catalog/) | Five default playlists; no personal stations |
| [manifest.json](manifest.json) | Catalog version, sizes and SHA-256 hashes |
| [checks/](checks/) | Dated PC stream-verification report |
| [scripts/](scripts/) and [tests/](tests/) | Catalog tooling and offline checks |
| [ROADMAP.md](ROADMAP.md) | Firmware, memory and catalog priorities |

## Licensing and contributions

Original application code in **firmware/** is **GPL-3.0-or-later**.
Third-party modules retain their own licenses (see
[copyright notices](firmware/COPYRIGHT-NOTICES.md)).
Original catalog scripts and metadata have a separate
[MIT license](catalog/LICENSE-MIT); this never licenses third-party
broadcasters' audio or logos.

See [CONTRIBUTING.md](CONTRIBUTING.md) for station suggestions and fixes.
Pocket Jazz is independent of M5Stack and radio broadcasters.
