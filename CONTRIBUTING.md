# Contributing stations

Thanks for helping keep the Pocket Jazz default catalog useful.

## New or changed station

Provide the station's official name, primary **HTTPS MP3/ICY** URL, country,
optional verified backup URL, and a short explanation of the change.
No access tokens, user-specific streams, direct MP3 downloads, playlists
that return AAC/HLS, or links that require bypassing TLS verification.
Do not include personal music libraries.

Before proposing a change:

1. Test the station on a desktop with `python scripts/verify_streams.py`.
   The checker requests ICY metadata, removes it from the bounded sample,
   verifies HTTPS, and looks for three aligned MP3 frames. It does not
   guarantee that the stream will work continuously on an ESP32.
2. Keep the five curated M3U filenames stable. Preserve `#EXTINF`,
   `#PJ-ALT` and optional `#PJ-MOOD` syntax. Primary and backup URLs must
   be unique.
3. The current firmware limit is **64 stations total**, including the
   listener's personal stations. Do not silently consume the last slots.
4. The maintainer updates the dated PC report, version and manifest for
   each published release and runs offline validation/tests.

Changes are reviewed for format, clarity, licensing and scope.
**An unreachable station is a report, not evidence of software failure.**
Please distinguish PC network verification from actual Cardputer playback.
