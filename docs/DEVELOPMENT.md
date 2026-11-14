# Building and verifying Pocket Jazz Radio

## Source layout

`firmware/` is the **source snapshot** corresponding to the latest
software-preview candidate (v0.7z12). The root-level `catalog/`,
`scripts/` and `tests/` provide separately versioned station data
and its desktop verification.

The project targets **M5Stack Cardputer ADV, ESP32-S3, no PSRAM**.
The reference build used a pinned M5Stack Arduino-ESP32 package
(3.3.9), M5Unified and a modified ESP8266Audio/libmad stack.
The archived `firmware/arduino-sketchbook/` subset contains selected
adapted library sources and license notices. Reproducing the exact
Arduino build on another machine requires the pinned toolchain and
full installed dependencies, not just the firmware source tree.

The public repository includes **host-test-only compatibility files**
under `firmware/tests/runtime-stubs/` and
`firmware/tests/fixtures/`. The latter contains a SHA-checked,
authentic M5Cardputer 1.1.1 upstream keyboard header. These are
for portable CI test compilation and keymap assertions, **not used
in the ESP32 application binary**. The public host keyboard test
references this fixture instead of the developer PC's external
Arduino sketchbook; it is a test-only portability adaptation relative
to the immutable release source archive. The public historical
`firmware/ROADMAP.md` is not the developer's current local roadmap.

## Reproduce the software verification

On a suitable Windows development system, consult
[`firmware/README.md`](../firmware/README.md) and
[`firmware/tools/build.ps1`](../firmware/tools/build.ps1). This
project's PowerShell release script assumes a particular local,
pinned-toolchain layout; it is **not** a universal one-command
installer. Adapt only the build paths on another machine, not the
flash partition scheme on an existing Launcher device.

The verified release script performed:

- Arduino ESP32-S3 compilation and build-input validation.
- Python unit/contract tests (**284 PASS** for v0.7z12).
- C++ host tests (**53 PASS** for v0.7z12).
- Source snapshot, BIN, ZIP and SHA-256 manifest checks.

These gates cannot prove speaker output, radio-server uptime,
physical display layout or intermittent Wi-Fi/audio buffering.

To validate the separate editorial station catalog:

```bash
python scripts/catalog_manifest.py --check
python -m unittest discover -s tests -v
```

To intentionally recheck live broadcaster endpoints on a PC:

```bash
python -m pip install requests
python scripts/verify_streams.py --report checks/pc-stream-check-YYYYMMDD.json
```

The report is a **dated observation**, not a live guarantee. Do not
regenerate the public manifest without reviewing all failures, updating
the version/report reference and validating file hashes.

## Audio architecture and limits

- Streaming sources use plain HTTP only when explicitly configured, or
  verified HTTPS with certificate and hostname checks.
- One FreeRTOS audio owner coordinates network source, MP3 ring,
  libmad decoder and the M5Unified speaker output.
- The compressed stream ring is **16/32 KiB adaptive** according to
  free heap. v0.7z10 adds bounded heap lifecycle checkpoints to
  investigate persistent BUFFERING and contiguous-allocation pressure.
- The UI, Notes and station catalog share scarce internal RAM with TLS
  and audio. The program does not require PSRAM, but arbitrary AAC, HLS
  and DASH playback is not supported.
- The optional vinyl/hiss ambience is disabled by default and skipped
  when memory headroom is inadequate.

For troubleshooting, collect `heap-lifecycle.tsv` from the **Memory
7/8** page only after stopping the stream, together with the existing
audio-runtime report. Do not infer a hardware fix from software-only tests.

## Emulator images and preview assets

[Velxio](EMULATOR.md) can boot merged ESP32-S3 firmware to examine
its LCD and keyboard paths. The README and [Feature Tour](FEATURE-TOUR.md)
now use **actual 240x135 captures from the local v0.7z13 candidate**,
shown as compact 360x203 PNGs instead of stretched design mockups.
Their source app segment matches the local v0.7z13 app-only BIN by SHA-256
and byte comparison. They do **not** prove audio, Wi-Fi or SD on real
hardware; the current public diagnostic preview is v0.7z14. Former illustrative
layout previews are preserved as historical assets.

## Licensing and safety

The original firmware is GPL-3.0-or-later, and the modified upstream
libraries retain their notices and licenses. The station catalog tools
and metadata are MIT; no license to the broadcasters' sound streams
is granted.

**Never** upload personal Wi-Fi credentials, SD backups, or private
`04-my-stations.m3u` content. Do not run flash/erase commands as part
of routine source or documentation maintenance.
