# Jazz Notes — independently versioned editorial data

**Version:** 2026-11-16 / wave38-sourced. **This is NOT a new firmware release.** The public `v0.8.0` source archive, app-only BIN, distribution ZIP, immutable `firmware/SHA256-SOURCES.txt`, and all older release tags remain unchanged.

The current editorial dataset has **1,498 indexed musician entries**, of which **1,313** have an individually source-reviewed biography and exact musician hint (63 original plus 1,250 later individually investigated). Each of the **1,163** later research dossiers contains a concrete chain of `evidence[].id` → factual `claim` → `source_url`, with **2,809** attributed individual claims across all dossiers. The remaining **185** entries retain their general offline entry but **are not** presented as having personally verified histories: **71** with still-insufficient leads, **70** lacking usable source details, and **44** with unresolved identity ambiguity.

## Files

| Folder | Contents | How to use |
| --- | --- | --- |
| `sd/` | `jazz-encyclopedia.tsv`, `jazz-encyclopedia.idx` | Matched pair for offline Jazz Notes search and biographies on an existing Cardputer ADV SD card; **not** a BIN |
| `source/` | `jazz-researched-context.tsv`, `jazz-editorial-dossiers.json` | Human-readable/structured individual-source data for auditing, contributions and future builds |
| `source/` | `JazzNowPlaying.h` | Generated, *future-build source header*: **does not** change the Now Playing feature in already compiled v0.8.0 |
| `source/` | `JAZZ-ENCYCLOPEDIA-PROVENANCE.md` | Generation rules, file checksums, source limitations and index contract |
| `manifest.json` | SHA-256 and byte count for every file, accepted 1300/185 split | Validate the exact dataset before copying anywhere |

## Update **offline Jazz Notes only**, manually and optionally

1. Keep the Cardputer powered off or stop audio playback. Make a **separate backup** of the existing files on the **user's own SD card** at `/PocketJazz/jazz-encyclopedia.tsv` and `/PocketJazz/jazz-encyclopedia.idx`.
2. Copy the **two files together** from this directory's `sd/` into `/PocketJazz/` on the backed-up SD card. Do not copy any executable or full-flash image.
3. Unmount safely, restart the app as normally launched through M5Launcher 3 and use **Jazz Notes (`U`)**. The 1,498-row data index must match the TSV version. If there is a problem, restore your two previous files from backup.

**Do not overwrite** the private `04-my-stations.m3u`, liked tracks, music files or any Wi-Fi credentials. Copying the editorial files never modifies M5Launcher, partitions, firmware, NVS or any cloud account. The URL above is a manual process, not remote access or automatic installation.

### Source-level Now Playing is separate

The installed **v0.8.0** app-only firmware still contains **595** compiled musician-specific hints. Its SD Jazz Notes can be updated independently as described, but for the new **1,313** exact Now Playing / Explained hints to show during listening, a **future separately compiled and tested firmware version** must use this `source/JazzNowPlaying.h` in its source tree, then be released through the normal software workflow. Merely downloading these files or pulling GitHub main does not update a compiled executable. **Do not copy this header to the SD card**.

Source header updates and source merge must be deliberate: do not replace the archived `firmware/PocketJazzRadio/` v0.8.0 snapshot in this repository, as CI pins every byte of that archival snapshot by SHA-256. Development work also includes separate concurrent changes not in this editorial update.

## Checks, provenance and editorial policy

Run `python -m unittest discover -s tests -v` from the repository root to verify the published overlay, index consistency, attributed dossier format, exact counts and SHA-256. The public CI additionally verifies that every original archived `firmware/` byte still equals `firmware/SHA256-SOURCES.txt`.

No musician is promoted solely because a MusicBrainz identifier, a Wikidata entry, an HTTP status or a search snippet exists. Wave38 adds **13 individually sourced musician histories and 39 attributed claims** from original 78-rpm inventories, national library recordings, original label credits, period music journalism and historic archival discographies. Two initially ambiguous IDs are separately resolved as a performer versus his orchestra and a discographer versus his pseudonym, without conflating identities. Distinct original record dates, modern digitization or reissue dates, and non-performance archival credits remain clearly separated. No unsupported featured solos, session personnel or jazz performances are inferred. Cases with ambiguous person identifiers (including Leon Spencer, Scatman John, Rashid Bakr, Sam Brown) remain pending. All 1,300 previous artist histories, v0.8.0 artifacts and personal SD contents remain unchanged; the source-only P2 transaction lock continues to cover the checkpoint verifier.

The unchanged **150 Jazz Spark facts**, **36 album recommendations**, **24 Jazz Journey lessons**, and **30 listening exercises** stay in their respective older v0.8.0 files. These features were not modified or republished by this editorial dataset update. Screens shown elsewhere come from earlier [Velxio Cardputer ADV UI emulator](https://velxio.dev/example/cardputer-adv-m5-display) captures.

For the currently published firmware, see the existing [v0.8.0 preview](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.8.0) and [quick start](../../docs/QUICK-START.md). The editorial `main` branch data is independently versioned without a new v0.8.0 binary.
