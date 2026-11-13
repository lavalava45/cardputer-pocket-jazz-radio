# Pocket Jazz Radio v0.7z12 — safer station updates and global volume

**PREVIEW · 13 November 2026 · M5Stack Cardputer ADV (ESP32-S3, no PSRAM)**

This is a **software-verified candidate**, not an acoustically accepted
stable firmware. Use the **app-only BIN through M5Launcher 3** and retain
an earlier BIN for rollback.

## What's included

- **Global volume:** `-` / `=` controls remain available in Radio,
  Help, Clock, Diagnostics, lists, local MP3 and even on the first
  keypress that wakes the dimmed display. Password/search fields keep
  literal punctuation; `Fn+-` / `Fn+=` control volume there. The
  sounding alarm retains a separate temporary volume target.
- **Full-width radio display:** permanently visible small VU, text
  `VOL xx%`, station / artist / title over the available width.
  UTF-8 clipping avoids splitting multibyte characters. One Blue Note
  clock with saved timezone controls; Help includes the new catalog
  update instructions and retains its other shortcuts.
- **Manual GitHub catalog updater:** while both network and offline
  audio are completely **STOPPED**, press **D** from Radio and then
  **Enter** to confirm. The updater downloads the manifest and exactly
  five official M3U playlists from this same GitHub repository over
  certificate-verified HTTPS, validating sizes, SHA-256 hashes,
  playlist formats and all 64-station/12-playlist limits.
- **Two-phase SD activation:** saved originals and a transaction journal
  remain until the application successfully rereads all five new
  playlists from SD and confirms their station counts. Failed reload,
  short/corrupted download, missing SD or partial activation produce an
  error and rollback or recoverable pending transaction, not false
  `Catalog updated`. Personal files, including
  `04-my-stations.m3u` and any private PLS/M3U, are never update
  targets. PLS capacity calculation now matches even unusual
  `File1extra` keys accepted by the player.
- **Additional power-loss protection in v0.7z12:** the SD updater
  removes an orphaned *empty* backup directory when pending-journal
  creation failed, but never deletes nonempty unjournaled backups. It
  fails closed if a commit marker is corrupt after a partially completed
  backup cleanup; it does not silently accept mixed old/new catalog
  entries.
- **Reduced transient HTTPS pressure:** the heap-backed in-memory JSON
  manifest and its stream wrapper are destroyed **before** the five
  sequential TLS playlist downloads. This fixes an actual allocation
  lifetime, but no physical free-heap gain has been measured.
- **Live audio changes inherited from v0.7z11:** corrected root-relative
  HTTPS redirects for `https://host?query` URLs, preserved TLS
  certificate/hostname checks and kept adaptive **16/32 KiB** compressed
  MP3 rings. Initial prefill aims for **8 KiB** with a bounded timeout
  in either mode; neither the ring nor the TLS heap is enlarged blindly.
- **More reproducible tests:** Help-page preview tests now write into
  temporary files, not the firmware source tree, and the current Help
  counter is automatically derived from its 18-page table. GitHub host
  CI uses a SHA-checked copy of an authentic M5Cardputer 1.1.1 keyboard
  header, not a made-up keymap.

## Files

**M5Launcher 3 — recommended**

- `PocketJazzRadio-v0.7z12-app-only.bin`
- **Size:** 1,626,464 bytes
- **SHA-256:** `9cec36e22eaa191e6491d6f17107f6c3cdfe070f783b45acd7a24c87fcbc3de4`

**Optional SD sample data**

- `PocketJazzRadio-v0.7z12-Launcher3-SD.zip`
- **SHA-256:** `24050202030c50067d2427e5d8710fbbf1bbd622d9574fb9487965b53a852240`
- Back up `/PocketJazz` first. Never replace your personal
  `04-my-stations.m3u` with the empty example included in a ZIP.

No merged/full-flash v0.7z12 image was made; the old v0.7z10
M5Burner asset belongs only to the earlier release, replaces
M5Launcher and is **not** a normal upgrade.

## Executed gates

| Gate | Result |
| --- | --- |
| Firmware Python unit/host contract tests | **284/284 PASS** |
| Firmware C++ / CTest | **53/53 PASS** |
| ESP32-S3 Arduino compilation, PSRAM disabled | **PASS** |
| Sketch size | 1,626,323 bytes (48% of configured app maximum) |
| Compile-reported static RAM | **94,140 bytes** (28% of 327,680-byte report window) |
| Build inputs, immutable source/BIN and checksums | **PASS** |
| Actual Cardputer audio / SD recovery / Fn keys | **NOT TESTED** |

The static RAM report is unchanged from v0.7z11,
**16 bytes higher than v0.7z10** and **19,904 bytes below v0.7z6**.
These are differences in compile-reported globals, **not** runtime
free heap or largest contiguous TLS allocation.

## What remains uncertain

An earlier real Cardputer v0.7z6 trace showed the compressed MP3 buffer
draining from 18,432 bytes to zero, with PCM underflows. At one point
only **8,608 bytes total heap** and **1,332 bytes largest block**
remained. Separate reconnect attempts produced TLS allocation error
`-32512`. This demonstrates a memory failure at reconnect but **does
not identify why network input initially stopped**.

Source/host regression tests cover cooperative network reads, ICY and
chunked framing, cancellation and cleanup, and a simulated
`outage → cleanup → another station → first PCM` sequence.
That simulation is not a real HTTPS handshake or audible speaker test.

**Minimum hardware acceptance:** use the app-only BIN in the existing
M5Launcher 3, check volume on every screen and long UTF-8 strings,
listen to a known MP3 station for 10–20 minutes and switch stations
without reboot. Export Diagnostics → Memory 7/8 → M after the first
gap to obtain `/PocketJazz/heap-lifecycle.tsv`. Only after backing
up the SD directory should you test manual catalog update and
personal-file integrity. See the full
[hardware checklist](HARDWARE-ACCEPTANCE.md).

Previous [v0.7z10 preview](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z10)
remains available for rollback. M5Launcher, bootloader, partition
table, NVS and actual user SD data were not modified during this build.
