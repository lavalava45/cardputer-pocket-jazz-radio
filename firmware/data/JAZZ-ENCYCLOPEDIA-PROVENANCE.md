# Offline jazz encyclopedia provenance

Generated UTC: 2026-11-14T13:24:52.912031+00:00

- 1498 musician entries; the original 1487-row catalog was imported from Wikidata occupation Q15981151.
- 63 original editorial musician summaries; 0 keys were absent from the input corpus in this regeneration.
- 175 FACT/QUOTE records (including 135 original editorial facts and 25 attributed short quotes).
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
- Short quotations (25 in the current corpus), with attributions and cited
  interviews/books/secondary sources in their respective Wikiquote pages.
  These are attribution trails, not independently authenticated recordings;
  no song lyrics were added. Pages additionally consulted:
  https://en.wikiquote.org/wiki/Louis_Armstrong
  https://en.wikiquote.org/wiki/Charles_Mingus
  https://en.wikiquote.org/wiki/Bill_Evans
  https://en.wikiquote.org/wiki/Herbie_Hancock
  https://en.wikiquote.org/wiki/John_Coltrane
  https://en.wikiquote.org/wiki/Billie_Holiday
  https://en.wikiquote.org/wiki/Ornette_Coleman
  https://en.wikiquote.org/wiki/Count_Basie
  https://en.wikiquote.org/wiki/Chick_Corea
  https://en.wikiquote.org/wiki/Wynton_Marsalis
  https://en.wikiquote.org/wiki/Nina_Simone
  https://en.wikiquote.org/wiki/Dave_Brubeck
  https://en.wikiquote.org/wiki/Oscar_Peterson
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
- SHA-256 jazz-facts.tsv: e56a5a6b5555e0db53713765f3b8da37922911866c8371a9870d4dd0e55498c7
