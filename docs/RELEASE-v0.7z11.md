# Pocket Jazz Radio v0.7z11 — global volume, verified catalog refresh

**Software-verified PREVIEW · 13 November 2026 · Cardputer ADV / ESP32-S3**

The new build is **app-only** for M5Launcher 3. It has **not** been
accepted on the physical Cardputer for uninterrupted sound, new screens,
or SD update power-loss recovery. Keep a previous working BIN for rollback.

## Highlights

- **Volume controls anywhere:** `-` and `=` adjust volume in Radio,
  Help, Clock, Diagnostics, playlists and offline MP3, including the
  first keypress that wakes a dimmed display. In Wi-Fi/password/search
  input, they remain literal text; **Fn+- / Fn+=** adjust volume.
  Alarm playback retains its separate temporary target.
- **UTF-8 safe text:** long station, artist and song names are shortened
  only at complete UTF-8 code points. Main player still uses full-width
  text, a permanent compact VU and `VOL xx%`.
- **Manual verified catalog update:** `D` from the radio screen, then
  `Enter` to confirm, **with radio and offline playback stopped**.
  Downloads `manifest.json` and exactly five official M3U playlists
  from this **same GitHub repository** via validated HTTPS. Checks
  SHA-256, byte counts, playlist contents and 64-station/12-list total,
  including user's existing M3U and PLS entries. Updates are staged
  on SD with a pending journal and original backups.
- **Two-phase activation and recovery:** downloaded files are *not*
  reported as installed until the application rereads them from SD and
  matches each playlist's station count to the manifest. If reload fails,
  a distinct activation error triggers rollback or leaves a recoverable
  pending journal for the next card mount. An incomplete commit marker
  must never authorize discarding backups.
- **HTTPS redirect fix:** handles `https://host?key=...` followed by a
  root-relative redirect without treating query text as part of the
  hostname; certificate and hostname verification remain enabled.
- **Startup buffering:** both adaptive 16- and 32-KiB MP3 input rings
  now aim for **8 KiB of compressed prefill**, subject to the existing
  five-second limit. The ring is **not enlarged automatically**, and
  24-KiB remains a measurement-dependent future option.

## Download

**Recommended, M5Launcher 3 (preserves launcher/partitions):**

- `PocketJazzRadio-v0.7z11-app-only.bin` — **1,626,080 bytes**
- SHA-256:
  `eb34df21d5bcb1d8be692aeb81c87aea7e324f7ee0340dff9c3ea7128314d754`

**Optional data:** `PocketJazzRadio-v0.7z11-Launcher3-SD.zip`,
SHA-256:
`47eff7bded6b0041dd584283f8f90dcf02d83e8061ff61094644c3a11173c436`.
Never overwrite a personal `04-my-stations.m3u` when copying the ZIP.

There is **no v0.7z11 full-flash/M5Burner image**. The older v0.7z10
full-flash asset belongs only to that prior release and replaces the
launcher; it is not a normal update option.

## Software verification

| Check | Result |
| --- | --- |
| Python unit, contract and host failure-scenario tests | **279/279 PASS** |
| C++ host/CTest tests | **53/53 PASS** |
| Cardputer ADV Arduino-ESP32-S3 build, PSRAM disabled | **PASS** |
| Sketch size | 1,625,939 bytes (48% of configured app maximum) |
| Compile-reported static RAM | **94,140 bytes** (28% of 327,680-byte report window) |
| ZIP / BIN / source-input SHA-256 and release gate | **PASS** |
| Physical sound, actual TLS reconnect, FAT update, live LCD | **NOT TESTED** |

Measured by the same compiler/toolchain, the static RAM figure is
**16 bytes higher than v0.7z10's 94,124 bytes**, and **19,904 bytes
lower than v0.7z6's 114,044 bytes**. This is a **compile-time static
RAM difference**, not a free-heap or largest-contiguous-block reading
while TLS and libmad are operating. This candidate does not claim any
additional measured runtime heap recovery.

## Outstanding audio issue

An earlier physical v0.7z6 `audio-runtime.tsv` showed MP3 input
buffer depletion from **18,432 bytes to zero** with PCM gaps. At the
low point, the device reported **8,608 bytes total free heap and
1,332 bytes largest block**; a separate HTTPS reconnect failed with
`-32512` (`MBEDTLS_ERR_SSL_ALLOC_FAILED`). These observations
support severe allocation pressure, but they **do not establish why
upstream MP3 bytes first stopped arriving**.

The bounded cooperative HTTPS reads, ICY metadata handling, de-chunking
and decoder/source/ring cleanup have host-level regression coverage.
Host models also cover `stream interruption → cleanup → new station
generation → accepted PCM` and transactional catalog power-loss steps.
Neither is an actual device handshake or audible-speaker test.

## What to test on the device

Use the [hardware acceptance checklist](HARDWARE-ACCEPTANCE.md):
volume keys on every screen (including Fn text exceptions and wake),
long Cyrillic/accented names, 10–20 minutes of live MP3, STOP/reconnect
and station switches without reboot. After a gap, stop playback and
export **Diagnostics → Memory 7/8 → M** to
`/PocketJazz/heap-lifecycle.tsv`. Before testing a catalog update,
back up the SD directory and confirm private files stay unchanged.

The [v0.7z10 release](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z10)
remains available for rollback. Installing the app-only BIN through
M5Launcher does not require erasing NVS or changing partitions;
normal application settings still use NVS as before.
