# Pocket Jazz Radio release packaging

`tools/build.ps1` compiles and packages a versioned **app-only** ESP32 application for M5Launcher 3. No USB flashing, bootloader installation, partition changes, or erase operations are performed.

**Current packaging policy (12.11.2026):** the previous `0x150000` (~1.3 MiB) limit was an **unsupported project-side guess**, not a verified capacity limit of M5Launcher 3. This old threshold has been removed from `tools/build.ps1` and from the active ROADMAP. Arduino must still compile successfully for the selected board/partition scheme; packaging also retains an independent `0x400000` (4 MiB) **malformed-image sanity cap, not a statement about the actual M5Launcher APP slot**. Firmware is packaged as an app-only BIN, not a merged/bootloader/partition image. The legacy `-PackageUnverifiedPManSize` argument is accepted as a no-op solely for old automation scripts. `PackageOnly` without proven matching sources remains **NOT CLEARED FOR INSTALL** for provenance reasons, unrelated to image size. No flashing/repartitioning occurs during packaging.

From `apps/pocket-jazz-radio`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Version 0.2a
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Version 0.7z4
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -PackageOnly -AllowUnverifiedPackageOnly -Version 99.98
```

The final command is for diagnostic archive-only tests; it packages an *existing* `dist/build/PocketJazzRadio.ino.bin` without compiling and explicitly marks its source `UNVERIFIED BUILD SOURCE` and `NOT CLEARED FOR INSTALL`. Do not use its resulting archive for device installation. The default version remains `0.1.1` for compatibility, but **pass a fresh explicit `-Version` for every new release**. Existing release directories are immutable; no force-overwrite option exists.

Each release is stored in `dist/releases/<version>/`:

```text
firmware/PocketJazzRadio-v<version>-app-only.bin
PocketJazz/*.m3u and *.pls (where present)
README-INSTALL.txt
PocketJazzRadio-v<version>-Launcher3-SD.zip
SHA256-MANIFEST.txt
source/PocketJazzRadio/PocketJazzRadioApp.cpp
source/PocketJazzRadio/PocketJazzRadioApp.h
source/PocketJazzRadio/PocketJazzRadio.ino
source/PocketJazzRadio/AudioOutputM5Speaker.h
source/PocketJazzRadio/SecureICYSource.cpp (when present)
source/PocketJazzRadio/SecureICYSource.h (when present)
source/README.md
source/ROADMAP.md
source/docs/KEYS.md
source/docs/CHANGELOG.md
source/docs/build-release-notes.md
source/docs/SD-TIMESHIFT-FEASIBILITY.md (when present)
source/docs/STATION-LOGOS.md (v0.6a+)
source/docs/SD-EXPORTS.md (v0.7a+)
source/docs/ICY-METADATA.md (v0.7b+)
source/tools/build.ps1
source/tools/*.py and source/tests/*.py,*.cpp (when present)
source/tests/pcm/CMakeLists.txt (when present)
source/experiments/*.h (when present)
source/playlists/*.m3u and *.pls
source/SHA256-SOURCES.txt
TEST-REPORT.txt
HOST-CTEST-REPORT.txt
BUILD-INPUTS.txt
source/arduino-sketchbook/libraries/*/library.properties (when present)
source/arduino-sketchbook/libraries/ESP8266Audio/src/ selected files
```

The ZIP has `firmware/`, `PocketJazz/`, and `README-INSTALL.txt` at its root. Starting with **v0.6b**, the packager explicitly writes ZIP entry names with forward slashes (`/`) for portable extraction. The historical v0.6a ZIP contains Windows-style backslashes in entries; do not alter that immutable artifact. The separate `source/` snapshot is retained in the release folder only; it is **not included in the SD ZIP**. `source/SHA256-SOURCES.txt` lists SHA-256 digests of source files. `SHA256-MANIFEST.txt` records the BIN/ZIP hashes, size, image header and size clearance. Packaging requires `0xE9` first byte and at least 512 bytes. By default the maximum is `0x150000`; with the explicit archive-only switch the hard maximum is `0x400000`. Neither threshold proves that the real installed launcher has sufficient APP space.

**Mandatory release tests:** `tools/build.ps1` rejects missing, empty or failing Python unittest discovery. C++ tests are **mandatory**: missing CMakeLists, zero executed tests, C++ compilation failure or failed assertion block release publication. Release-mode tests explicitly activate `assert`, unlike the original v0.3b/v0.4a reports. `TEST-REPORT.txt`, `HOST-CTEST-REPORT.txt` and `BUILD-INPUTS.txt` are hashed in the manifest. Packaging fixtures include a synthetic C++ test so they exercise the real gate, including deliberately failing and missing cases. This is host validation, not physical Cardputer testing.

**Process correctness (v0.6b):** Windows PowerShell 5.1 can block on resident descendant processes when `Start-Process -Wait` is used, and can report an unset `ExitCode` after manual `WaitForExit` with `Start-Process -PassThru`. Python unittest and CTest now run via `System.Diagnostics.Process` with both output streams consumed asynchronously and the direct child's exit status checked. A positive fixture proves publication with valid hash reports; negative fixtures ensure failed/missing tests still block it. No `PackageOnly` shortcut is taken for full releases.

Source snapshots are created **only for newly packaged versions**; all past releases remain immutable. Full builds check sketch SHA-256 hashes before and after Arduino compilation, before archiving and after host tests. Build metadata records FQBN, library descriptors and selected ESP8266Audio files, **not a complete hermetic Arduino toolchain**. `PackageOnly` cannot prove BIN provenance and is explicitly not cleared for installation. Do not change source files during a release build.

Pre-existing `dist/release/`, `dist/PocketJazzRadio-v0.1.0-Launcher3-SD.zip`, and `dist/BUILD-MANIFEST.txt` belong to the original `v0.1.0` packaging and are retained unchanged. Intermediate `.merged.bin`, `.bootloader.bin`, `.partitions.bin` in `dist/build` are diagnostic files and must never be selected for Launcher installation.
