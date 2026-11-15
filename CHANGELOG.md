# Changelog

## v0.8.0 — Jazz Journey and source-attributed offline encyclopedia (2026-11-15)

- Adds 24 offline Jazz Journey lessons and 30 Learn to Listen exercises, with integrated Explained / Now Playing music notes and existing Jazz Spark.
- All 1,498 offline musician entries remain indexed. **595** have exact individually sourced Now Playing/Explained hints (63 original + 532 additional). **445** source dossiers and **897** concrete evidence claims are linked to the additional editorial set; **903** fallback/machine-enriched entries lack individual historical verification. All 903 were categorized without claiming false sources.
- Retains the 51-station catalog, 150 Jazz Spark claims, 36 album recommendations, Wi-Fi presets, local playback, alarm, diagnostics and existing file safety restrictions.
- Released as **Cardputer ADV app-only BIN** plus optional SD data ZIP, not a merged device image. Software tests and checksum provenance in [release notes](docs/RELEASE-v0.8.0.md); **physical audio and input acceptance pending**.


## v0.7z16 — single-file post-STOP incident export (2026-11-14)

- Added one-key `I → R` (only after STOP) on any Diagnostics page: creates a new `/PocketJazz/incident-*.tsv` with station identity, latest ICY, Wi-Fi/HTTPS, buffer/decode/PCM/heap, at most 16 recent runtime events, and heap/source progress checkpoints. The report must end with `END complete=1`; partial results are explicitly invalid.
- No automatic SD writes, second audio task, password/SSID/IP/URL storage or extra audio hot-path work. Existing `E/M/X` are unchanged, and home-screen `R` still reloads playlists. The report does not fix rare BUFFERING; user/device issue remains open.
- ESP32-S3 build (no PSRAM): **92,468 B static RAM**, 1,632,768-byte app-only BIN; **297 Python** and **54 Windows CTest** checks passed. **Physical Cardputer SD/FAT/speaker acceptance pending.**
- [v0.7z16 details](docs/RELEASE-v0.7z16.md). Earlier release assets remain immutable.


## v0.7z15 — interface and Jazz Spark preview (2026-11-14)

- Fixed Notes search result `Enter` and added `Enter` to leave a biography.
- ICY history now lets you browse the existing **five** recent announcements with Up/Down and read a selected full title on Enter. No automatic SD writing was added.
- Help reopens on its last session page.
- Tuning sound effect maximum channel volume reduced **42 → 8** and scales with the user volume, including MUTE. Actual perceived loudness needs testing on a speaker.
- Optional SD Jazz Spark library now includes **150 facts + 25 attributed quotes**; source-backed, bounded SD reads without a bulk RAM cache. Spark `Enter` advances to the next card. The station catalog is unchanged.
- Full v0.7z15 app-only ESP32-S3 build and 294 Python / 54 Windows host C++ checks passed. Physical user acceptance pending. Rare BUFFERING is an acknowledged non-blocking limitation, not a proven eliminated bug.
- Release: [v0.7z15 preview details](docs/RELEASE-v0.7z15.md). Immutable earlier releases and private user SD files were not changed.


## First-time visitor guide and community introduction — 2026-11-14 (documentation only)

- Added the [three-step quick start](docs/QUICK-START.md), with a direct
  app-only BIN link, first-run Wi-Fi, and a precise list of optional
  microSD files for Jazz Notes and Jazz Spark. The personal
  `04-my-stations.m3u` overwrite warning is prominent.
- Added [Why Pocket Jazz?](docs/WHY-POCKET-JAZZ.md): a narrative about the
  listening and discovery experience, the reasons for a dedicated
  Cardputer interface, and a reusable draft for a community showcase.
- Linked both from the README, installation and feature tour, with an
  unchanged disclaimer about v0.7z14 hardware acceptance and the
  v0.7z13 emulator screenshots. No firmware, playlists or release
  assets were changed.

All notable public preview releases are summarized in English here.
These notes distinguish **software verification** from actual audio and
display acceptance on an M5Stack Cardputer ADV. The detailed historical
engineering journal remains in the frozen source snapshots.

## v0.7z14 — diagnostic preview: RAM & MP3 source visibility (2026-11-14)

- **New app-only preview** for M5Launcher 3, with the v0.7z13 scheduler
  priority and playback/retry behavior preserved.
- One fixed Notes search workspace replaces two permanently allocated result
  arrays. Measured **92,460 B static RAM**, down from **94,140 B** on v0.7z13
  (net saving **1,680 B** including new telemetry).
- Input source-progress observations now accompany the first recorded MP3
  shortage in a **manual, post-STOP** heap-lifecycle TSV: source byte count,
  idle time and longest completed gap.
- Actual release packaging and source SHA-256 verified. **289/289 Python**
  and **54/54 host C++ tests PASS**, plus ESP32-S3 no-PSRAM compile and three
  optional compilation checks. Physical speaker audio on v0.7z14 remains
  **untested**; the rare BUFFERING interruption is an open issue.
- The v0.7z13 gallery is intentionally retained as a historically accurate
  emulator capture and is **not** labelled as a screenshot of v0.7z14.
- No launcher, flash partition, device NVS or private SD content was touched.

[Full release details and guarded install instructions](docs/RELEASE-v0.7z14.md).

## Documentation showcase — 2026-11-14 (no firmware change)

- Reworked the public README and added an accessible, detailed
  [Feature Tour](docs/FEATURE-TOUR.md) covering listening, artist notes,
  favorites, clock/alarm, offline music, personalization and diagnostics.
- Captured eight real LCD views from the local v0.7z13 BIN running in
  a headless Velxio Cardputer ADV simulator. The application segment was
  independently compared with the immutable app-only candidate.
- Replaced giant/upscaled mock screenshot embeds with **compact 360x203
  nearest-neighbour PNGs displayed at their native file dimensions**.
  The new cover uses a labelled editorial composite of real captures.
- Updated [User Guide](docs/USER-GUIDE.md),
  [Emulator provenance](docs/EMULATOR.md) and
  [Development notes](docs/DEVELOPMENT.md) to distinguish actual emulator
  frames from older design previews and physical-device acceptance.
- No firmware binaries, archived source snapshots, release tags, station
  playlists, private SD assets or user settings were changed.

## Curated station catalog 2026.11.14.1 — 2026-11-14 (catalog-only)

- Removed **Groove Salad | SomaFM** (primary and backup stream) from
  `03-mellow-bossa.m3u`; retained the *separately listed* **Groove Salad
  Classic** station pending a separate editorial decision.
- Removed **181.fm Fusion Jazz**, **181.fm Trance Jazz**, and **181.fm
  Acid Jazz** from `02-modern-jazz.m3u` on the owner's request. This
  decision does not independently establish that the three streams
  actually broadcast identical programming.
- The curated catalog now contains **51 stations** and **4 alternate URLs**.
  The updated PC report retains **55 positive November 13 observations**
  for the unchanged URLs; **no new live stream checks** were performed.
  SHA-256/size/count manifest and offline tests were regenerated.
- This changes only the GitHub-downloadable catalog, not firmware binaries,
  frozen source snapshots, or the user's personal SD playlists.

## Curated station catalog 2026.11.13.2 — 2026-11-13 (catalog-only)

- Removed **Sonic Universe | SomaFM** (primary and backup URLs) from
  `catalog/02-modern-jazz.m3u` by editorial request: its programming does
  not fit the intended jazz focus. Other playlists and user-owned files
  are not modified.
- Updated the verified downloadable manifest and checksums to **55**
  stations and **5** backup URLs. The verification report retains **60**
  passing November 13 PC observations for the unchanged URLs; these streams
  were **not re-tested** for this editorial removal.
- This updates the remote catalog used by **D → Enter** at STOP. It is
  **not a firmware release**; existing firmware snapshots and release
  binaries are unchanged.

## [v0.7z12] — 2026-11-13 (software-verified preview)

**Manually verified catalog activation with stronger SD recovery, global
volume and more predictable network startup.**

- Includes the global `-` / `=` volume keyboard fixes, full-width
  UTF-8-safe radio display, compact permanent VU, single clock and
  18-page on-device Help developed in the preceding local candidate.
- Added manual **D → Enter** GitHub catalog refresh for five fixed
  editorial M3U files only, over verified HTTPS with SHA-256, size,
  station-count and 12-list/64-station validation. All personal files
  remain outside the update target.
- Requires successful SD reload and verified station counts *before*
  accepting an update; keeps backup journal and supports rollback.
  Fixed orphaned empty backup directories after failed pending-journal
  writes and disallows ambiguous recovery of damaged commit markers
  after partial backup cleanup.
- Destroys temporary heap-backed manifest buffers before opening the
  TLS playlist downloads; **no measured runtime heap saving claimed**.
- Keeps adaptive 16/32-KiB MP3 rings and an 8-KiB startup prefill
  goal; corrected a root-relative HTTPS redirect for host-only query
  URLs. TLS verification and hostname checks stay enabled.
- Moved Help preview test output to a temporary directory, corrected
  Radio-only Tab search instructions and added source-snapshot and
  recovery regression checks.
- **284 Python + 53 C++ host tests and no-PSRAM ESP32-S3 build PASS**.
  App-only BIN SHA-256:
  `9cec36e22eaa191e6491d6f17107f6c3cdfe070f783b45acd7a24c87fcbc3de4`.
  Static RAM **94,140 bytes**. **Physical audio, FAT recovery and
  screen/keyboard acceptance remain open.**

[Full v0.7z12 notes](docs/RELEASE-v0.7z12.md).

## [v0.7z11] — 2026-11-13 (software-verified preview)

**Global volume controls, rollback-safe manual GitHub station catalog refresh,
and an HTTPS redirect fix.**

- Made `-` / `=` effective from Help, Clock, Diagnostics, lists, offline
  MP3 and on dimmed display. Retained normal punctuation input with Fn
  volume shortcuts in password/search fields and the alarm's temporary volume.
- Prevented UTF-8 truncation from splitting a multi-byte artist, track or
  station-name character. Help now includes a separate catalog-update page.
- Added **D → Enter** (only with audio STOPPED): verified HTTPS from this
  repository, five strictly allowlisted official M3U files, SHA-256, byte
  lengths and station-capacity checks, two-phase SD rename/rollback and
  positive activation after reload. Personal M3U/PLS never targeted.
- Fixed root-relative HTTPS redirects when the original authority is
  followed by a query without a slash, keeping CA/hostname validation.
- Increased the **initial compressed-MP3 prefill target** from 2 to 8 KiB
  on a 32-KiB ring (16-KiB mode was already 8 KiB), without forcing a
  larger RAM allocation. A five-second startup timeout remains.
- **279 Python tests / 53 C++ host tests / ESP32-S3 build PASS**.
  App-only SHA-256:
  `eb34df21d5bcb1d8be692aeb81c87aea7e324f7ee0340dff9c3ea7128314d754`.
  Compile-reported static RAM **94,140 bytes** (+16 vs v0.7z10).
- **Physical sound continuity, SD power-loss recovery and new UI remain
  unverified.** No full-flash v0.7z11 was made; v0.7z10 remains available.

[Detailed v0.7z11 notes](docs/RELEASE-v0.7z11.md).

## [v0.7z10] — 2026-11-13

**Full-width player, clearer clock, and a heap lifecycle diagnostic.**

### Changed

- Replaced the narrow station-text column with a 222-pixel full-width
  content area for station, artist and track. Up to two text lines
  per field, with UTF-8-aware truncation and no scrolling marquee.
- Moved the compact VU below the text; VU is now always on, with a
  `VOL xx%` readout. Removed the empty `VU OFF` sidebar and the
  previous V key toggle.
- Simplified the Blue Note clock, retaining one timezone line and
  a clearer `DEL BACK` footer instead of duplicate NTP/UTC labels.

### Added

- Low-overhead memory checkpoints around compressed-buffer allocation,
  HTTP/TLS opening, prefill, decoder startup, first PCM, playback
  low-water and cleanup, including the first stream starvation.
- Explicit **Diagnostics → Memory 7/8 → M** export after STOP to
  `/PocketJazz/heap-lifecycle.tsv`. No SD logging during playback.

### Verification and limits

- **246/246 Python tests, 50/50 C++ host tests and ESP32-S3 build PASS.**
- App-only BIN: **1,604,480 bytes**, SHA-256
  `d8e470af443ac7f521eb959ffa8411eeff7ee13138ea4e329741a01e5049372b`.
- **Physical LCD layout, optional effects and long-session audio
  stability remain unverified** for this version. The previous v0.7z9
  is retained for rollback. TLS verification was not disabled.

## [v0.7z9] — 2026-11-13

**Fixed the clock showing UTC time while displaying a saved UTC+2 label.**

- Replaced the NTP startup `configTime(0, 0, ...)` call, which reset
  the active timezone, with `configTzTime(clockTz, ...)`.
- Preserves the stored NVS UTC offset or an SD POSIX timezone rule,
  including daylight-saving transitions where the rule applies.
- **246/246 Python, 49/49 C++ host tests and ESP32-S3 build PASS**.
- App-only SHA-256:
  `b69f40e5d2da593ef9564761fcf13d29856189b3ee4ac5ef1ea75ebc5fa16d7a`.
- The user subsequently confirmed the corrected local time on real
  hardware. This does **not** constitute full audio acceptance.

## [v0.7z8] — 2026-11-13

**Smaller RAM footprint and a more focused interface.**

- Replaced always-reserved station alternate URL fields with a
  bounded sparse string pool.
- Removed the unused 8-KiB station-logo cache and the separate SIGNAL
  page. Moved its useful metrics to eight-page Diagnostics.
- Unified the clock modes, removed the old marquee and added static
  station/artist/track text.
- Added two independent, OFF-by-default NVS settings for quiet vinyl
  crackle and white noise during initial stream prefill. Effects use a
  separate virtual speaker channel, fade after first accepted PCM and
  are skipped under memory pressure.
- **Static RAM decreased from 114,044 to 93,660 bytes** in the
  specified build configuration.
- **240/240 Python and 49/49 C++ host tests PASS**. Physical audio
  quality of the effects not verified.

## [v0.7z7] — 2026-11-13

**Adaptive compressed MP3 ring to reduce TLS memory pressure.**

- Introduced adaptive allocation of **16 KiB or 32 KiB** depending
  on free heap before ring reservation. If the 32-KiB allocation
  fails, it attempts 16 KiB.
- The smaller ring starts playback after an 8-KiB prefill; the larger
  ring still used a 2-KiB prefill target.
- **234/234 Python, 47/47 C++ host tests PASS**.
- On physical Cardputer, the user reported noticeably more stable
  sound and stations that had previously failed to play. Occasional
  buffering remained; this is **not** a complete audio fix.

## [v0.7z6 and v0.7z5] — 2026-11-12

- Reworked the lifetime/order of the input ring and libmad workspace:
  reserve the ring before TLS, allocate decoder memory after prefill,
  and clean up on STOP/retry instead of retaining large blocks
  indefinitely.
- Improved eight-page Diagnostics labels, failure codes and memory
  observability.
- Confirmed and investigated real-world low-memory/TLS allocation
  failures; fixes were software-verified, not proved acoustically.

## [v0.7z4 – v0.7z] — 2026-11-11/12

- Improved TLS resource lifetime, strict HTTPS redirect handling,
  stage timing, nonblocking ICY parsing and response-body accounting.
- Added diagnostics for DNS/TCP/TLS, compressed audio buffering,
  PCM timing and first-error reporting.
- Fixed an HTTP `host?query` redirect normalization corner case and
  prevented volume-only commands from cancelling an active connection.
- Kept CA and hostname checks enabled; no silent HTTPS→HTTP downgrade.

## Earlier development previews — 2026-11

- Curated M3U playlists, personal station files, saved presets and
  Wi-Fi setup.
- Offline MP3 library, Jazz Notes, Jazz Spark, artist/history search,
  Likes, QR lookups, daily radio alarm, sleep timer and clock.
- SD settings backup, diagnostic TSV exports and hardware-oriented
  FreeRTOS/PCM tests.

The full historical engineering details, older experimental versions
and their hashes are preserved under the source snapshot's
[`docs/CHANGELOG.md`](firmware/docs/CHANGELOG.md) (original Russian
archive). This **English changelog** is the maintained public release
summary. GitHub Releases are the source of downloadable versioned BINs.
