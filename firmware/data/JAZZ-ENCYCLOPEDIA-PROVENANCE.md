# Offline jazz encyclopedia provenance

Generated UTC: 2026-11-10T12:03:22.977139+00:00

- 1498 musician entries; original 1487-row Wikidata snapshot uses jazz musician occupation Q15981151.
- 63 original editorial musician summaries; 11 additional entries absent from snapshot.
- 155 FACT/QUOTE records (including 135 original editorial facts and 5 attributed short quotes).
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

- SHA-256 jazz-encyclopedia.tsv: 0a288ccbdff85b32c100137ff53f22b0aacfbe52fd15bb0165bdf4f5a6df2dc6
- SHA-256 jazz-facts.tsv: 3c1d060895d1717f7b2b615a1974f15971ed1717376e89629fb748b7f3e6c827
