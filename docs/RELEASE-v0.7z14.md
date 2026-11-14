# Pocket Jazz Radio v0.7z14 — RAM headroom & stream diagnostics

**PREVIEW / DIAGNOSTIC RELEASE — 14 November 2026**
**M5Stack Cardputer ADV • app-only BIN through M5Launcher 3 • no PSRAM**

This preview preserves the more responsive keyboard and the comparatively
reliable radio audio of the locally tested v0.7z13, while making a small,
verified reduction in static RAM use and adding a way to observe why the
MP3 buffer sometimes runs dry. **It is not a confirmed fix for intermittent
BUFFERING.** Keep the previous known-good v0.7z13 BIN on your SD card.

## What changed

- **Less permanently reserved RAM:** Legacy Jazz Notes search and the
  expanded encyclopedia now share a single, allocation-free result page.
  The exact same three results remain available in either mode.
- **1,680 bytes of net static RAM saved:** the archived v0.7z13 used
  **94,140 bytes**; the RAM+telemetry candidate used **92,460 bytes** in
  the same no-PSRAM ESP32-S3 board configuration. This is a compiler
  measurement, *not* a measured change in run-time Wi-Fi heap fragmentation.
- **Observability when input stalls:** A tiny passive reader tracks the
  MP3 source's existing byte position, without a second socket read.
  Manual `heap-lifecycle.tsv` export includes three additional numbers:
  `source_bytes`, `source_idle_ms`, `source_gap_max_ms`. The first
  recorded stalled attempt is retained as `FIRST_STALL_EVER` even after
  retries, so a later export does not overwrite the useful initial event.
- **Recovery invariants preserved:** The existing bounded live-input
  stall detection (4 seconds), automatic reconnect after actual failure,
  16/32 KiB input-ring selection, 8 KiB prefill goal, and task priority
  **1** were deliberately left unchanged.
- **All existing features retained:** 51-station remotely maintained
  editorial catalog, nine presets, 2 Wi-Fi profiles, offline MP3, Jazz
  Notes & Spark, QR, clock, alarm, global volume, backup and Help.

## Software evidence and remaining limitations

| Check | Result |
| :--- | :--- |
| Python source, data and policy regressions | **289/289 PASS** |
| C++ host tests | **54/54 PASS**, including source monitoring |
| Repeated STOP/cleanup/new stream scenario | **100 simulated cycles PASS**; *not* 100 real TLS sessions |
| ESP32-S3 with PSRAM disabled | **Compiled** |
| Strict HTTPS CA/hostname verification | **Preserved**, no `setInsecure()` or HTTPS→HTTP fallback |
| Real Cardputer sound quality | **Not verified for v0.7z14** |
| Continuous, gap-free radio playback | **Not demonstrated** |

The physical v0.7z13 previously restored working keyboard controls during
Radio Swiss Jazz playback and improved general audible reliability, but a
BUFFERING gap was still observed. This version **does not claim to have
eliminated it**. Existing [audio issue #1](https://github.com/lavalava45/cardputer-pocket-jazz-radio/issues/1)
remains open.

## Install safely

1. **Keep your existing v0.7z13 BIN** on the microSD card for immediate
   rollback. Back up your `/PocketJazz/` folder before copying any optional
   SD data.
2. Download the **`PocketJazzRadio-v0.7z14-app-only.bin`** asset and copy
   it to microSD. Start it through the **already-installed M5Launcher 3**.
3. Do **not** flash the ESP32 bootloader, partitions, an 8 MiB merged
   emulator image, NVS, or your personal SD playlists. Never overwrite
   `/PocketJazz/04-my-stations.m3u`.
4. If your radio is playing, **STOP it** before requesting the current
   remote station catalog using **D → Enter**. The catalog update is
   independent of the firmware.
5. If audio or controls are worse, return to your original v0.7z13
   app-only BIN in M5Launcher; no migration is required.

## If a station falls silent

During a real listening session, note the station and whether the display
shows `BUFFERING`. Do not repeatedly flash different versions. After the
first incident, **STOP playback**. Open **I / Diagnostics**, go to the
memory pages **7/8**, then press **M** for the manual export to
`/PocketJazz/heap-lifecycle.tsv`.

Inspect `FIRST_STALL_EVER` alongside `LOW_FREE`, `LOW_BLOCK`,
`PRE_OPEN`, `CODEC_RESULT` and the old `ring/buffer` counters.
`source_idle_ms` measures time without **observed progress of the MP3
source byte position**; it does *not* tell us whether the broadcaster,
TCP/IP stack, TLS allocator or scheduler is the cause. If available,
also export `audio-runtime.tsv` for context. Keep private Wi-Fi
credentials and custom station URLs out of public reports.

For the detailed keyboard reference and limits, see the
[User Guide](USER-GUIDE.md), the
[Feature Tour](FEATURE-TOUR.md) and the
[hardware acceptance checklist](HARDWARE-ACCEPTANCE.md).

**Preview status: software-tested, hardware audio acceptance pending.**
