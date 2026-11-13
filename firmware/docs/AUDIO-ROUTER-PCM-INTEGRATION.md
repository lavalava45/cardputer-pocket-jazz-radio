# Pocket Jazz Radio ↔ Audio Router SDK: non-owning PCM mirror integration

**Status (10 November 2026): SDK host-only; NOT integrated into Pocket Jazz
firmware. No physical ADV or USB/BLE audio proof.** The working application's
`AudioOutputM5Speaker` and its buffer-release callback are the **only** owners
of `M5.Speaker`/ES8311. Neither the experimental SDK `CardputerM5SpeakerSink`
nor any new audio sink may call `begin()`, `playRaw()`, `stop()` or register a
release callback while the application owns the speaker.

## Actual safe observation point

In `PocketJazzRadio/AudioOutputM5Speaker.h`, `flush()` sets the current buffer
busy, calls `M5.Speaker.playRaw(...)`, then invokes its existing **one** monitor
callback with exactly the accepted PCM16 stereo buffer and frame count, or
`nullptr,0` on failed `playRaw()` (currently lines 58–73). Buffers are reused
after the speaker task's release callback. `AudioOutputM5Speaker::stop()` drains
the buffers before notifying its monitor of silence. The correct first SDK
insertion point is therefore *inside the already registered monitor chain*,
with **one** copy of each accepted 1–512-frame callback, never inside the
speaker's release callback.

The current production `pcmMonitor()` in `PocketJazzRadioApp.cpp` (lines
277–284) maintains `pcmPeak` and `audioFrames`. It must be kept and invoked
**once per successful legacy buffer**. Calling `output.setMonitor()` again
would *replace* that callback, not create a second subscription. A future
integration must use **one composite callback** that calls the original VU
logic once, and optionally calls the new mirror once. No separate summing,
downmixing, monitor task, UI access or network I/O in that callback.

## SDK contract, deliberately not yet wired into the application

`audio-router-sdk/include/cardputer_audio/CardputerPcmMirror.h` provides
`CardputerPcmMirror<256,3>`: fixed-size nonblocking, one-producer/one-consumer
PCM16 copies, at most **512 stereo frames** per accepted legacy callback split
into one or two blocks of at most **256 frames**. If there is insufficient
capacity for the whole callback, drop the mirror copy; **never delay MP3
playback**. There is no second hardware output, resampler, gain processing or
duplicate monitor accounting. The three-block PCM payload budget is
**3 × 256 × 2 × 2 = 3,072 bytes**, with complete object `sizeof` bounded by
**8 KiB** through SDK compile-time assertions. Actual ESP32 stack/heap under
Wi-Fi/TLS/decoder is not measured.

`beginQuiescent({sampleRate,2}, epoch)` requires a known decoder sample rate,
exactly two channels and a nonzero *strictly increasing* epoch. The current
decoder changes sample format dynamically; start the mirror only after
`AudioOutputM5Speaker::getRate()` is valid and stable, never assume every
station is 44.1 kHz. Any rate/channel change or station/reconnect requires:

1. Stop the mirror **consumer** and stop producing accepted callbacks (on the
   current `audioWorker`), including the decoder's final `output.stop()` drain.
2. With both producer and consumer quiescent, call `stopQuiescent()` to discard
   the old queue; allocate a fresh monotone epoch, failing closed on wrap.
3. After the new stream's rate/channels are known, call `beginQuiescent()` and
   resume bounded copying. Do not share/reconfigure its non-atomic control
   fields concurrently; the normal data channel is SPSC with acquire/release.

Production audio memory already includes 3×512 stereo speaker frames (6,144
bytes), `STREAM_BYTES` network buffer (32,768 bytes), `MP3_CODEC_BYTES`
decoder workspace (29,192 bytes), and an audio task stack; another mirror
consumes scarce no-PSRAM memory and is therefore **OFF until a useful consumer,
RAM profile and complete lifecycle are validated**. A subscriber for USB/BLE
would be a separate future implementation, subject to hardware verification.

## What the current software gate actually proves

The SDK's `nonowning_pcm_mirror_512_split_spsc_epoch_format_lifecycle` CTest
verifies exact stereo identity, splitting, all-or-nothing queue pressure,
generation/rate validation, quiescent reconnect, uint32 head overflow refusal,
and threaded 30,000-attempt SPSC stress with active MSVC Release assertions.
Its Arduino compile-only probe instantiates the **SDK** mirror and the
preallocated PJMF storage adapter with a deliberately unavailable fake SD
device; it does not compile the full Pocket Jazz audio worker with this hook.

**Additional cross-project compile-only validation (completed):**
`tests/pcm_mirror_app_compile/pcm_mirror_app_compile.ino` includes the
**actual** `PocketJazzRadio/AudioOutputM5Speaker.h` and the separate SDK
`cardputer_audio/CardputerPcmMirror.h`. It compiles a composite monitor that
preserves one legacy VU-accounting call and one non-owning SDK mirror copy,
with no `M5.Speaker.begin/playRaw` or second release-callback registration.
The Arduino ESP32-S3 compile-only probe **PASS** used **503,075 B** sketch
storage and **34,908 B** static data; log:
`dist/build/pcm-mirror-app-probe.log` (worktree build evidence, not included in
the historical v0.7d release). `test_sdk_pcm_mirror_app_probe.py` checks its
source contract. This verifies compilation and linking, **not** actual
`AudioOutputM5Speaker::flush()` scheduling or sound on hardware.

**Provenance for the future app release:** `tools/build.ps1` pins the separate
immutable SDK **v0.8e-host-only** source ZIP by exact SHA-256
`867bc067d73900cfcde1f41e8a0d97420bd1782bdc774a076bb822afbe7c6020`,
extracts `AudioRouter.h`, `CardputerPcmMirror.h` and the MIT LICENSE from that
verified archive, and uses *those same bytes* in the mandatory Arduino
speaker/mirror compile-only gate. The two headers and MIT notice are copied
into `release/source/vendor/audio-router-sdk-0.8e-host-only/` with per-file
SHA-256 and pinned ZIP provenance. A failed/missing pin must abort the release;
the builder must never silently use whatever mutable sibling SDK happens
to be installed. Host tests check both the pin and the build gate's contract.
The future app BIN continues to use its original MP3/Speaker implementation.

**Missing before enabling the mirror in firmware:** actual production
`pcmMonitor()` composite integration into the main app, a real consumer with
clear shutdown ownership, complete source and
license provenance for SDK headers in the radio's own release, and sustained
hardware MP3/TLS/Wi-Fi reconnect tests with measured CPU, free heap, largest
block, stack high-water, latency and `audioUnderruns`. Only an app-only BIN
that has independently passed the full current gate may be considered for
PMan assessment; even then the contiguous APP region must be checked
read-only before any install. This document grants **no permission to flash**.
