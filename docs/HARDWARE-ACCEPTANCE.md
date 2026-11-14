# Hardware acceptance — next Pocket Jazz preview

Software gates cannot verify physical audio. Use only the new app-only BIN
through an existing M5Launcher 3. Keep the actually tested v0.7z13 app-only BIN for rollback. Never
overwrite bootloader, partitions, NVS or personal SD files.

## 1. Keyboard and LCD

- On Radio, Help, Clock, Diagnostics, Browse, Settings, Jazz Notes, FX
  and Offline MP3: minus and equals must adjust volume by one step;
  global volume bounds remain 0..185. No stream reconnect or screen jump.
- Repeat with a dimmed screen; the first volume press must work.
- In Wi-Fi password, station search, and Jazz Notes search, minus/equals
  must type literal characters; Fn combinations should adjust volume
  without inserting punctuation.
- A sounding alarm changes only its temporary target (range 0..80).
- Inspect long UTF-8 station/artist/title strings, especially Cyrillic
  and accented text, then change to shorter metadata. Check no ghosting,
  permanent compact VU, volume percentage, clear TZ display and Help.

## 2. Real audio stability

- Test a previously failing HTTPS station and a previously audible station.
  A displayed ON AIR is not proof of accepted speaker PCM.
- Listen for 10–20 minutes to a 128-kbit/s stream, noting the first actual
  gap, displayed BUFFERING and underrun count.
- Test STOP, reconnect, station switch, deliberate disconnection and
  recovery without reboot. Note first failed HTTPS/MP3 stage.
- After first gap, STOP playback, open Diagnostics memory page 7/8 and
  press M. Copy heap-lifecycle.tsv and available audio-runtime.tsv.
  Compare PRE_RING, PRE_OPEN, OPEN_RESULT, PREFILL_DONE, CODEC_RESULT,
  FIRST_PCM, FIRST_STALL, LOW_FREE/LOW_BLOCK, BEFORE_FREE/AFTER_FREE.

## 3. Manual catalog updater (if enabled)

- First back up the SD card's PocketJazz directory.
- Stop playback and disable any offline source. Press D from radio,
  inspect confirmation screen, then Enter. Never automatically update
  from startup or while listening.
- Verify only five default M3U files may change, and 04-my-stations.m3u,
  arbitrary user M3U/PLS, offline files, notes and backups remain identical.
- Test no Wi-Fi, no correct time, hash failure, full SD, and interruption
  during staging/rename if safely reproducible. Recovery must not present
  a mixed catalog as successful.

## 4. Regressions

Recheck two saved Wi-Fi profiles, presets, Backup confirmations, local MP3,
Jazz Notes and Spark, alarm and sleep timer, volume-only commands, FX
OFF by default, TLS certificate/hostname verification and manual reports.

**Acceptance:** only real-device listening and file checks may establish
hardware PASS. Host tests and emulator boot remain separate evidence.
