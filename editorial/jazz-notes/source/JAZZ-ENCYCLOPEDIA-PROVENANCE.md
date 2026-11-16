# Offline jazz encyclopedia provenance

Reproducible offline snapshot; no generation-time timestamp.

- 1498 musician entries; the original 1487-row catalog was imported from Wikidata occupation Q15981151.
- 63 original editorial musician summaries, preserved verbatim.
- 1432 non-editorial entries have matching locally archived Wikidata statement snapshots.
- 1159 additional independently cited jazz-contribution capsules override machine descriptions; data/jazz-researched-context.tsv is the source of their exact wording and URLs.
- Newly displayed structured music clauses (non-editorial): recording=139, co_performer=24, performance_venue=3, ensemble=70.
- Available claim counts by field within snapshot-matched entries: P569=1421, P570=791, P19=1393, P27=1431, P1303=1368, P800=41.
- Claim IDs, English labels, and explicit absent properties are preserved in data/jazz-structured-facts.json.
- Properties: P569 birth, P570 death, P19 birthplace, P27 citizenship, P1303 instrument, P800 notable work.
- A Wikidata statement is an auditable database assertion, not independently corroborated primary evidence.
- Birthplaces and citizenship are not evidence of performance geography or collaborator relationships.
- P800 notable works are not necessarily recordings; independent MusicBrainz album picks remain limited to 36.
- Verified direct MusicBrainz performance-recording relations: 1231 artists; member-of-band links: 964 artists; collaboration-project links: 48 artists.
- MusicBrainz relations are attributed community claims, not independently authenticated session evidence.
- Exact identity chain: Wikidata API wbgetentities P434 -> MusicBrainz artist UUID; optional reciprocal MusicBrainz url-rels -> same Wikidata QID.
- Recording proof: MusicBrainz artist inc=recording-rels (instrument/vocal/performer); named co-performers also require recording inc=artist-rels to credit BOTH artists on the SAME recording UUID.
- Venue proof: MusicBrainz artist inc=event-rels (main performer/support act) plus event inc=place-rels (held at); no geography inferred otherwise.
- A recorded title is a deterministic representative of explicit instrument/vocal/performance links, not an assessment of noteworthiness.
- MusicBrainz collaboration-project links do not establish named co-performers; no venues inferred from birthplaces.
- Unavailable fields are omitted, not guessed; this does not establish recording or collaborator coverage.
- data/jazz-encyclopedia.idx v2 pins CRC32 of exact LF ASCII TSV and index rows separately; rebuild both together.
- data/jazz-encyclopedia-seed.tsv is the immutable original corpus, SHA-256: 0a288ccbdff85b32c100137ff53f22b0aacfbe52fd15bb0165bdf4f5a6df2dc6.
- 325 FACT/QUOTE records (including 135 original editorial facts and 25 attributed short quotes).
- Source columns reside in TSV on SD and are not shown on LCD.
- Names and short biographies are from Wikidata CC0 structured descriptions;
  albums for 36 original artists remain independently verified MusicBrainz records.
- HISTORICAL FACTS are original concise summaries, see:
  https://en.wikipedia.org/wiki/Jazz
  https://en.wikipedia.org/wiki/Jazz_Age
  https://en.wikipedia.org/wiki/Jazz_standard
- Individual editorial facts have article references in data/jazz-curated-facts.tsv;
- Curated biographies are in data/jazz-editorial-bios.tsv (one topic URL per card).
  no encyclopedia thumbnail or text claims a deep biography for every name.
- Short quotations, with attributions in their respective Wikiquote pages:
  https://en.wikiquote.org/wiki/Duke_Ellington
  https://en.wikiquote.org/wiki/Thelonious_Monk
  https://en.wikiquote.org/wiki/Miles_Davis
  https://en.wikiquote.org/wiki/Art_Blakey
- Some musician cards are deliberately brief: they are not fabricated narrative biographies.
- Monk's 'wrong notes' quotation is a secondary attribution by radio host Phil Schaap,
  not a surviving transcript of Monk directly saying those words.
- A dash album means no verified recommendation; do not claim otherwise.
- Query:

    SELECT DISTINCT ?item ?name ?description WHERE {
     ?item wdt:P106 wd:Q15981151.
     ?item rdfs:label ?name FILTER(LANG(?name)="en")
     OPTIONAL {?item schema:description ?description FILTER(LANG(?description)="en")}
    } LIMIT 1600

- SHA-256 jazz-encyclopedia.tsv: 9584276dd409c2fb7963ed79dc32453ac4020c1b4ca63b247080cbdd7cf4ff47
- SHA-256 jazz-encyclopedia.idx: 509045e54a2df4f0aaa4ec233dcf5739763f53013b494171bb8d5bf9b32dd82b
- SHA-256 jazz-facts.tsv: 8bc221a6afb784cb6e75a414a7a8cfb384a7d2cdc4f3a184ba947c7c104bdbf5
- SHA-256 jazz-structured-facts.json: 59d2ade3f77038ec816bf667f9e5c82d7c508c6ddc162deb85b5642fd6dc818a
- SHA-256 jazz-music-evidence.json: 9523f5b4581a1a1ff48de97fc71c946b15ee1737b52d807822fe8c9af75a3cc2
- SHA-256 jazz-researched-context.tsv: 662fa0512ec47b614af17ddd268cdec6e832fe338e0f628bd61d67d81281feb4
- SHA-256 jazz-editorial-bios.tsv: ea558fcd3a519a786d4413c4b7b6837e9a627a21012fa763e61c92ddbde1f057
