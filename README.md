<div align="center">

<img src="docs/images/emulator-v07z13-cover.png" alt="Pocket Jazz Radio — real v0.7z13 emulator screens in an editorial cover" width="850">

# Pocket Jazz Radio

### A little radio. A wider world of jazz.

**Turn your M5Stack Cardputer ADV into a tiny, personal jazz companion.**

Listen. Discover. Remember. No streaming account, phone app or subscription required.

[**Get the preview**](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z14) · [**Explore all features**](docs/FEATURE-TOUR.md) · [**Install safely**](docs/INSTALL.md) · [**User guide**](docs/USER-GUIDE.md)

![Device](https://img.shields.io/badge/for-Cardputer%20ADV-243b4a)
![Stations](https://img.shields.io/badge/51%20curated-stations-c8a66d)
![Catalog](https://img.shields.io/badge/catalog-2026.11.14.1-63998e)
[![Checks](https://github.com/lavalava45/cardputer-pocket-jazz-radio/actions/workflows/validate.yml/badge.svg)](https://github.com/lavalava45/cardputer-pocket-jazz-radio/actions/workflows/validate.yml)

</div>

---

## A radio made for listening, not scrolling

Pocket Jazz feels like having a small jazz station on your desk. Pick a favourite with one key, explore a new sound, look up the musician you are hearing, or keep a track in your listening diary. The tiny screen stays readable, and nearly everything is a keypress away.

It is an **independent, open-source project** designed specifically for the Cardputer ADV. Radio uses freely accessible broadcaster streams over **2.4 GHz Wi-Fi**; extra reading and offline listening features can be kept on a microSD card.

| **Listen** | **Explore** | **Make it yours** |
| :--- | :--- | :--- |
| 51 hand-picked stations in five jazz collections | Jazz Notes artist biographies and Jazz Spark facts | Nine instant station presets |
| Direct station search and browsing | Mood Tuner and manual Tap Tempo | Your own stations, likes and offline MP3s |
| Track titles when broadcasters provide them | Recent tracks and QR lookups | Clock, sleep timer and daily radio alarm |

## Real screens, from real firmware in an emulator

The frames below come directly from the **240 × 135 LCD of the v0.7z13 firmware running in Velxio**. The images are deliberately kept **compact (360 px wide)** instead of being stretched across the page. They are not drawings or photographs of the physical Cardputer. The emulator had no personal SD library or Wi-Fi connection, so some screens show offline or empty states.

<table>
<tr>
<td align="center" width="50%"><img src="docs/images/emulator-v07z13-browse.png" width="360" height="203" alt="Pocket Jazz station browser captured from the v0.7z13 emulator"><br><b>Station browser</b><br>Discover another station without interrupting the one you are listening to until you press Enter.</td>
<td align="center" width="50%"><img src="docs/images/emulator-v07z13-qr.png" width="360" height="203" alt="Pocket Jazz station QR display captured from the v0.7z13 emulator"><br><b>Share by QR</b><br>Scan a public station link with your phone, or open a song search after saving a like.</td>
</tr>
<tr>
<td align="center"><img src="docs/images/emulator-v07z13-clock.png" width="360" height="203" alt="Blue Note clock captured from the v0.7z13 emulator before time synchronization"><br><b>Blue Note clock</b><br>A quieter screen for time, with sleep and alarm options close at hand.</td>
<td align="center"><img src="docs/images/emulator-v07z13-help.png" width="360" height="203" alt="Pocket Jazz on-device help captured from the v0.7z13 emulator"><br><b>Built-in help</b><br>18 pages of controls available right on the keyboard.</td>
</tr>
<tr>
<td align="center"><img src="docs/images/emulator-v07z13-mood.png" width="360" height="203" alt="Mood Tuner captured from the v0.7z13 emulator without the optional SD mood tags"><br><b>Mood Tuner</b><br>Move between gentle and energetic, acoustic and electric, and pick a matching tagged station.</td>
<td align="center"><img src="docs/images/emulator-v07z13-fx.png" width="360" height="203" alt="Optional tuning sound effects captured from the v0.7z13 emulator"><br><b>Tuning ambience</b><br>Optional vinyl crackle and soft hiss while a station starts — off by default.</td>
</tr>
</table>

[**See more actual emulator screens and the full guided tour →**](docs/FEATURE-TOUR.md)

## What you can do

### 01. Travel through jazz

Explore **five curated collections**: Essentials, Modern Jazz, Mellow & Bossa, Swing & Vocals, and Latin & Soul Jazz. Move between stations, search by name or browse freely. Save nine favourite stations and recall them without a menu.

The editorial catalog is updated separately from the firmware. You can request a **verified, manual refresh directly from GitHub** when playback is stopped. Want something different? Add your own compatible radio stations on microSD.

### 02. Remember the music

When a broadcaster supplies a song title, Pocket Jazz shows it on the radio screen. Keep a short history of recent track announcements, press **K** to save a discovery in your own listening diary, or bring up a QR code to search for it using Spotify or Google.

The QR code **starts a web search**; it does not automatically recognise a recording, control Spotify or edit your playlists.

### 03. Discover the people behind the records

A surprisingly deep offline listening companion fits inside this little machine. Search an **artist directory of approximately 1,498 entries**, with expanded editorial notes for 63 musicians. Read **150 jazz facts and five attributed quotations** in Jazz Spark. Try the **Mood Tuner** for a different listening direction, or tap out a rhythm to estimate its BPM.

The reading library is available when the accompanying microSD data files are installed. The mood descriptions are editorial, not automatic AI genre detection.

### 04. Make an everyday ritual

The **Blue Note clock**, **30/45/60-minute sleep timer**, gentle **daily radio alarm**, automatic screen dimming and always-accessible volume keys make Pocket Jazz useful beyond a quick demonstration. The alarm needs the Cardputer **left powered on**; it cannot turn on a switched-off device.

### 05. Keep control of your own collection

Bring up to **24 local MP3 files** from a microSD folder, remember up to **two Wi-Fi profiles**, maintain your own stations, and export or restore selected settings manually. Personal playlists and likes belong to you; the official online catalog updater deliberately does not replace them.

[**The complete feature tour — listening, library, favourites, clock, controls, offline music and more →**](docs/FEATURE-TOUR.md)

## Getting started

**Recommended: keep M5Launcher 3 and install an app-only BIN.**

1. Download the [**public v0.7z14 diagnostic preview**](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z14). Select the **app-only** BIN, not an 8 MB full-flash image.
2. Copy it onto your microSD card and open it from **M5Launcher 3 → SD**. Do not replace the launcher, bootloader or partitions.
3. Press **W** to connect to your **2.4 GHz Wi-Fi**.
4. For the full Notes, Spark and curated playlist experience, copy the optional files to **/PocketJazz/** after making a backup. **Never overwrite your personal 04-my-stations.m3u.**
5. Once connected, stop playback, then select **D → Enter** to request the current verified editorial catalog from this GitHub repository. This operation does **not** install a new firmware version.

[**Step-by-step installation and SD safety guide →**](docs/INSTALL.md)

### Your first shortcuts

| Key | What it does |
| :--- | :--- |
| **← / → · Enter · P** | Tune stations, play/stop, switch collections |
| **F · Tab · G** | Browse stations, search by name, choose by mood |
| **1–9 · Fn+1–9** | Recall or save a favourite station |
| **Y · K · Q** | Recent track titles, save a like, open QR choices |
| **U · N · Z · J** | Search Jazz Notes, current artist, Jazz Spark, Tap Tempo |
| **C · T · Fn+T** | Blue Note clock, sleep timer, daily alarm |
| **0 · W · I · H** | Offline MP3s, Wi-Fi, diagnostics, on-device Help |
| **− / =** | Volume down/up on most screens; **Fn** in text-entry fields |

[**Every shortcut and screen explained →**](docs/USER-GUIDE.md#keyboard-reference)

## An evolving preview

**The newest public diagnostic preview is v0.7z14.** It retains the v0.7z13 keyboard fix, uses **1,680 fewer bytes of static RAM** on the no-PSRAM ESP32-S3 build, and can record source-byte progress before an MP3 input stall. **The gallery is still from an actual v0.7z13 emulator run, not the newly released firmware.** On a physical Cardputer, v0.7z13 restored keyboard responsiveness and sounded more reliable, but at least one **BUFFERING interruption** remained. **v0.7z14 has not undergone physical speaker acceptance** and is not advertised as a cure for that interruption. Keep your v0.7z13 BIN for rollback. See the [v0.7z14 release notes](docs/RELEASE-v0.7z14.md).

The emulator provides evidence of **display output and navigation**, not of physical Wi-Fi, certificate handling, SD-card timing, or speaker performance. Read [image provenance](docs/EMULATOR.md), follow the [audio reliability issue](https://github.com/lavalava45/cardputer-pocket-jazz-radio/issues/1), or inspect the [changelog](CHANGELOG.md).

For makers: contributions, verified stream suggestions and reproducible fixes are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) and [development notes](docs/DEVELOPMENT.md). Firmware licensing is GPL-3.0-or-later with bundled notices; catalog tools and metadata are MIT. This independent project is not affiliated with M5Stack, Spotify or any broadcaster.

<div align="center"><strong>Small radio. Big listening.</strong></div>
