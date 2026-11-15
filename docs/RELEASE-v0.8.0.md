# Pocket Jazz Radio v0.8.0 — Cardputer ADV software preview

The first public software preview to ship the expanded offline **Jazz Journey** learning experience and full Jazz Notes corpus. **It is not a declaration of field-tested playback stability.**

## Includes

- One **app-only ESP32-S3 BIN** for M5Stack Cardputer ADV / M5Launcher 3, not a bootloader, partition image or merged flash dump.
- Optional SD ZIP containing the same BIN plus five curated playlists (51 public streams), an **empty example** `04-my-stations.m3u` that must **not** overwrite your own file, the offline encyclopedia TSV **and IDX**, evidence-backed Jazz Spark, Jazz Journey and listening exercises, and legacy artist-note TSV/IDX.
- 1,498 offline musician entries. **63 original + 532 separately source-checked additional biographies**; 903 still carry the earlier fallback/auto-enriched data. **595** exact editorial Now Playing/Explained captions, 445 individual source dossiers, 897 source claims; 150 unchanged Jazz Spark facts and 36 recommended albums; 24 Journey topics and 30 listening exercises.
- Wi-Fi profiles, presets, optional offline MP3, sleep/alarm, backup, audio diagnostics and personal likes retained.

## Install safely

1. Download `PocketJazzRadio-v0.8.0-app-only.bin` from [this release](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.8.0).
2. **Back up your existing SD files and keep your working BIN**. Copy only the new BIN into a suitable Cardputer SD location.
3. Choose it through **M5Launcher 3**. Do not replace M5Launcher, repartition, erase NVS or use a merged emulator image. Confirm hardware slot capacity before proceeding if M5Launcher prompts for changes.
4. For optional offline lessons, copy the named `PocketJazz/` data files out of the ZIP individually. Never overwrite your private `04-my-stations.m3u`.

## Verified scope and limits

The v0.8.0 release pipeline compiles the exact ESP32-S3 application and three additional compile-only hardware-code probes, executes host Python and C++/CTest suites, hashes archived source and assets, and validates ZIP CRC. Individual counts and SHA-256 are frozen in `SHA256-MANIFEST.txt` and `TEST-REPORT.txt` / `HOST-CTEST-REPORT.txt` in the complete local release provenance and are also summarized in this GitHub release description.

**Not verified on a new physical Cardputer ADV:** reliable long-duration HTTPS audio on Swiss Jazz/TSF/other streams, real speaker and keyboard behavior, microSD latency, partition capacity and all Wi-Fi paths. Playback BUFFERING can still occur; an emulator snapshot from v0.7z13 does not demonstrate v0.8.0 hardware behavior. This preview does not flash the user's device, touch their personal SD or guarantee any external stream's uptime.

## Research transparency

The 903 unreviewed biographies are not missing from the offline index. A reproducible local triage classified all of them: 748 potential source leads, 103 insufficient evidence and 52 unresolved identities. **These are research leads, not 903 proven historical biographies.** The source-derived additions include deliberate clarifications on original label versus reissue, recorded versus released dates, composition versus performance, and exact player personnel.

[Three-step guide](QUICK-START.md) · [Features](FEATURE-TOUR.md) · [Known hardware limitations](HARDWARE-ACCEPTANCE.md)
