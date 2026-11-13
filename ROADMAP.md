# Pocket Jazz Radio — unified roadmap

This repository includes firmware and curated station data. **firmware/**
contains an immutable source snapshot of v0.7z9. Changes in the
development working tree are not a published release until validated.

## P0: audio stability and heap profiling

- Measure free and largest contiguous **internal 8-bit and DMA heap**
  at STOPPED, pre-ring allocation, pre/post verified TLS, prefill,
  codec allocation, first PCM, lowest free RAM and first BUFFERING.
- Analyze lifetimes of UI notes/search/encyclopedia data. Most areas
  are currently **static** and cannot simply be freed: evaluate safe
  lazy allocation or reuse between mutually exclusive screens.
- Do not unconditionally restore the 32-KiB MP3 ring without hardware
  evidence. The adaptive policy is 16 KiB below 180000 bytes of free
  heap at allocation, 32 KiB above. v0.7z6 reached only 8608 bytes free
  / 1332 bytes largest during playback, and mbedTLS once returned -32512.
- In 16-KiB mode prefill targets 8192 bytes; in 32-KiB mode only
  2048 bytes. Measure comparable prefill levels and their impact on
  underruns before attributing buffering solely to ring capacity.
- Continue 10–20 minute on-device acceptance, retries and first-stall
  journal captures. Never relax HTTPS certificate and hostname checks.

## P1: one full-width radio interface and clear clock

- Finish a single radio UI with full-width station/artist/track text,
  a permanent compact VU meter and a compact **VOL 27%** label.
  No blank left panel and no VU/TEXT toggle. Check actual LCD clipping
  and UTF-8 wrap on Cardputer.
- Simplify the sole Blue Note clock: large correct time, date, one
  timezone indicator and clear **DEL BACK** hint, with NTP status
  retained in Diagnostics. Correct time was confirmed by the user
  on v0.7z9; new UI has not yet been released.
- Keep M5Launcher 3, Wi-Fi profiles, backup, offline MP3 and alarm
  functional while refining UI.

## P1: update editorial stations from this same GitHub repo

- **Done:** publish the combined public repo, verified station data,
  SHA-256 manifest, tests and CI.
- **Next:** user-invoked update of **default playlists only** while
  radio is stopped. Fetch over verified HTTPS, validate bounded file
  sizes/schema and hashes, stage on SD and implement power-safe rollback.
- Never modify personal SD stations or silently drop them when default
  and user station counts exceed 64 (12 playlists total).
- Test invalid hashes, incomplete downloads, missing network/SD,
  power loss during activation and unchanged personal files.

## Later

- Radio Browser search with manual import of individual personal stations.
- Larger paged catalogs without holding every URL permanently in RAM.
- PC display simulator using shared UI code; not a substitute for audio
  acceptance on the actual ESP32-S3.

For historic details see the archived
[firmware roadmap](firmware/ROADMAP.md) and
[firmware diagnostics](firmware/docs/).
