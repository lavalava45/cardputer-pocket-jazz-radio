# Pocket Jazz Radio artist cards — official MusicBrainz provenance

Verification timestamp (UTC): 2026-11-10T04:07:54+00:00

- 36 artist/release-group pairs selected by a Pocket Jazz editor; original three retained.
- All 36 verified by the existing MusicBrainz API builder in its generation run (36/36; no rejected rows).
- The prior verify-only run accepted 35/36 with one transient URLError; no output was published from that run.
- Mandatory checks: artist MBID and exact name, release-group MBID and exact title, primary type Album and named artist in artist-credit.
- Only official MusicBrainz core fields supply the note; editorial album selection does not imply a MusicBrainz recommendation.
- Export is ASCII-only, seven TSV fields, with artist-notes.idx SHA-256 of TSV bytes and exact offsets.
- Official core data licence: CC0 1.0; no Wikipedia descriptions, MusicBrainz tags, user ratings or annotations were copied.
- One identified API request per >=1.2 seconds; no private tokens, scraping or fabricated biographies.

## Curated source pairs

| Artist (MusicBrainz canonical) | Editorial album (MusicBrainz release-group title) | Artist MBID | Release-group MBID |
| --- | --- | --- | --- |
| Miles Davis | Kind of Blue | [561d854a-6a28-4aa7-8c99-323e6ce46c2a](https://musicbrainz.org/artist/561d854a-6a28-4aa7-8c99-323e6ce46c2a) | [8e8a594f-2175-38c7-a871-abb68ec363e7](https://musicbrainz.org/release-group/8e8a594f-2175-38c7-a871-abb68ec363e7) |
| John Coltrane | Blue Train | [b625448e-bf4a-41c3-a421-72ad46cdb831](https://musicbrainz.org/artist/b625448e-bf4a-41c3-a421-72ad46cdb831) | [c63baaaa-10c0-4b86-ad7e-1f39d7e2eccc](https://musicbrainz.org/release-group/c63baaaa-10c0-4b86-ad7e-1f39d7e2eccc) |
| Bill Evans | Waltz for Debby | [8247a3f2-3a8e-4256-b322-6c57b03a4e36](https://musicbrainz.org/artist/8247a3f2-3a8e-4256-b322-6c57b03a4e36) | [3706fa92-d486-414f-892a-7b21d0f6037f](https://musicbrainz.org/release-group/3706fa92-d486-414f-892a-7b21d0f6037f) |
| Duke Ellington and His Orchestra | Ellington at Newport | [c01557ad-d41e-4316-a285-605769e2ab3b](https://musicbrainz.org/artist/c01557ad-d41e-4316-a285-605769e2ab3b) | [c7af2ab6-81b0-3f92-8253-9417d00f1361](https://musicbrainz.org/release-group/c7af2ab6-81b0-3f92-8253-9417d00f1361) |
| Dave Brubeck | Time Out | [de0222a6-e1c4-403d-8b01-3f66d505061b](https://musicbrainz.org/artist/de0222a6-e1c4-403d-8b01-3f66d505061b) | [59c053ab-7f39-4be0-9198-df7e3aaa6e78](https://musicbrainz.org/release-group/59c053ab-7f39-4be0-9198-df7e3aaa6e78) |
| Thelonious Monk | Brilliant Corners | [8e8c7417-c905-46b1-b42a-5260b4274ed4](https://musicbrainz.org/artist/8e8c7417-c905-46b1-b42a-5260b4274ed4) | [764d0d9e-68b1-39fe-b1d1-eda104926777](https://musicbrainz.org/release-group/764d0d9e-68b1-39fe-b1d1-eda104926777) |
| Charles Mingus | Mingus Ah Um | [f3b8e107-abe8-4743-b6a3-4a4ee995e71f](https://musicbrainz.org/artist/f3b8e107-abe8-4743-b6a3-4a4ee995e71f) | [48ec720b-2fc2-35dd-8130-5b186c4abcbc](https://musicbrainz.org/release-group/48ec720b-2fc2-35dd-8130-5b186c4abcbc) |
| Art Blakey | Moanin' | [601e7466-eaf5-4a91-9909-ffd770b7e04a](https://musicbrainz.org/artist/601e7466-eaf5-4a91-9909-ffd770b7e04a) | [abb77cc8-5b99-4bb9-9667-6e9c80caf346](https://musicbrainz.org/release-group/abb77cc8-5b99-4bb9-9667-6e9c80caf346) |
| Sonny Rollins | Saxophone Colossus | [3b47247e-5b57-49b6-a0ed-bad80243802a](https://musicbrainz.org/artist/3b47247e-5b57-49b6-a0ed-bad80243802a) | [cf29d6a2-a18a-3bb6-a861-495148baee1f](https://musicbrainz.org/release-group/cf29d6a2-a18a-3bb6-a861-495148baee1f) |
| Herbie Hancock | Maiden Voyage | [27613b78-1b9d-4ec3-9db5-fa0743465fdd](https://musicbrainz.org/artist/27613b78-1b9d-4ec3-9db5-fa0743465fdd) | [c5e5e8ad-dc89-319e-8b2d-b3ff5e59fcea](https://musicbrainz.org/release-group/c5e5e8ad-dc89-319e-8b2d-b3ff5e59fcea) |
| Wayne Shorter | Speak No Evil | [2379937f-6e0d-46a2-b8ff-633fafd72002](https://musicbrainz.org/artist/2379937f-6e0d-46a2-b8ff-633fafd72002) | [eae0c18f-f2fd-3b97-8631-3f682a2f3957](https://musicbrainz.org/release-group/eae0c18f-f2fd-3b97-8631-3f682a2f3957) |
| The Horace Silver Quintet | Song for My Father | [e9ac5139-2da0-4f00-9ac2-c713e3813ff4](https://musicbrainz.org/artist/e9ac5139-2da0-4f00-9ac2-c713e3813ff4) | [5fd8134c-ff96-3f48-a0a7-952a1e00e78e](https://musicbrainz.org/release-group/5fd8134c-ff96-3f48-a0a7-952a1e00e78e) |
| Chet Baker | Chet Baker Sings | [1ba1d493-7114-45e2-b163-a36d49a0c065](https://musicbrainz.org/artist/1ba1d493-7114-45e2-b163-a36d49a0c065) | [a0aabd83-2341-335d-a0d6-e0ea609b0b3f](https://musicbrainz.org/release-group/a0aabd83-2341-335d-a0d6-e0ea609b0b3f) |
| The Oscar Peterson Trio | Night Train | [b083ec9b-27e0-4fda-801d-cb40861a0578](https://musicbrainz.org/artist/b083ec9b-27e0-4fda-801d-cb40861a0578) | [9ce4b9ee-9ad4-3deb-95a4-4cb4e68c145e](https://musicbrainz.org/release-group/9ce4b9ee-9ad4-3deb-95a4-4cb4e68c145e) |
| Ella Fitzgerald | Ella Fitzgerald Sings the Cole Porter Song Book | [54799c0e-eb45-4eea-996d-c4d71a63c499](https://musicbrainz.org/artist/54799c0e-eb45-4eea-996d-c4d71a63c499) | [dcc0d723-e27f-36e8-8438-dbbde7eb9a67](https://musicbrainz.org/release-group/dcc0d723-e27f-36e8-8438-dbbde7eb9a67) |
| Nina Simone | Little Girl Blue | [2944824d-4c26-476f-a981-be849081942f](https://musicbrainz.org/artist/2944824d-4c26-476f-a981-be849081942f) | [eb5f5415-fcd1-4ca0-b0be-4080cb315a98](https://musicbrainz.org/release-group/eb5f5415-fcd1-4ca0-b0be-4080cb315a98) |
| Louis Armstrong | Louis Armstrong Plays W.C. Handy | [eea8a864-fcda-4602-9569-38ab446decd6](https://musicbrainz.org/artist/eea8a864-fcda-4602-9569-38ab446decd6) | [3c6e5005-cad2-4444-825f-71e4b8324d51](https://musicbrainz.org/release-group/3c6e5005-cad2-4444-825f-71e4b8324d51) |
| Count Basie | The Atomic Mr. Basie | [0dbd6300-efdc-420b-857e-895e18fad317](https://musicbrainz.org/artist/0dbd6300-efdc-420b-857e-895e18fad317) | [1b464368-017c-3aba-8a2d-437bcf43b5d0](https://musicbrainz.org/release-group/1b464368-017c-3aba-8a2d-437bcf43b5d0) |
| Bud Powell | The Amazing Bud Powell | [dbc5809c-7837-4b6f-961e-340d64fbb41c](https://musicbrainz.org/artist/dbc5809c-7837-4b6f-961e-340d64fbb41c) | [34887975-3564-3094-831f-2476fe3b64e9](https://musicbrainz.org/release-group/34887975-3564-3094-831f-2476fe3b64e9) |
| Eric Dolphy | Out to Lunch! | [badda5cf-f2c5-4dc2-b3d3-07467bdf0f71](https://musicbrainz.org/artist/badda5cf-f2c5-4dc2-b3d3-07467bdf0f71) | [bc6c2573-bc7d-346a-8880-0e34b8fb7e50](https://musicbrainz.org/release-group/bc6c2573-bc7d-346a-8880-0e34b8fb7e50) |
| Cannonball Adderley | Somethin' Else | [a4c73ebe-b2c7-4f13-b99d-2fe1f9f27da8](https://musicbrainz.org/artist/a4c73ebe-b2c7-4f13-b99d-2fe1f9f27da8) | [07eb42bc-78e5-4ee7-b7ec-62904a66ca8a](https://musicbrainz.org/release-group/07eb42bc-78e5-4ee7-b7ec-62904a66ca8a) |
| McCoy Tyner | The Real McCoy | [22fe7b6f-af38-458e-87bd-8971e7a2912e](https://musicbrainz.org/artist/22fe7b6f-af38-458e-87bd-8971e7a2912e) | [637892d3-1941-3905-9795-76564a7e4413](https://musicbrainz.org/release-group/637892d3-1941-3905-9795-76564a7e4413) |
| Lee Morgan | The Sidewinder | [a1235272-3650-4ed7-9317-5a55a08701ec](https://musicbrainz.org/artist/a1235272-3650-4ed7-9317-5a55a08701ec) | [24d2fedc-7976-3b88-bb91-b49a05f26e55](https://musicbrainz.org/release-group/24d2fedc-7976-3b88-bb91-b49a05f26e55) |
| Gerry Mulligan | Night Lights | [68f19808-1f7b-46d4-bd1d-71aaf0f6b267](https://musicbrainz.org/artist/68f19808-1f7b-46d4-bd1d-71aaf0f6b267) | [5925a825-8e0e-3f35-a238-8f77db7229dd](https://musicbrainz.org/release-group/5925a825-8e0e-3f35-a238-8f77db7229dd) |
| Ornette Coleman | The Shape of Jazz to Come | [169c0d1b-fcb8-4a43-9097-829aa7b39205](https://musicbrainz.org/artist/169c0d1b-fcb8-4a43-9097-829aa7b39205) | [5879cfca-e1d0-3ef4-8014-3b284832ed18](https://musicbrainz.org/release-group/5879cfca-e1d0-3ef4-8014-3b284832ed18) |
| Chick Corea | Now He Sings, Now He Sobs | [8446fcca-109e-4c6d-a2ff-5a269b32b4c2](https://musicbrainz.org/artist/8446fcca-109e-4c6d-a2ff-5a269b32b4c2) | [3f37ff43-48f3-3032-ad5f-d7ed57841cf7](https://musicbrainz.org/release-group/3f37ff43-48f3-3032-ad5f-d7ed57841cf7) |
| Pat Metheny | Bright Size Life | [7daac7f9-8fcc-485f-a14f-f8091d98cf25](https://musicbrainz.org/artist/7daac7f9-8fcc-485f-a14f-f8091d98cf25) | [abac8d3d-f25a-386f-a819-5063c8df56a4](https://musicbrainz.org/release-group/abac8d3d-f25a-386f-a819-5063c8df56a4) |
| Billie Holiday | Lady in Satin | [d59c4cda-11d9-48db-8bfe-b557ee602aed](https://musicbrainz.org/artist/d59c4cda-11d9-48db-8bfe-b557ee602aed) | [74fdc36c-ffa6-44a2-942a-cab1b47987e1](https://musicbrainz.org/release-group/74fdc36c-ffa6-44a2-942a-cab1b47987e1) |
| Benny Goodman | The Famous 1938 Carnegie Hall Jazz Concert | [b09ae88f-4156-4caa-b129-1cacb5e1632e](https://musicbrainz.org/artist/b09ae88f-4156-4caa-b129-1cacb5e1632e) | [d986aa1d-42d2-308a-a54b-d0339339ae01](https://musicbrainz.org/release-group/d986aa1d-42d2-308a-a54b-d0339339ae01) |
| Freddie Hubbard | Red Clay | [59ae7a1a-f454-435b-8a5a-e327e692bb5a](https://musicbrainz.org/artist/59ae7a1a-f454-435b-8a5a-e327e692bb5a) | [2c39394e-b9e4-352b-b339-8e61df67479e](https://musicbrainz.org/release-group/2c39394e-b9e4-352b-b339-8e61df67479e) |
| Pharoah Sanders | Karma | [b3a0912a-a62a-4388-9368-7cb21ed5caf9](https://musicbrainz.org/artist/b3a0912a-a62a-4388-9368-7cb21ed5caf9) | [22f978cb-26e6-3757-a13a-347970574eec](https://musicbrainz.org/release-group/22f978cb-26e6-3757-a13a-347970574eec) |
| Joe Henderson | Page One | [bcab8301-c7e5-4689-a4ad-9ee947b4ce37](https://musicbrainz.org/artist/bcab8301-c7e5-4689-a4ad-9ee947b4ce37) | [b8387a4b-fd1a-3b68-bdba-95ec30962428](https://musicbrainz.org/release-group/b8387a4b-fd1a-3b68-bdba-95ec30962428) |
| Wes Montgomery | The Incredible Jazz Guitar of Wes Montgomery | [663f8232-8c46-4851-803f-a91d31593b14](https://musicbrainz.org/artist/663f8232-8c46-4851-803f-a91d31593b14) | [92829cab-8dee-3e6b-97ad-22fd49c95ed5](https://musicbrainz.org/release-group/92829cab-8dee-3e6b-97ad-22fd49c95ed5) |
| Dizzy Gillespie | Sonny Side Up | [e9ba8ccb-505f-4e5c-b909-65998d0d35b5](https://musicbrainz.org/artist/e9ba8ccb-505f-4e5c-b909-65998d0d35b5) | [19f3ecb4-eef6-418c-8c01-da2c90ecf7c7](https://musicbrainz.org/release-group/19f3ecb4-eef6-418c-8c01-da2c90ecf7c7) |
| Andrew Hill | Point of Departure | [ede80a9f-f114-4041-b0d9-2584a0b30ec9](https://musicbrainz.org/artist/ede80a9f-f114-4041-b0d9-2584a0b30ec9) | [d7931540-f35c-3978-9bd8-6e77ab66334d](https://musicbrainz.org/release-group/d7931540-f35c-3978-9bd8-6e77ab66334d) |
| Bobby Timmons | This Here Is Bobby Timmons | [ef05197e-aacb-4dbf-9cc4-2a9abee82f03](https://musicbrainz.org/artist/ef05197e-aacb-4dbf-9cc4-2a9abee82f03) | [0749d0ec-687d-39c0-908a-bb3810777917](https://musicbrainz.org/release-group/0749d0ec-687d-39c0-908a-bb3810777917) |

Scope: IDs and title-credit matching were checked via MusicBrainz API. No independent biographical fact-check or physical Cardputer display test was performed.
