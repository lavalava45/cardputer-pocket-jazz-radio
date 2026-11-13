# Building and verifying Pocket Jazz Radio

## Source layout

`firmware/` is the **immutable source snapshot** corresponding to
the published v0.7z10 app-only release. The root-level `catalog/`,
`scripts/` and `tests/` provide separately versioned station data
and its desktop verification.

The project targets **M5Stack Cardputer ADV, ESP32-S3, no PSRAM**.
The reference build used a pinned M5Stack Arduino-ESP32 package
(3.3.9), M5Unified and a modified ESP8266Audio/libmad stack.
The frozen `firmware/arduino-sketchbook/` tree contains the project's
adapted library sources and license notices.

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
- Python unit/contract tests (**246 PASS** for v0.7z10).
- C++ host tests (**50 PASS** for v0.7z10).
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

[Velxio](EMULATOR.md) can load a merged ESP32-S3 image to examine boot
and some display/key paths. Its Wi-Fi and speaker environment differs
from a physical Cardputer. The README's illustrated LCD views are
**clearly labelled mock layout previews**, not emulator or device
screenshots. They can be regenerated without connecting to the user's
hardware or modifying an immutable release.

## Licensing and safety

The original firmware is GPL-3.0-or-later, and the modified upstream
libraries retain their notices and licenses. The station catalog tools
and metadata are MIT; no license to the broadcasters' sound streams
is granted.

**Never** upload personal Wi-Fi credentials, SD backups, or private
`04-my-stations.m3u` content. Do not run flash/erase commands as part
of routine source or documentation maintenance.
