# Changelog

All notable public preview releases are summarized in English here.
These notes distinguish **software verification** from actual audio and
display acceptance on an M5Stack Cardputer ADV. The detailed historical
engineering journal remains in the frozen source snapshots.

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
