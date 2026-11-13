# Changelog

All notable public preview releases are summarized in English here.
These notes distinguish **software verification** from actual audio and
display acceptance on an M5Stack Cardputer ADV. The detailed historical
engineering journal remains in the frozen source snapshots.

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
