# Pocket Jazz Radio v0.8.0 — Cardputer ADV software preview

The first public software preview to ship the expanded offline **Jazz Journey** learning experience and full Jazz Notes corpus.

## Includes

- One **app-only ESP32-S3 BIN** for M5Stack Cardputer ADV / M5Launcher 3, not a bootloader, partition image or merged flash dump.
- Optional SD ZIP containing the same BIN plus five curated playlists (51 public streams), an **empty example** `04-my-stations.m3u` that must **not** overwrite your own file, the offline encyclopedia TSV **and IDX**, evidence-backed Jazz Spark, Jazz Journey and listening exercises, and legacy artist-note TSV/IDX.
- 1,498 offline musician entries. **63 original + 532 separately source-checked additional biographies**; 903 still carry the earlier fallback/auto-enriched data. **595** exact editorial Now Playing/Explained captions, 445 individual source dossiers, 897 source claims; 150 unchanged Jazz Spark facts and 36 recommended albums; 24 Journey topics and 30 listening exercises.
- Wi-Fi profiles, presets, optional offline MP3, sleep/alarm, backup, audio diagnostics and personal likes retained.

## Implemented features in this preview

- **Internet radio:** 51 curated streams in five collections, station browser/search, nine presets, user M3U playlists, volume/mute, VU display, ICY artist/title, track history, manual stream catalog update, and reconnect / alternate URLs where configured.
- **Jazz discovery:** 1,498 offline musician entries, 595 exact artist Now Playing / Explained hints, 150 Jazz Spark facts and 25 attributed quotations, 36 album recommendations, Mood Tuner and Tap Tempo.
- **Learning and personal library:** Jazz Journey (24 lessons, four trails), Learn to Listen (30 exercises) with quizzes, Jazz Diary and Passport, saved track likes, QR codes for stations and **Google/Spotify search URLs** (not Spotify remote control), and optional local playback of up to 24 MP3 files from microSD.
- **Clock and alarm:** NTP/timezone-aware Blue Note clock, 30/45/60-minute sleep timer, daily station alarm with gradual volume fade-in (the device must stay powered on), and display dimming.
- **Configuration and support:** two saved Wi-Fi profiles, selective settings backup/restore, 18 help pages, eight diagnostics pages and on-demand incident TSV exports. Optional vinyl crackle and radio-tuning ambience are off by default.

**Development simulator:** The project used the [Velxio M5 Cardputer ADV display example](https://velxio.dev/example/cardputer-adv-m5-display) for earlier UI work; the screenshots are explicitly from v0.7z13, not from this v0.8.0 release.

## Install safely

1. Download `PocketJazzRadio-v0.8.0-app-only.bin` from [this release](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.8.0).
2. **Back up your existing SD files and keep your working BIN**. Copy only the new BIN into a suitable Cardputer SD location.
3. Choose it through **M5Launcher 3**. Do not replace M5Launcher, repartition, erase NVS or use a merged emulator image. Confirm hardware slot capacity before proceeding if M5Launcher prompts for changes.
4. For optional offline lessons, copy the named `PocketJazz/` data files out of the ZIP individually. Never overwrite your private `04-my-stations.m3u`.

## Verified scope and limits

The v0.8.0 release pipeline compiles the exact ESP32-S3 application and three additional compile-only hardware-code probes, executes host Python and C++/CTest suites, hashes archived source and assets, and validates ZIP CRC. Individual counts and SHA-256 are frozen in `SHA256-MANIFEST.txt` and `TEST-REPORT.txt` / `HOST-CTEST-REPORT.txt` in the complete local release provenance and are also summarized in this GitHub release description.

**Preview limitations:** Earlier BUFFERING and unresponsive-controls cases remain tracked in GitHub issues #1 and #2; this release is not advertised as a fix for them. The older screenshots were captured in the [Velxio Cardputer ADV emulator](https://velxio.dev/example/cardputer-adv-m5-display) with v0.7z13 firmware. Downloading this app-only release does not automatically flash a device, change the launcher/partitions or overwrite personal SD files. See [current status](PROJECT-STATUS.md).

## Research transparency

The 903 unreviewed biographies are not missing from the offline index. A reproducible local triage classified all of them: 748 potential source leads, 103 insufficient evidence and 52 unresolved identities. **These are research leads, not 903 proven historical biographies.** The source-derived additions include deliberate clarifications on original label versus reissue, recorded versus released dates, composition versus performance, and exact player personnel.

[Three-step guide](QUICK-START.md) · [Features](FEATURE-TOUR.md) · [Simulator used during development](https://velxio.dev/example/cardputer-adv-m5-display)
