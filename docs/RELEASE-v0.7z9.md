# Pocket Jazz Radio v0.7z9 — archived app-only candidate

**2026-11-13 · M5Stack Cardputer ADV · M5Launcher 3**

This is the first firmware release published in the unified GitHub
repository. It was already built and sealed in the local project before
the repository was created; its archived BIN and source provenance have
not been rewritten.

## Files

- **PocketJazzRadio-v0.7z9-app-only.bin** — the application-only
  firmware for loading from an existing M5Launcher 3 installation.
- **PocketJazzRadio-v0.7z9-Launcher3-SD.zip** — optional starter SD
  assets. Back up the SD card and preserve all personal playlists.
- Detailed toolchain, hashes, test reports and frozen source:
  see the original [firmware source snapshot](../firmware/)
  and [clock-fix analysis](../firmware/docs/CLOCK-TZ-RESET-FIX-20261113.md).

## What's included

- Adaptive MP3 input ring (16 or 32 KiB based on heap free memory).
- Verified HTTPS with CA chain and hostname checking.
- Compact alternate station URL storage and smaller static RAM footprint.
- Optional prefill-only tuning ambience, disabled by default.
- Single Blue Note clock, corrected NTP time-zone application.
- Station presets, Wi-Fi profiles, Backup, offline MP3, diagnostics.

## Validation and limitations

- ESP32-S3 build: **PASS**, static RAM **93,660 B**.
- Python tests: **246/246 PASS**.
- C++ host tests: **49/49 PASS**.
- SHA-256 of app-only BIN:
  **b69f40e5d2da593ef9564761fcf13d29856189b3ee4ac5ef1ea75ebc5fa16d7a**.
- The user subsequently confirmed that the clock showed the correct local
  time on physical v0.7z9. This **does not** establish full audio or UI
  acceptance.
- **The new always-full-width radio display and simplified clock labels
  are NOT included in v0.7z9**; they are under development for a later
  candidate. See [root roadmap](../ROADMAP.md).

**Installation:** use only app-only BIN through existing M5Launcher 3.
Do not flash a merged full-chip image or alter bootloader, partition
table, NVS or the user's SD files. Hardware acceptance and rollback
remain the user's choice.
