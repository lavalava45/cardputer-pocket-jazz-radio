# Why Pocket Jazz?

### Not just another internet radio. A pocket-sized journey through jazz.

*A little radio. A wider world of jazz.*

There is a different feeling to turning on a radio.

You don't have to decide which album to play, build a queue, or reach for a phone. You choose a station, listen, and let the next tune surprise you. Sometimes it's a familiar recording. Sometimes it's an artist you have never heard of. And sometimes the name on the screen makes you curious enough to look further.

**Pocket Jazz Radio is an attempt to give that simple ritual a home on the M5Stack Cardputer ADV.**

<div align="center">

<img src="images/emulator-v07z13-qr.png" width="360" height="203" alt="Pocket Jazz Radio displaying a station's QR code in the v0.7z13 emulator">

<sub>Actual v0.7z13 emulator screen. The owner later confirmed Cardputer ADV hardware testing; the exact v0.8.0 long-play acceptance scenarios have not been documented.</sub>

</div>

## More than a stream list

There are already general-purpose internet radios for small devices. Pocket Jazz makes a different trade-off: rather than trying to play every kind of audio, it gives **jazz and the experience of discovering it** a little more attention.

The starting point is **51 hand-picked radio stations** across five collections — from essentials and modern jazz to bossa, swing, vocals and Latin/soul-inflected sounds. These are real broadcasters, with real schedules that can change. The station directory can be refreshed separately from the firmware, and listeners can keep their own stations alongside it.

But the radio is also a companion for *what happens after an unfamiliar track catches your ear*:

- **Jazz Notes** provides an offline artist directory, with extended editorial notes for selected musicians. It is something to read on the device, not an online AI service.
- **Jazz Spark** offers short musical facts. **Mood Tuner** lets you choose a direction — gentle or energetic, acoustic or electric — using editorial tags.
- When a station supplies artist and track metadata, you can **remember the title**, browse recent announcements, or open a **QR-based web search** later. It does not identify music from sound or control a Spotify account.
- Your own **nine station presets**, local **MP3 files**, **Blue Note clock**, sleep timer and daily radio alarm give the project room to become part of a listening routine.

All of this is designed around the Cardputer's *actual keyboard and tiny display*. Most actions take a single key. No account is required to use the radio; streaming needs Wi-Fi, while the optional reading library lives on microSD.

## The destination: a connected jazz-learning companion

The v0.8.0 preview now links **Jazz Notes**, **Jazz Spark**, and the radio into a small **interactive history-of-jazz course**. Learn to Listen cards invite you to hear musical ideas; Jazz Trails follow historical connections; a personal Jazz Diary remembers discoveries; a local Jazz Passport tracks the styles you have actually explored. When a station supplies unambiguous **Artist - Track** metadata, Now Playing, Explained aims to connect the current artist to a verified offline note.

**Included in the v0.8.0 software preview:** 24 Jazz Journey lessons, 30 listening tips, local diary/passport screens and 595 exact Now Playing/Explained hints, with 150 provenance-backed Spark facts. The owner has confirmed a Cardputer ADV test, while detailed results for every v0.8.0 screen and prolonged speaker/SD/keyboard operation are not yet recorded. No subscription or audio recognition is planned, and broadcasters do not promise a particular track.

## Why build for such a small machine?

A dedicated little device has useful limits. It sits on a desk, plays music, shows just enough information and stays out of the way. The Cardputer ADV also makes the engineering interesting: a radio has to share scarce RAM between HTTPS, MP3 decoding, the speaker and the user interface.

That constraint is part of the work, not something to disguise. The project is open source, and the **v0.8.0 Jazz Journey preview** retains a measured reduction of **1,672 bytes of static RAM** and extra telemetry for investigating a rare input-buffer stall. On the physical device, an earlier v0.7z13 restored keyboard responsiveness, but a sporadic **BUFFERING** pause remained. The new version is **not yet confirmed to eliminate it**. An earlier v0.7z16 addition was a one-key **post-STOP incident export**: a single SD report records the station, recent audio behavior, network and heap statistics, so troubleshooting no longer depends on photos of numeric screens. It has software validation, not physical SD acceptance.

Pocket Jazz isn't a finished commercial appliance. It is a small, evolving music project that other Cardputer owners can inspect, try and help improve.

## What I'd love other Cardputer owners to try

Do the stations you enjoy remain reachable? Do the keys respond while music plays? Is the display readable without a phone? Does switching stations feel natural — and what happens if a stream slows down?

Those observations are more valuable than a view count. Reports with the firmware version and station name are welcome, especially from independent users. The [open audio issue](https://github.com/lavalava45/cardputer-pocket-jazz-radio/issues/1) explains the current limitation.

**Try it:** [Three-step quick start](QUICK-START.md) · [Download v0.8.0 preview](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.8.0) · [Explore the features](FEATURE-TOUR.md).

---

## Short introduction for a community post

This is an **archived early draft**; the actual [r/CardPuter post](https://www.reddit.com/r/CardPuter/comments/1xwqkah/pocket_jazz_radio_v080_for_cardputer_adv_51/) has already been published. A fuller feature-description draft is available in the project conversation but has **not automatically replaced the existing post**. The owner confirmed physical Cardputer testing; include a real current photo/video only if available.

**Suggested title:** I built a pocket-sized jazz radio for the Cardputer ADV — with 51 stations and an offline artist library

> I've been working on **Pocket Jazz Radio**, an open-source internet radio for the **M5Stack Cardputer ADV**.
>
> The idea is to keep the experience closer to a tiny dedicated radio than a general-purpose streaming app: pick a station, listen, discover a musician and save a title you want to look up later.
>
> It has **51 curated jazz/jazz-adjacent stations**, keyboard shortcuts and nine presets, plus optional offline **Jazz Notes**, **Jazz Spark**, a mood-based station selector, clock and local MP3 playback. You can also maintain your own station list and update the curated catalog without reflashing.
>
> There's a ready-to-run **app-only BIN for M5Launcher 3**, with no need to replace the launcher. It's still a **preview**: Rare short buffering pauses remain a known, non-blocking limitation, and I haven't yet verified the new v0.7z16 interface/audio on physical hardware. The source and current limitations are public.
>
> GitHub / download: https://github.com/lavalava45/cardputer-pocket-jazz-radio
>
> I'd be interested in hearing what other Cardputer ADV owners think — especially whether station switching, responsiveness and audio continuity work well for you.

**Historical draft only:** the v0.7z16-era example above is superseded by the actual v0.8.0 publication. Its untested wording must not be copied as current fact. The owner confirmed real device testing, but exact test scope/duration is not recorded. The screenshots above are authentic **v0.7z13 emulator** images and must remain labelled as such.
