# Contributing to Pocket Jazz Radio

Thanks for helping make a small radio more reliable and enjoyable.
Contributions are welcome for **both the Cardputer firmware and the
curated jazz station catalog**.

## A useful issue report

Please include the **firmware version**, Cardputer model (ADV),
whether you launch via M5Launcher 3 or standalone, the exact feature
and the shortest reproducible steps. A photo of the actual LCD helps
for interface problems. For audio problems, include the station and
its stream URL, whether another station works, and the first
available diagnostic counters / error.

On v0.7z10, the **Memory 7/8 → M** action after stopping playback
can export a `heap-lifecycle.tsv` trace. Before attaching any TSV
or SD file, inspect it for private SSIDs, credentials, locations,
personal station URLs or other sensitive information.

Please **distinguish hardware observation from a software test**.
A passed host test does not prove continuous sound on the ESP32-S3.

## Firmware changes

The archived firmware source is under [`firmware/`](firmware/).
Changes should be based on a source branch or a new working branch,
not a silent rewrite of an already published BIN or source snapshot.

Our important invariants:

- Target **M5Stack Cardputer ADV / ESP32-S3 without PSRAM**.
- Preserve verified TLS certificates and hostnames; never use
  `setInsecure()` or silently downgrade HTTPS to HTTP.
- Keep the app-only M5Launcher 3 installation path working.
  Never flash, erase, repartition or modify user SD/NVS content
  as an automatic part of development.
- Retain Wi-Fi profiles, presets, settings backup, offline MP3,
  clock, alarm, diagnostics and existing keyboard shortcuts.
- Do not add blocking work, SD writes or dynamic allocations
  inside the PCM/audio hot path.
- Include regression tests and discuss the actual memory impact.

The archived v0.7z10 reference gate passed **246 Python, 50 C++**
tests and an Arduino-ESP32-S3 build. Consult
[development notes](docs/DEVELOPMENT.md) before changing the
audio/network internals.

## Curated station proposals

For a new/changed station provide its official name, country,
direct **HTTPS MP3/ICY** stream URL, and an optional backup stream
URL from the broadcaster. Never include private login tokens
or full downloaded music files. We currently do not include AAC,
HLS, DASH or dedicated blues stations in the editorial defaults.

Run the PC verifier when possible:

```bash
python -m pip install requests
python scripts/verify_streams.py --report checks/pc-stream-check-YYYYMMDD.json
```

This checks HTTPS and three aligned MP3 frames, including removal
of ICY metadata. It does not prove permanent broadcaster uptime
or playback on the physical Cardputer.

Keep five curated M3U filenames and their `#EXTINF`,
`#PJ-ALT`, `#PJ-MOOD` fields intact. The maintainer publishes
a new dated report and manifest when changing the catalog.
The current firmware limit is **64 stations / 12 playlists**,
including personal stations.

**Never include or alter a listener's `04-my-stations.m3u`.**

## Copyright

Use original code, illustrations and text or material with suitable
licenses. Preserve third-party notices in the adapted audio
libraries. Do not upload broadcaster audio, album covers or
trademark artwork without permission.
