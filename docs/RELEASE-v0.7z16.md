# Pocket Jazz Radio v0.7z16 — one-key audio incident reports

**Diagnostic preview for M5Stack Cardputer ADV. App-only BIN for existing M5Launcher 3.**

## Why this version exists

Users should not need to photograph individual Diagnostics pages to explain why a stream buffers. The previous version, v0.7z15, had the measurements but split them among `diagnostics.tsv`, `audio-runtime.tsv`, and `heap-lifecycle.tsv`. The data also lacked explicit station context in one report.

**On v0.7z16:** after a buffering episode, stop the radio with **Enter**, open **I / Diagnostics**, then press **R** from any diagnostic page. The application writes **one uniquely named file** `/PocketJazz/incident-<uptime_ms>-<NN>.tsv`.

The TSV includes:

- `BEGIN`: firmware version, UTC time if synced (otherwise uptime), capture time.
- `STATION`: station index, curated group, name and the latest ICY title.
- `WIFI` and `HTTPS`: signal, connection/reconnect details, handshake timing, TLS result and memory before/after connection.
- `STREAM` and `DECODE`: MP3 ring capacity, fill/minimum, underruns, opens, MPEG frame decoding, source/decode/PCM timing maxima.
- `PCM` and `MEMORY`: queue gaps and rejected writes, heap, largest block, task stack and reset reason.
- `EVENT`: last **up to 16** numeric runtime events captured **before STOP**, including historical ring fill, state, Wi-Fi, attempts and errors.
- `HEAP`: available staged heap/source progress, including `FIRST_STALL_EVER` where captured; **this record can describe an earlier station/attempt**, not necessarily the one most recently selected.
- `END complete=1`: last line indicates complete write. If missing, treat the file as **partial**.

The instantaneous stream status is naturally **STOPPED** in the saved report. Use historical `EVENT` and `HEAP` rows to inspect the stall.

## Safeguards, limits and privacy

- **Never while playing:** `R` refuses the export until both the playback state and the requested intent are STOPPED.
- **Manual only:** no SD writes added to the audio task, interrupts, automatic retries or background networking.
- Reports are appended **only to a new application-owned filename**; existing files are not overwritten. Every row is bounded and read back for verification. The maximum file size is **16 KiB**. This does **not** guarantee power-loss durability; low-space/FAT and card-removal tests remain open.
- Neither Wi-Fi credentials, SSID, IP address, stream URLs, NVS, raw music nor audio recordings are included. **Station names and metadata may nevertheless reveal private interests or private station titles**. Review before posting.
- `R` on the ordinary radio home screen still reloads edited SD playlists; `E`, `M`, and `X` on Diagnostics retain their previous behavior.
- This is a **diagnostic UX improvement**, not a fix for occasional empty MP3 input rings. Stalls may still occur on some stations; [audio issue #1](https://github.com/lavalava45/cardputer-pocket-jazz-radio/issues/1) is the place to report reproducible cases.

## Installation and validation

Download only `PocketJazzRadio-v0.7z16-app-only.bin`, copy it to microSD and launch from **M5Launcher 3 → SD**. Keep your previously working `v0.7z15` (or the developer's physically verified `v0.7z13`) BIN for rollback. There is **no reason to overwrite bootloader, partition table, M5Launcher, NVS, personal playlists, likes or any SD user data**. The optional SD ZIP still includes a blank **example** personal M3U, never replace your own `04-my-stations.m3u` with it.

**Software evidence:** ESP32-S3 app-only compilation PASS, **92,468 bytes global static RAM**, **297/297 Python tests**, **54/54 Windows C++/CTest** and optional compile-only component tests. v0.7z16 app-only BIN **1,632,768 bytes**, SHA-256 `a8e1f2cd6b35768fb08618d2a6c5a899ec1ca22224136b307fb57f83f2ce0a36`; Launcher 3 ZIP SHA-256 `090edb5c9f442cb66646709138e378da98e3268bb246fc9028de3cb77de1ca06`. These values are from the frozen local packaging report; CI and release asset checks are tracked separately.

**Important:** The actual Cardputer **speaker, Wi-Fi stream, SD/FAT writing, STOP ordering and keyboard** on v0.7z16 have **not** yet been physically accepted. Neither software compilation nor the emulator can establish acoustic or SD timing reliability.

[Install steps](INSTALL.md) · [Quick start](QUICK-START.md) · [Audio diagnostics explanation](../firmware/docs/SD-EXPORTS.md) · [Earlier v0.7z15 release](RELEASE-v0.7z15.md)
