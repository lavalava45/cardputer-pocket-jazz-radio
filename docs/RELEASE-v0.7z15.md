# Pocket Jazz Radio v0.7z15 — navigation, quiet tuning & Jazz Spark

**Diagnostic / user-interface preview for M5Stack Cardputer ADV, launched with M5Launcher 3.**

## What changed

- **Jazz Notes (`U`):** fixed an Enter key that was swallowed after searching. Select a result with Up/Down and press Enter to open the biography. Enter or Del returns from the biography.
- **ICY titles (`Y`):** browse the **five most recently received** station metadata announcements with Up/Down. Enter displays the selected title across multiple lines, then Enter/Del returns to the list. The history stays a five-entry RAM ring: it is not an unlimited recording of tracks. The E key still manually appends a bounded TSV after user action.
- **Help (`H`):** reopen the last Help page you visited during this power-on session, rather than resetting to 1/18 every time.
- **Gentle tuning sound:** the optional white-noise/vinyl effect channel cap was reduced from 42 to 8 and scales with the user-set master volume, including mute. It still stops promptly after real PCM begins, does not play on automatic retries, and uses the same audio owner. It does **not** calculate the loudness of a not-yet-playing stream; physical sound levels still need acceptance.
- **Jazz Spark (`Z`):** retains **150 short jazz facts**, now includes **25 source-attributed short quotations**, up from five. The optional `/PocketJazz/jazz-facts.tsv` file in the SD ZIP must be updated to see the new additions. Quotes are selected from the entire file instead of assuming the last five rows, and `Enter` advances the cards. This data is read on demand from SD; it does not become a 175-card RAM cache.

## Known limits and rollback

This is a **preview**, not a claim of flawless gap-free streaming. The owner reports that rare short BUFFERING gaps are **not a blocker** to listening and beta use; that observation does not prove the last stall has been fixed for all stations or networks. [Audio observations / issue #1](https://github.com/lavalava45/cardputer-pocket-jazz-radio/issues/1) remain available for testers.

This version has **no physical Cardputer acceptance yet** for the new features. Please keep your previous v0.7z14 or personally proven v0.7z13 **app-only BIN** on SD for rollback. Use M5Launcher 3 to launch; **do not** flash a merged emulator image, overwrite partitions/bootloader/NVS, or replace your personal `/PocketJazz/04-my-stations.m3u`. Copy only the updated `jazz-facts.tsv` from the optional ZIP if you want the additional quotes.

## Source and testing

The project source, SD datasets, checksums, test evidence and changelog are linked from the repository. A real-device test should verify Jazz Notes Enter, ICY Up/Down and full title, restored Help page, the new Spark selection, and **subjectively faint hiss** at low and high master volumes with mute. Software tests and emulator checks are not evidence of acoustic loudness.

[Quick start](https://github.com/lavalava45/cardputer-pocket-jazz-radio/blob/main/docs/QUICK-START.md) · [Feature tour](https://github.com/lavalava45/cardputer-pocket-jazz-radio/blob/main/docs/FEATURE-TOUR.md) · [Open issue #2 on the old Jazz Notes regression](https://github.com/lavalava45/cardputer-pocket-jazz-radio/issues/2)

## Software validation for this build

- Real Arduino-ESP32-S3 no-PSRAM compilation: **PASS**; **92,468 B global static RAM**. The previously archived v0.7z13 measured 94,140 B.
- 294/294 Python unit/integration tests and 54/54 Windows C++/CTest passed; three optional compile-only integration probes passed.
- App-only BIN: **1,628,592 bytes**, SHA-256 `cb53cf5e4dc82f83ab30fb67eaff69c6bedc6834fbad7693a7e35a947a546983`.
- Launcher 3 SD ZIP SHA-256 `80cccfd6c8e23e956032da25d54b8c755c629f79cfacec8b2c04392879c0a655`.
- Emulator startup and GitHub CI results are tracked separately; neither substitutes for physical speaker or SD-FAT acceptance.
