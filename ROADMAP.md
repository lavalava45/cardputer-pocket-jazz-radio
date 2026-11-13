# Catalog roadmap

This is a station-data project; **firmware lives elsewhere**. No bootloader,
partition table, NVS settings, launcher files or user-owned SD playlists are
published or managed here.

## Done — 2026-11-13

- [x] Publish a separate, public Cardputer-specific repository with curated
  playlists and contribution instructions.
- [x] Publish the first set: **56 primary MP3 stations plus 6 fallback URLs**,
  five M3U files with no personal stations.
- [x] Add a deterministic manifest with SHA-256 and exact byte sizes.
- [x] Run a PC TLS+MPEG/ICY verification with the actual Cardputer headers,
  preserving the dated report (62/62 at the observation time).
- [x] Add offline validation tests and GitHub Actions. Network verification
  intentionally runs only when explicitly requested by the maintainer.

## Next — high priority in Pocket Jazz Radio

- [ ] **Create a manual in-device `Check/update default catalog` action.**
  Only while playback is stopped. It must fetch this repository's versioned
  manifest and default M3U files through certificate- and hostname-verified
  HTTPS (no insecure mode, no HTTPS→HTTP redirect fallback).
- [ ] Validate manifest version, file list, per-file lengths, SHA-256, M3U
  schema, primary/alternate URLs, total size and the radio's **64 stations /
  12 playlists** limits *before activating anything*.
- [ ] Stage into temporary SD files, read back all bytes, and implement
  an explicitly testable two-phase swap/rollback. A reset or removed SD
  card must never leave a half-installed default catalog.
- [ ] **Never edit or delete `04-my-stations.m3u` or any other personal
  playlist.** If default+personal station counts exceed firmware capacity,
  fail safely and explain why rather than silently dropping stations.
- [ ] Keep an offline usable default set bundled with the app for missing
  network/SD and never require a catalog update to play existing stations.
- [ ] Test failure injection: partial download, corrupt manifest/hash,
  unsafe redirect, invalid station formats, full SD, cold reboot midway
  through replacement, multiple personal playlists and unavailable GitHub.
- [ ] Show the installed catalog version and last successful local update
  in Settings/Diagnostics. No autonomous background network operations
  while playing a stream.

## Later

- [ ] Record real Cardputer playback observations separately from PC
  transport/MP3-header verification; neither one guarantees availability
  tomorrow or on other networks.
- [ ] Evaluate a paged catalog index that does not keep every station URL
  permanently in ESP32 internal RAM.
- [ ] Consider opt-in manual search via Radio Browser, as a *separate*
  workflow for adding **personal** stations. Never conflate that with
  curated default catalog updates.

Publication rules: version and report date must change with each verified
catalog release; run `python scripts/catalog_manifest.py --check` and
`python -m unittest discover -s tests -v` before pushing.
