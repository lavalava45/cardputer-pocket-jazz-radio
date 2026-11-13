# Pocket Jazz Radio v0.7z10 — full-width player & memory snapshots

**Preview release · 13 November 2026 · M5Stack Cardputer ADV / ESP32-S3**

This release makes better use of the Cardputer's 240×135 screen and
adds low-overhead memory measurements for diagnosing occasional stream
buffer underruns. This is a **software-verified preview**; the new
LCD layout and continuous audio stability remain subject to physical
device acceptance.

## What's new

- **Full-width station/artist/track text** with bounded UTF-8-aware
  wrapping instead of the old narrow side column and moving marquee.
- **Compact, always-visible analog VU** below the song information,
  alongside volume as `VOL xx%`; removed the V show/hide toggle.
- **Cleaner Blue Note clock**: large time, date, one timezone display,
  and a labelled `DEL BACK` shortcut.
- **Heap lifecycle instrumentation** for pre-ring, TLS, prefill,
  decoder allocation, first PCM, low-memory conditions, first
  buffering and cleanup. After STOP: **I → Memory 7/8 → M** exports
  `/PocketJazz/heap-lifecycle.tsv` manually.
- Retains v0.7z9's timezone fix, the adaptive 16/32-KiB MP3 ring,
  curated playlists and optional OFF-by-default tuning sound effects.

## Downloads — choose the correct format

**Recommended for owners of M5Launcher 3**

- `PocketJazzRadio-v0.7z10-app-only.bin` — **1,604,480 bytes**.
  SHA-256:
  `d8e470af443ac7f521eb959ffa8411eeff7ee13138ea4e329741a01e5049372b`.
  Copy to microSD and run through the existing M5Launcher. Does not
  require repartitioning or replacing the bootloader.

**Optional starter SD files**

- `PocketJazzRadio-v0.7z10-Launcher3-SD.zip` — sample playlists,
  offline Jazz Notes datasets and notices. **Back up the SD card first;
  do not overwrite your personal `04-my-stations.m3u`.**

**Advanced ONLY — standalone full-flash image for M5Burner**

- `PocketJazzRadio-v0.7z10-M5Burner-FULL-FLASH-REPLACES-LAUNCHER.bin`
  — **8,388,608 bytes**, SHA-256:
  `af1898e3cf2e6cfeecd9288e76c81c0d8f323df8193e2b9a32a6025a5b91f83f`.
  The application stored inside at flash offset `0x10000` was
  independently verified **byte-for-byte identical** to the
  app-only v0.7z10 BIN.

  **WARNING: A full-flash install REPLACES M5Launcher, the partition
  table and flash content, and can erase NVS/settings.** It is not
  a normal upgrade. Do not use it on a Cardputer where you want
  M5Launcher or personal flash-resident data preserved. The merged
  image has **not been installed on physical hardware** during this
  project. See the [installation guide](INSTALL.md).

The release also includes the original build SHA-256 manifest and
test reports. Immutable source is in the
[`firmware/`](../firmware/) folder at this tag.

## Verification

| Gate | Result |
| --- | --- |
| Arduino-ESP32-S3 main app build (no PSRAM) | PASS |
| Python regression/contracts | **246/246 PASS** |
| C++ host/CTest | **50/50 PASS** |
| App-only BIN and source SHA-256 | PASS |
| Merged image embedded app vs app-only | **Byte-for-byte PASS** |
| Physical LCD layout / extended live audio | **Not accepted yet** |

**Important:** successful compilation, desktop stream checks and
Velxio emulator boot are not proof of stable audio from the
Cardputer's speaker. The user can return to the preserved
[v0.7z9](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z9)
app-only BIN through M5Launcher.

## Gallery & guide

The [README gallery](../README.md#a-small-screen-a-lot-of-jazz)
shows **source-derived illustrative UI layout previews**, while
the [Velxio documentation](EMULATOR.md) includes a real emulator
boot capture. All controls and installation details are in the
[user guide](USER-GUIDE.md) and [installation guide](INSTALL.md).
