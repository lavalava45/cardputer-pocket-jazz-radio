# Jazz Journey — interconnected offline jazz learning

## Wave13 final edition — source-led learning without speculative Spark (2026-11-15)

Before the public release candidate, 11 additional content-audited jazz artist notes were accepted into indexed TSV, Now Playing and Explained, bringing the exact previews to **595** across **532** individually researched additional bios. All **1,498** encyclopedia entries remain accessible offline, including **903 fallback/machine-enriched records not yet individually corroborated**. A reproducible read-only classification of all these 903 produced **748 leads**, **103 insufficient** and **52 ambiguous**, not 903 verified lesson sources. Original **150** Spark items continue to require exact artist/evidence/claim/source/one-of-24-topics binding; the new 11 were not assigned invented lesson connections. No lesson selects a radio station autonomously, no change to 36 album recommendations, 24 lessons, or player/clock settings. Release status is software preview until physical audio/controls acceptance.


## Current post-wave11 source-audited continuity — wave12 research02

Three exact offline musician previews were added for Chris Pyne (ECM ensemble trombone), Bruce Ditmas (original Postcards *What If*) and Bert Boeren (dated live O.A.P. Records performance), each tied to new independently researched dossiers. Now Playing/Explained has **584** strictly matched captions, alongside **521** individually researched additional biographies and **434** detailed dossiers. The existing **150** exact evidence-claim-to-topic Spark rows, **24** original Jazz Journey lessons, lesson examples and all station-selection semantics are unchanged; no new lesson-topic binding was improvised without a clear historical pedagogical fit. All **1,498** entries are available via the production offline TSV/index; **914** still lack individual historical research and remain explicitly machine-enriched rather than verified. Host checks and source evidence support the data, not physical Cardputer ADV acceptance. Older counts 581/573/570/569/531 below are labelled checkpoints, not present-day values.


## Wave12 researched biographies — host-only post-wave11 update (15 Nov 2026)

The completed wave11 (initial 38; later independently audited 39 and source-QA 42) remains SHA-protected. A **new, separately pinned** wave12 research01 batch extends exact Now Playing/Explained previews from 573 to **581** after eight new source-verified biographies, with **518** researched additional notes and **431** claim-level dossiers. Existing **150** individually identified editorial Jazz Spark facts still map, by exact key → evidence ID → text → HTTPS citation → one of the existing **24** Journey lessons/topics; none was added solely to boost counts, and all originally-approved lesson topics and listening prompts were preserved. Both the Jazz Notes (`N`) source lookup and the Spark (`Z`) lesson navigation remain non-retuning: no station switches without a separate explicit user action. C++ host tests probe each new key through the production `.tsv` and `.idx` files and reject fuzzy name overlays; 1,498 fixed-width artist layouts are exercised. **917** cards are still not individually researched and are only automatically enriched using attributed Wikidata/MusicBrainz claims. This is not physical Cardputer ADV acceptance or a new firmware release.


## Latest source-QA wave11 correction (15 November 2026)

The third, **separately SHA-pinned** wave11 addendum retains the same fixed **24 lessons** and the same **150 editorial Jazz Spark rows**. A combined-source Spark biography was corrected for Carlo Krahmer: the approved `New Orleans jazz` example now repeats exactly `carlo-krahmer -> ev02 -> Jan 11, 1948 Chicagoans session -> JazzDisco URL -> New Orleans jazz`; the different 1948 Esquire-label founding claim has a separate `ev03` with its own Jazz Journal URL and is **not** used as an unsupported alternate Spark source. Re-audit also corrected Stan Tracey's drummer from Jackie Douglas to **Jackie Dougan** (1965 *Under Milk Wood*).

The available exact-name Now Playing / Explained labels increased to **573**, including Fred Guy, George Tucker and Othello Molineaux, whose recording-level claims are in individually pinned dossiers and the offline TSV/index. No automatic fallback is generated for **925** artists without individual editorial research. The third addendum comprises **510** additional individually researched musician cards and **423** individual dossiers with **853** distinct evidence claims. MusicBrainz/Wikidata data is identified as structured/community attribution, **not** primary proof of performance; where session or track attribution remains unresolved the artist is deferred. Every Journey prompt stays informational and never re-tunes the active station. Only host C++/Python integration was tested; no Cardputer ADV acceptance.

### Earlier checkpoints retained as history

The following 570/569/531 caption totals describe respectively the first audited wave11, original wave11 and earlier wave10 snapshots; they are **not current totals**.

## Source-audited wave11 update

The editorial re-audit confirmed and corrected the factual provenance for Jazz Notes' wave11. The fixed curriculum still has **24** lessons and the same **150** evidence-linked Spark facts: no new speculative lesson links, no radio station switching and no new playback behavior. The corrected corpus supplies **570** exact Now Playing/Explained previews and 507 individually researched additional biographies. Nathan Davis's original-label *Happy Girl* recording adds a documented artist biography without introducing an unproved Spark mapping. Every pinned Spark still binds exact artist key → evidence ID → claim → HTTPS source URL → existing topic. The independently audited corpus has 420 dossier records and 845 evidence claims; 928 cards remain without individual editorial expertise. Host-only tests do not imply real ADV screen or audio acceptance.


> **Unreleased local source after v0.7z16.** This feature has **not** been
> compiled into a Cardputer BIN, published as a release or accepted on hardware.
> The public v0.7z16 firmware does **not** have this new learning interface.

## Jazz Notes wave11 integration (host-only, 15 November 2026)

Three wave11 artist facts were bound to existing Journey lessons by exact artist key, evidence ID, claim text, HTTPS citation, and lesson topic: Calvin Newborn (`Hard bop`), Cecil Scott (`Swing`), and Carlo Krahmer (`New Orleans jazz`). There are now 150 evidence-bound editorial Spark facts across the fixed 24-lesson curriculum; no topics were invented, no station-switch or playback action was introduced. Added 38 exact Now Playing hints and 38 researched offline biographies, giving 569 curated captions and 929 non-individually-reviewed cards. The host regression, traceability pins and SD-index tests apply; this is not device acceptance.

**Concept:** Not just another internet radio. A pocket-sized journey through jazz.

The new **E — Jazz Journey** hub connects the existing radio, Jazz Notes and
Jazz Spark to four historical learning trails, a listening guide, a personal
Jazz Passport and a tagged Jazz Diary. Content is educational and curated,
not an automatic identification of whatever the broadcaster currently plays.
**Now Playing, Explained** now has short, sourced captions for **531 exactly
matched** entries (63 protected editorial profiles plus 468 independently
researched musical profiles, including selected ensembles). Unknown
artists get the ordinary radio screen; the preview needs no SD read,
and pressing **N** explicitly opens the offline biography if available.
The 531 flash captions are generated deterministically from a hand-edited
mapping, cross-validated against the existing biographies via
`tools/build_now_playing_hints.py`. There is no invented fallback
description for the other 967 encyclopedia entries.

## Content and links

- `data/jazz-journey.tsv`: **4 trails × 6 stages = 24 lessons**. Each stage
  includes a historical summary, an explicit offline artist key present in
  `jazz-encyclopedia.tsv`, a listening prompt, a three-answer quiz, a Jazz
  Spark topic and a reference URL. Sources include Smithsonian Music,
  Jazz at Lincoln Center, the National Park Service and Smithsonian
  Latin Jazz archives. The prose and quiz wording are original.
- `data/jazz-listen.tsv`: **30 Learn to Listen cards**, each anchored to
  one historical lesson for follow-up. No background station polling.
- `data/JAZZ-JOURNEY-PROVENANCE.md`: source trail and limits.
- **Existing** `jazz-encyclopedia.tsv` and `jazz-facts.tsv` are not
  replaced or automatically downloaded. `N` in a lesson opens the
  specifically linked artist from Jazz Notes; `Z` opens a matching FACT
  topic from Jazz Spark (not a fabricated quotation).
- `F` in a lesson opens the ordinary radio station browser **without**
  retuning until the user explicitly selects a station. A lesson's
  listening prompt is **not** a claim that the current broadcaster is
  airing the referenced artist or style.

The course is **a coherent introductory 24-stage curriculum**, not a
fully annotated scholarly history. The separate **1,498-card Jazz Notes**
directory includes Wikidata statement-backed dates and instruments and
separate, explicitly credited MusicBrainz performance/ensemble/venue links.
The **63 older editorial biographies are preserved verbatim**, with **468**
more source-researched musical profiles. That still does **not** establish
meaningful, characteristic recordings,
collaborators and performance venues for every musician. All biographies
remain bounded to the one-screen offline format; see
[Jazz Encyclopedia](JAZZ-ENCYCLOPEDIA.md) for field-by-field coverage.
Specific music recordings are never bundled, played or distributed.

## Keyboard (unreleased)

| Context | Keys |
| --- | --- |
| RADIO | **E** opens Jazz Journey; **N** opens a contextual Now Playing artist note if ICY says `Artist - Track`; **K** keeps its original `liked.txt` behavior; **Fn+K** opens tagged Diary |
| E hub | Up/Down chooses Trails, Learn to Listen, Passport or Diary; Enter opens; Del returns; **P** saves pending progress/diary **only after STOP** |
| Trails | Up/Down chooses one of 4 trails; Enter starts at its first stage; Del returns |
| Lesson | Up/Down navigates six stages of the current trail; **T** toggles the full listening task and the historical story; Enter opens the quiz; **N** linked Jazz Notes; **Z** matching Jazz Spark; **F** station browse; **V** marks the current lesson as **heard by user's explicit choice**; Del returns; P saves after STOP |
| Quiz | **1/2/3** chooses an answer; correct answers mark **learned** in RAM; wrong answers allow retry. No automatic score for simply opening a lesson |
| Learn to Listen | Up/Down cycles 30 exercises; Enter opens the related historical stage; Del returns |
| Passport | **11 musical styles** (roots, swing, bebop, cool, hard bop, modal, free, Latin, bossa nova, fusion, modern) with six visible rows per page. **Q** (quiz passed) and **H** (user marked hearing) are independent; Up/Down selects a style; Enter opens the first lesson in that style |
| Diary | Up/Down selects `Explore later`, `Beautiful solo` or `New favourite`; Enter queues the current station/title with timestamp **only when ICY title metadata is present**; max **4 pending RAM drafts**; **P** saves after STOP; Del returns |

Global volume remains `-/=` on all screens. An alarm still stops by Enter
even from the course. The remaining existing key mappings are unchanged.
User-facing quiz, SD-export and missing-data messages appear as brief
overlays on every course screen and disappear automatically.

## Storage, privacy, and power-loss safety

**Only user-initiated SD operations:** the two learning catalog scans
are triggered by a key and use bounded streaming reads, never the
FreeRTOS audio worker. A new SD scan is refused while a stream is
connecting/buffering, while playing an offline MP3, or unless the
playing MP3 ring is at least **75% full**. This may still cause audible
latency on some microSD cards: real-device profiling is pending.

Course progress is loaded at boot **before audio starts** from
`/PocketJazz/jazz-progress.tsv`, an optional, append-only journal of
32-byte checksum-verified `JZP1` records. Only the most recent intact
record is applied. Completion uses 24-bit lesson masks:
**learned** (correct quiz) versus **heard** (explicit user declaration),
not inferred listening or background audio recognition. A missing file
means an empty passport. A corrupt/truncated record does not trigger
repair, truncate or overwrite.

**P while fully stopped** saves dirty progress, then queued Diary drafts
to the separate `/PocketJazz/jazz-diary.tsv`. Each draft holds bounded
UTC-or-uptime, sanitized station/title and one of three tags. Saves
are append-only and read back with the existing safe SD exporter.
Capacity is bounded to **16 KiB progress / 64 KiB diary**; on failure
only successfully verified drafts are removed from RAM. There is no
auto-rotation or deletion. Up to four unsaved entries survive within
the current powered-on session **only**, not a power loss.

Neither course progress nor diary data are written to NVS.
No new partitions, credentials, SSIDs, IP addresses, station URLs,
oauth tokens, lyrics, third-party audio or secret keys are stored.
Station/ICY names may be sensitive; the owner controls sharing.

For an optional readable diary page on the PC (not on the Cardputer):

```powershell
python tools/diary_to_html.py "E:\path\jazz-diary.tsv" "E:\path\my-jazz-diary.html"
```

HTML is generated **offline**, escapes text, has no third-party JS or
images. Optional MusicBrainz search links open only if the user clicks.

## Implementation and verification gates

`PocketJazzRadio/JazzJourney.h` contains the host-testable streaming
TSV parser, bounded lesson/tip structs and checksum passport protocol.
`tests/jazz_journey_test.cpp` and `tests/test_jazz_journey_data.py`
verify row count, record bounds, source references and progress integrity.
`tests/test_jazz_journey_integration.py` checks key dispatch, read-only
network behavior, personal SD paths, Notes/Spark and unchanged K/backup.
`tests/test_jazz_diary_html.py` checks HTML escaping.

**Acceptance still required when user permits a BIN build:**
Arduino ESP32-S3 no-PSRAM compile and global/static RAM check, full
Python and host CTest, preview UI/keyboard navigation (especially
20-page Help), real SD reads while live streaming, note/topic lookups,
quiz outcomes and recovery after reboot, full/removed SD, corrupt and
partial progress files, slow SD and actual MP3 underruns, daily alarm,
unchanged M5Launcher/partition/NVS/personal playlists. Until then,
this remains **local source work**, not an installable feature.
