# Installing Pocket Jazz Radio

> **Hardware:** M5Stack Cardputer ADV (ESP32-S3). This is a preview release.
> For the ordinary installation, **keep M5Launcher 3** and use the app-only BIN.

In a hurry? Use the [three-step quick start](QUICK-START.md). Curious about the project? Read [Why Pocket Jazz?](WHY-POCKET-JAZZ.md).

## Choose your installation method

| Method | What you install | M5Launcher preserved? | Recommended? |
| --- | --- | --- | --- |
| **M5Launcher 3 → SD** | `PocketJazzRadio-v0.7z16-app-only.bin` | **Yes** | **Yes, preview** |
| **M5Burner / direct USB full flash** | Older v0.7z10 8-MiB merged image | **No**; also replaces partitions and other flash content | Only for an intentionally standalone device |

These are **different firmware versions**. v0.7z16 is distributed
**app-only**; the old optional full-flash image is v0.7z10. Full-flash
is *not* an upgrade for a device whose Launcher and data you want to keep.

## Recommended: M5Launcher 3

1. Open [v0.7z16 diagnostic preview on GitHub Releases](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z16).
2. Download **`PocketJazzRadio-v0.7z16-app-only.bin`**. Do not choose an
   8-MiB merged image.
3. Power down the Cardputer. Copy the BIN to a microSD card using a card
   reader, or your existing M5Launcher file-transfer method.
4. Start your already installed **M5Launcher 3**, open **SD**, select the
   Pocket Jazz BIN and follow the standard *app install/run* workflow.
5. If an installer asks to replace M5Launcher, overwrite a bootloader, or
   repartition the device, **cancel**. There is no need to change those.

### First-run Wi-Fi

Use **W** to open Wi-Fi setup, select a nearby 2.4 GHz access point with
**Up/Down** and press **Enter**. Enter the password if asked; the firmware
can remember two profiles. On later visits to the same network,
**Enter** reconnects using its saved credentials, while **Fn+Enter**
opens password editing. Do not publish firmware binaries containing
personal SSIDs or credentials.

The Cardputer's ESP32-S3 Wi-Fi is **2.4 GHz**, not 5 GHz. Network playback
also requires that the selected station's stream is still reachable.

### Optional SD assets and playlists

The release also has **`PocketJazzRadio-v0.7z16-Launcher3-SD.zip`**.
You do not need to copy every file in it to run the application.
For the full experience, put the appropriate assets under the card's
`/PocketJazz/` folder:

```text
/PocketJazz/
  01-essentials.m3u
  02-modern-jazz.m3u
  03-mellow-bossa.m3u
  04-my-stations.m3u       <- PERSONAL; NEVER REPLACE
  05-swing-vocals.m3u
  06-latin-soul-jazz.m3u
  artist-notes.tsv
  artist-notes.idx
  jazz-encyclopedia.tsv
  jazz-facts.tsv
  offline/                 <- Your own local MP3s (optional)
```

**Back up the entire existing `/PocketJazz/` directory first.** The ZIP
contains a **blank example** of `04-my-stations.m3u`; copying it over
your personal file destroys your saved entries. Never bulk-replace the
folder. Use **R** on the radio to reload edited station playlists.

### Post-STOP diagnostic export (v0.7z16)

When a station buffers, remember its name, then stop the radio with Enter.
Press **I** to open Diagnostics and **R** on any page to save a uniquely
named `/PocketJazz/incident-*.tsv` after STOP. It includes station/ICY,
Wi-Fi/HTTPS/MP3/PCM/heap and the last 16 audio events. A full report ends
with `END complete=1`. Nothing is written automatically while music
plays. There is no password, SSID, IP or stream URL in this report; however,
station names and titles may be sensitive when they come from personal
playlists. Physically verify this SD function before relying on it.

### Optional verified catalog refresh (v0.7z16)

Back up your SD card first. After Wi-Fi and NTP have connected,
**STOP online and offline playback**, press **D** from the radio home
screen, then **Enter** to confirm. This manually downloads exactly five
official playlists from **the same GitHub repository**, verifies HTTPS
certificates, SHA-256, lengths and station limits, and stages replacements
with rollback. The new lists are committed only after the application
successfully re-reads them from SD. Failure restores the old files where
possible or leaves a recovery journal for the next card mount.

The updater is **never automatic**, never updates firmware and never
targets personal M3U/PLS, notes, offline MP3 or backups. FAT SD
power-loss behavior still needs physical-device testing.

When supplying the offline Notes dataset, copy
`artist-notes.tsv` and `artist-notes.idx` **together** so their index
matches the text. `jazz-encyclopedia.tsv` supports offline search;
`jazz-facts.tsv` supplies Jazz Spark facts.

### Time and timezone

**C** opens the Blue Note clock. **Up/Down** adjusts the manual UTC offset
by 30 minutes; **0** clears the manual override; **Del** returns.
The offset is saved in NVS and used after NTP synchronization.
A manual fixed offset does **not** automatically switch for daylight
saving time. For a POSIX timezone rule (including DST), copy the
example `clock.example.txt` to `/PocketJazz/clock.txt`, edit its
`TZ=` line and reboot. A manually specified offset takes precedence
until reset.

## Alternative: standalone M5Burner installation

M5Stack's [M5Burner publishing instructions](https://docs.m5stack.com/en/uiflow/m5burner/publish)
describe sharing full firmware images. The matching optional asset is
named **`PocketJazzRadio-v0.7z10-M5Burner-FULL-FLASH-REPLACES-LAUNCHER.bin`**.
It is specifically **not** a v0.7z12 image.

**Warning:** this image is an 8-MiB flash layout including the bootloader,
partition table, and app. **Installing it replaces M5Launcher and can
erase NVS, Wi-Fi profiles, prior applications and other flash-resident
data.** It is intended only for a device deliberately being converted
to run Pocket Jazz as standalone firmware. It is **not** needed for the
recommended launcher workflow and has **not been flashed on a physical
Cardputer as part of this release**.

To use the M5Burner publishing workflow, open **USER CUSTOM → Publish**,
select the correct **Cardputer ADV** device type and supply the matching
full-flash image and this GitHub URL. **Publishing the binary into
M5Burner's public directory is a separate action**; adding an asset to
GitHub Releases does not automatically publish it to M5Burner. M5Stack
may require a cover image and its own account.

Read the [official M5Burner documentation](https://docs.m5stack.com/en/uiflow/m5burner/publish)
before doing a standalone install. Keep a backup of important files
and use the M5Burner version appropriate for your board.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| No sound after connection | Is playback ON? Is MUTE enabled? Is the station actually MP3/ICY? |
| Repeated CONNECTING / BUFFERING | Visit **I → Diagnostics**, inspect network, ring and TLS metrics. A strong RSSI does not rule out server-side stalls. |
| HTTPS never connects | Correct NTP time, reachable server, a valid certificate/hostname, and the station URL. HTTPS verification is intentionally enabled. |
| A personal station fails | The firmware does not guarantee AAC, HLS, DASH or every custom URL. Default stations are PC-checked, not guaranteed forever. |
| Clock shows incorrect local time | Confirm NTP sync and your saved offset / `clock.txt` timezone rule. |
| SD data missing | Check the exact `/PocketJazz/` filenames and paired Notes index. |
| Want to recover original app | Use M5Launcher to run the prior known-working app-only BIN, if Launcher was preserved. |

See [Diagnostics in the user guide](USER-GUIDE.md#diagnostics-and-reports)
before reporting audio bugs. Please attach the version, exact station,
symptom, and the **first** error or underrun if possible.
