# Local clock and timezone configuration

**Current v0.7o:** C opens the clock; Up/Down sets fixed UTC offset in ±30-minute steps; T toggles Blue Note/normal styling; 0 restores the optional SD POSIX rule; Del returns. B no longer opens a duplicate clock. Firmware checks the effective `localtime_r` UTC offset and reads back NVS settings; device reboot and real NTP acceptance are pending. [Comprehensive feature map](CURRENT-FEATURES-20261110.md).

**The original v0.7h key map below is archival, not current:**

The radio obtains UTC from NTP after Wi-Fi becomes stable. Its calendar clock has **no independent battery-backed time source verified** for Cardputer ADV: after a cold boot without NTP, Blue Note and Jam Session Clock must show `--:--` or `TIME NOT SET` instead of `1970`.

## Set a UTC offset on the device

Open **C / Jam Session Clock**. **A/D** changes the fixed UTC offset by
minus/plus 30 minutes. **S/W** changes it by minus/plus 60 minutes.
Valid range is UTC-12:00 to UTC+14:00, in 30-minute steps.
For Israel in winter, select UTC+02:00; in summer, UTC+03:00.
Settings are saved in NVS under pocketjazz / tzSet and tzMin and survive
power cycles. The manual offset has precedence over the SD POSIX timezone
file. Press **0 on the clock screen** to clear that preference and return to
the SD rule, or UTC0 if none exists.

Manual offsets do not adjust daylight saving automatically; using a complete
POSIX TZ rule on SD below remains necessary for automatic summer/winter.
Neither an NVS offset nor an SD TZ rule supplies the absolute UTC time when
NTP is unavailable after cold boot.

## Optional microSD configuration

The release ZIP includes `PocketJazz/clock.example.txt`, a harmless example that **does not override user settings**. To select your own timezone:

1. Copy `clock.example.txt` to `clock.txt` inside the SD card's `/PocketJazz/` directory.
2. Edit its single line, `TZ=UTC0`, to a valid **POSIX TZ rule**, e.g. `TZ=CET-1CEST,M3.5.0/2,M10.5.0/3` for a typical Central European DST rule. Review your region's *actual* DST rules; a plausible string may still be wrong.
3. Safely reinsert the SD and **reboot Pocket Jazz Radio**. The current source reads this file only at startup, before clock rendering. The same TZ applies to Blue Note and Jam Session Clock. The sleep timer uses monotonic `millis()` and is unaffected.

Input validation limits the line to 256 file bytes and the POSIX TZ value to at most 79 ASCII characters from a narrow whitelist. Invalid/missing files use **UTC0** rather than an inferred location. This is a defensive size/character filter, **not** a full POSIX grammar validator. The device does not call a geolocation API or export the rule. Only one `TZ=` line is read, no Wi-Fi password or secret belongs in this file. **Use a tested POSIX rule for Israel before relying on automatic daylight-saving changes**; the firmware currently has no automatic IANA timezone database updates.

## Display behavior

- **B**: Blue Note dark screen, now with the local time and abbreviated timezone.
- **C**: Jam Session Clock with original geometric piano-key motif, current local calendar time when valid, or `TIME NOT SET`.
- **S**: Network/Buffer screen: actual Wi-Fi RSSI in dBm, decoder's compressed audio-buffer level in bytes, and approximate ADC battery millivolts. **RSSI is not the strength of the internet station's radio transmitter**; the buffer is not a signal indicator.
- Neither C nor S pauses or rewrites an internet stream. The C key **inside Tap Tempo** only clears tempo, and the S key **inside Mood Tuner** changes its vertical position. The S key held while booting still requests safe start.

This has been host-tested only for the TZ string guard and ESP32 compilation when available; no physical timezone/DST, battery voltage or LCD test has been carried out yet.
