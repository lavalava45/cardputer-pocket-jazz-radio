# Pocket Jazz Radio — source and third-party license notices

The original main application (`PocketJazzRadio/PocketJazzRadioApp.cpp`) is
Copyright (c) 2026 Pocket Jazz Radio contributors, licensed under
**GPL-3.0-or-later**. The full GNU GPL version 3 text is supplied verbatim as
[`LICENSE`](LICENSE). The `or later` permission is expressed by the file's
SPDX identifier; the GPL license text itself is unchanged.

Other original modules retain their **individual SPDX identifiers**. In
particular, files marked `MIT` are not relicensed by this notice.

The release source snapshot contains a selected *subset* of the exact
dependency sources used by the application, not the complete Arduino
toolchain, M5Unified, M5GFX, M5Cardputer, and ESP8266Audio libraries. To
rebuild, install the matching versions listed in `BUILD-INPUTS.txt` and the
source README. Do not infer a fully reproducible toolchain from this archive.

## Included upstream sources

| Component | Role | License / attribution | License file included in release source |
| --- | --- | --- | --- |
| ESP8266Audio 2.4.1 | MP3 decoder, buffered HTTP/ICY source | GNU GPL v3, see upstream license and file headers | `arduino-sketchbook/libraries/ESP8266Audio/LICENSE` |
| M5Unified WebRadio speaker adapter | `AudioOutputM5Speaker.h`, based on M5Stack example | Copyright (c) 2021 M5Stack, MIT | `arduino-sketchbook/libraries/M5Unified/LICENSE` |
| M5GFX 0.2.32 | display and QR dependency (not vendored in full) | M5Stack, MIT as in upstream license | `arduino-sketchbook/libraries/M5GFX/LICENSE` |

The source archive retains upstream source headers and license documents.
M5Cardputer 1.1.1, the M5Stack Arduino core, ESP-IDF, transitive decoder
libraries, and toolchain have their **own** upstream copyright notices and
licenses. Their complete trees are *not* bundled, so use the licenses from
the installed upstream distributions when redistributing those components.

The separate `cardputer-adv/audio-router-sdk` is **not** installed in the
Pocket Jazz application firmware; its original code has its own MIT LICENSE.
For the v0.7e standalone cross-project compile-only probe, the corresponding
source archive may additionally contain the *exact* `AudioRouter.h` and
`CardputerPcmMirror.h` header files from the immutable SDK v0.8e-host-only
release under `source/vendor/audio-router-sdk-0.8e-host-only/`, **including its
verbatim MIT LICENSE**. Their SHA-256 values and pinned external SDK ZIP hash
are recorded in the release's `BUILD-INPUTS.txt` and source manifest. These
headers are compiled only by the SDK interoperability probe and are not
linked into the Pocket Jazz radio BIN.
Experimental ESP-NOW and MQTT modules in this application's source snapshot
also retain their own SPDX notices and are not enabled by the app-only BIN.

MusicBrainz artist/release identifiers, attributions and dataset provenance
are recorded separately in `data/ARTIST-NOTES-PROVENANCE.md` and the TSV.
No unverified biographies or chord charts are bundled.
