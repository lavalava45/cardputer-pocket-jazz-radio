#!/usr/bin/env python3
"""Render transparent-about-the-source LCD layout previews for GitHub docs.

These are graphical layout reconstructions, NOT physical or emulator captures.
The colors/labels/positions come from firmware/PocketJazzRadio/PocketJazzRadioApp.cpp.
Some demonstration artist/track values are illustrative rather than live ICY.
Requires Pillow: python -m pip install Pillow.
"""
from __future__ import annotations

import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'firmware/PocketJazzRadio/PocketJazzRadioApp.cpp').read_text('utf-8')
DEST = ROOT / 'docs/images'
W, H, SCALE = 240, 135, 4
BG = '#071624'
GOLD = '#D5B06D'
WHITE = '#E9EEF1'
MUTED = '#9CAFC0'
PANEL = '#203249'
RULE = '#304457'
GREEN = '#76CBAC'
RED = '#E78080'
FONT_DIR = Path(r'C:\Windows\Fonts')


def face(size=10, bold=False):
    name = 'consolab.ttf' if bold else 'consola.ttf'
    path = FONT_DIR / name
    if not path.is_file():
        return ImageFont.load_default()
    return ImageFont.truetype(str(path), size)


def assert_contract():
    for marker in (
        'text(9, 6, "POCKET JAZZ"',
        'drawWrappedRadioText(station, 28',
        'text(9, 97, "VU"',
        'snprintf(volumeLabel, sizeof(volumeLabel), "VOL %u%%"',
        'text(9, 116, dailyAlarmSounding ? "ALARM"',
        'text(10, 7, "BLUE NOTE CLOCK"',
        'text(8, 6, "BROWSE STATIONS"',
        '"WI-FI STATUS", "WI-FI DISCONNECTS", "HTTPS CONNECTION"',
        '"LEFT/RIGHT TUNE"',
    ):
        if marker not in SOURCE:
            raise SystemExit(f'Firmware layout differs; review preview code: {marker}')


class LCD:
    def __init__(self, *, black=False):
        self.image = Image.new('RGB', (W, H), '#050708' if black else BG)
        self.draw = ImageDraw.Draw(self.image)

    def txt(self, x, y, s, color=WHITE, size=10, bold=False):
        # Pixel clipping is handled by the firmware. Preview strings are
        # explicitly trimmed to avoid exceeding the physical 240x135 screen.
        self.draw.text((x, y), s, font=face(size, bold), fill=color, anchor='lt')

    def line(self, y):
        self.draw.line((9, y, 231, y), fill=RULE)

    def save(self, name):
        DEST.mkdir(parents=True, exist_ok=True)
        # A nearest-neighbor enlargement highlights the native LCD geometry
        # rather than pretending to be a high-res hardware photograph.
        self.image.resize((W * SCALE, H * SCALE), Image.Resampling.NEAREST).save(
            DEST / name, optimize=True
        )
        print(name, (DEST / name).stat().st_size, 'bytes')


def radio():
    s = LCD()
    s.txt(9, 6, 'POCKET JAZZ', GOLD, 10, True)
    s.txt(123, 6, '86%', MUTED)
    s.txt(161, 6, 'WiFi ON', GREEN)
    s.line(22)
    s.txt(9, 28, 'Radio Swiss Jazz', WHITE, 12, True)
    # Track metadata is a manually labelled layout SAMPLE, not a claim about
    # what that broadcaster was actually transmitting at capture time.
    s.txt(9, 49, 'Dave Brubeck Quartet', MUTED, 11)
    s.txt(9, 70, 'Take Five', GOLD, 12)
    s.txt(9, 97, 'VU', MUTED)
    # mini curved analog-style needle, physically within x=30..92
    s.draw.line((34, 107, 90, 107), fill=MUTED)
    for x in (34, 62, 90):
        s.draw.line((x, 103, x, 108), fill=MUTED)
    s.draw.line((62, 107, 79, 96), fill=GOLD, width=1)
    s.draw.ellipse((61, 106, 63, 108), fill=GOLD)
    s.txt(107, 97, 'VOL 27%', WHITE)
    s.line(111)
    s.txt(9, 116, 'ON AIR', GREEN, 10, True)
    s.txt(94, 116, 'H - Help', MUTED)
    s.save('radio-preview.png')


def clock():
    s = LCD(black=True)
    s.txt(10, 7, 'BLUE NOTE CLOCK', GOLD)
    s.txt(173, 7, 'ON AIR', MUTED)
    s.txt(75, 26, '19:42', WHITE, 24, True)
    s.txt(75, 60, 'Fri 13 Nov', MUTED)
    s.txt(11, 77, 'Radio Swiss Jazz', GOLD)
    s.txt(11, 90, 'Dave Brubeck - Take Five', WHITE)
    s.txt(11, 105, 'TZ UTC+2', MUTED)
    s.txt(9, 121, 'UP/DN TZ 30m  0 RESET  DEL BACK', WHITE, 8)
    s.save('clock-preview.png')


def browse():
    s = LCD()
    s.txt(8, 6, 'BROWSE STATIONS', GOLD, 17, True)
    s.txt(9, 27, '01 - Essentials', MUTED)
    choices = [('Jazz24', 'unknown'), ('Radio Swiss Jazz', 'unknown'),
               ('FIP Jazz', 'unknown'), ('TSF Jazz', 'unknown')]
    for i, (station, title) in enumerate(choices):
        y = 43 + 18 * i
        if i == 1:
            s.draw.rounded_rectangle((6, y-2, 234, y+15), 2, fill=PANEL)
        s.txt(10, y, station, GOLD if i == 1 else WHITE)
        s.txt(162, y, title, MUTED, 8)
    s.txt(8, 118, 'ICY peek OFF (SD opt-in)', MUTED, 8)
    s.txt(8, 127, 'UP/DOWN P NEXT PLAYLIST ENTER PLAY DEL', WHITE, 7)
    s.save('browse-preview.png')


def diagnostics():
    s = LCD()
    s.txt(8, 5, 'FREE MEMORY 7/8', GOLD, 10, True)
    s.txt(192, 5, '0.7z10', WHITE, 8)
    s.draw.line((8, 18, 232, 18), fill=RULE)
    # Demo figures are illustrative, not readings captured on a device.
    rows = [
        ('Heap available:', '81240 B'),
        ('Largest free block:', '31120 B'),
        ('DMA available:', '58400 B'),
        ('MP3 ring:', '16384 B'),
        ('Audio task stack:', '4080 B'),
        ('PC sample:', 'illustrative'),
    ]
    for i, (label, value) in enumerate(rows):
        y = 24 + 14 * i
        s.txt(9, y, label, WHITE, 10)
        s.txt(136, y, value, GREEN if i == 1 else MUTED, 9)
    s.txt(8, 120, 'M RAM(STOP) E status X reset', WHITE, 8)
    s.save('diagnostics-preview.png')


def help_screen():
    s = LCD()
    s.txt(8, 3, 'HELP 1/17  RADIO', GOLD, 10, True)
    s.draw.line((8, 24, 232, 24), fill=RULE)
    for i, label in enumerate(('LEFT/RIGHT TUNE', 'P NEXT PLAYLIST',
                               'ENTER PLAY/STOP', '- / = VOLUME')):
        s.txt(8, 29 + i * 20, label, WHITE, 14, True)
    s.draw.line((8, 110, 232, 110), fill=RULE)
    s.txt(8, 121, 'UP/DOWN PAGES   DEL BACK', WHITE, 9)
    s.save('help-preview.png')


def cover():
    # Optional cover for the standalone full-flash M5Burner upload workflow.
    # Not required for the recommended app-only M5Launcher installation.
    canvas = Image.new('RGB', (800, 480), '#0A1420')
    d = ImageDraw.Draw(canvas)
    d.rounded_rectangle((50, 48, 750, 431), radius=25,
                        fill='#15263A', outline='#40596A', width=3)
    d.text((91, 87), 'POCKET JAZZ', fill=GOLD, font=face(50, True))
    d.text((94, 161), 'JAZZ, WHEREVER WI-FI REACHES', fill=WHITE,
           font=face(25))
    d.line((94, 219, 704, 219), fill='#445565', width=3)
    d.text((94, 250), 'M5Stack Cardputer ADV', fill=MUTED, font=face(27))
    d.text((94, 294), 'Live MP3 radio  /  curated jazz stations', fill=WHITE,
           font=face(20))
    d.text((94, 345), 'v0.7z10  |  Preview release', fill=GOLD,
           font=face(21, True))
    canvas.save(DEST / 'm5burner-cover.png', optimize=True)


if __name__ == '__main__':
    assert_contract()
    radio()
    clock()
    browse()
    diagnostics()
    help_screen()
    cover()
