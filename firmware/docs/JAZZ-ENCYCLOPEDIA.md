# Jazz Notes and Jazz Spark — offline guide

## Copy data to SD

The current editable SD corpus includes 1498 performer directory cards and
175 Jazz Spark cards. The older, immutable v0.7h/v0.7z14 ZIPs still contain
their original 155 cards; copying an old ZIP does not add new quotations.
Copy all four data files into **/PocketJazz/** on microSD:

- jazz-encyclopedia.tsv — musician directory, including 63 enhanced editorial biographies, plus 36 verified album suggestions. Many other summaries are still short catalog labels.
- jazz-facts.tsv — 150 editorial historical/theory/listening facts and
  25 attributed short quotes; no catalog is cached in permanent RAM
- artist-notes.tsv and artist-notes.idx — legacy MusicBrainz compatibility pair

The app never downloads these itself; app-only BIN alone has no encyclopedia.
Preserve 04-my-stations.m3u, clock.txt, liked.txt and personal export logs.

## Controls

**U**: type a musician, album, style or topic; Enter searches. In the resulting
list **W/S** (or A/D) moves among ALL matches, three shown per SD page.
Enter opens the highlighted card. W/S turns biography pages. Enter/Del
returns to results, Del again switches to query editing, Tab exits.

**N**: look up the current artist if ICY stream title is explicitly formatted
as Artist - Track. No artist is guessed from a generic station name.

**Z**: Jazz Spark. A sourced historical fact, a listening tip, or occasionally a
short attributed quote. W/S browses; Enter moves forward; Z/Del returns. **T on the Spark
screen** enables/disables automatic Spark reminders, saved in NVS (default ON).
Every 12 minutes the radio may automatically display a card for 10 seconds
and return to RADIO, but only when idle, with microSD available and audio
buffer at least half full if playing. This still needs a real-device test for
SD latency, dropped audio and screen rendering.

**H**: keyboard help, four consistently formatted pages. W/S or A/D works
in both directions.

All data is read from SD in bounded read-only chunks on the UI task, not by
the FreeRTOS audio worker. Searches are initiated by a key; auto Spark
is the only scheduled SD scan. The entire database is never loaded into
RAM. Slow microSD scans can cause a visible delay and potentially affect
audio throughput; test before leaving auto Spark on during live playback.

## Sources, licensing, quality

The source URL is stored in each TSV row and in
data/JAZZ-ENCYCLOPEDIA-PROVENANCE.md. It is deliberately NOT displayed
on the 240 x 135 LCD: screen space belongs to the biography, fact or quote.
Facts are new short summaries of source material, not purported quotations.
Quotes are included only when an attribution trail was found, with a
person-specific Wikiquote page and bibliographic information where available;
some are excerpts from longer interviews, not newly authored slogans. Their
precise evidential status varies. The Monk "wrong notes" quotation has secondary
attribution rather than a verified first-person recording. See provenance.

The original 1487-entry imported catalog used Wikidata's jazz-musician
occupation Q15981151. The editorial layer enhances 52 of those entries and
adds 11 absent musicians (1498 total). This is a community-maintained
catalog, **not** an assurance that all listed people were major performers.
Descriptions vary in depth; 63 editor-written entries have linked article
references, but they are short summaries, not full-length biographies.
The original 36 album suggestions retain independent MusicBrainz verification;
all other profiles display no album rather than fabricating one.

Data formats, each LF-terminated and ASCII-only:

- Encyclopedia: artist-key TAB artist-name TAB biography TAB album-or-minus TAB URL
- Spark: FACT-or-QUOTE TAB text TAB subject TAB URL

The complete corpus is frozen for each release. Regenerate deliberately via
`python tools/build_jazz_encyclopedia.py` (requires internet), or to retain
the exact artist snapshot and update curated facts/quotes use
`python tools/build_jazz_encyclopedia.py --facts-only` (offline). Curated
source text is in `data/jazz-curated-facts.tsv` and
`data/jazz-editorial-bios.tsv`, preserved in the source snapshot.
Run the host tests before archiving. Sources never consume LCD pixels.

These are host-tested data and screen contracts; playback performance and
microSD timing on the physical Cardputer ADV remain to be accepted.
