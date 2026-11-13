#!/usr/bin/env python3
"""Convert an exactly 64x64 RGB/RGBA PNG to Pocket Jazz logo v1.

Install Pillow separately (`python -m pip install Pillow`). No rescaling,
network access or metadata copying. PNG alpha is composited onto black.
"""
import argparse
import os
from pathlib import Path
import struct
import tempfile
import zlib

WIDTH = HEIGHT = 64
PAYLOAD_SIZE = WIDTH * HEIGHT * 2
HEADER = struct.Struct('<4sBBBBII')


def station_file_id(url):
    """Stable 16-digit filename for exact UTF-8 station URL (FNV-1a 64)."""
    value = 14695981039346656037
    for byte in url.encode('utf-8'):
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f'{value:016x}'


def encode_pixels(rgb_bytes):
    if len(rgb_bytes) != WIDTH * HEIGHT * 3:
        raise ValueError('expected exactly 64x64 RGB pixels')
    output = bytearray(PAYLOAD_SIZE)
    for i in range(WIDTH * HEIGHT):
        r, g, b = rgb_bytes[i * 3:i * 3 + 3]
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        struct.pack_into('<H', output, i * 2, value)
    return bytes(output)


def pack_image(rgb_bytes):
    pixels = encode_pixels(rgb_bytes)
    return HEADER.pack(b'PJLG', 1, WIDTH, HEIGHT, 1, PAYLOAD_SIZE,
                       zlib.crc32(pixels) & 0xffffffff) + pixels


def validate_image(data):
    if len(data) != HEADER.size + PAYLOAD_SIZE:
        return False
    magic, version, width, height, fmt, size, checksum = HEADER.unpack_from(data)
    return (magic, version, width, height, fmt, size) == (b'PJLG', 1, 64, 64, 1, PAYLOAD_SIZE) \
        and (zlib.crc32(data[HEADER.size:]) & 0xffffffff) == checksum


def convert_png(source):
    from PIL import Image
    with Image.open(source) as image:
        if image.format != 'PNG' or image.size != (WIDTH, HEIGHT):
            raise ValueError('source must be a PNG of exactly 64x64 pixels')
        # Bounded conversion; never expand user-selected images of unknown size.
        if image.mode not in ('RGB', 'RGBA'):
            raise ValueError('PNG must use RGB or RGBA mode')
        if image.mode == 'RGBA':
            background = Image.new('RGBA', image.size, (0, 0, 0, 255))
            background.alpha_composite(image)
            image = background.convert('RGB')
        return pack_image(image.tobytes())


def write_atomic(path, data):
    path = Path(path)
    if path.suffix.lower() != '.pjlg':
        raise ValueError('output extension must be .pjlg')
    if not validate_image(data):
        raise ValueError('refusing to write invalid logo')
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile('wb', dir=path.parent,
                                         prefix='.pjlg-', suffix='.tmp', delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(data)
        os.replace(temporary, path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('png', type=Path, help='64x64 RGB/RGBA PNG input')
    parser.add_argument('pjlg', type=Path, nargs='?', help='explicit output .pjlg file')
    parser.add_argument('--station-url', help='exact stream URL from M3U/PLS')
    parser.add_argument('--output-dir', type=Path, help='SD PocketJazz/logos directory')
    args = parser.parse_args()
    if args.station_url is not None:
        if args.pjlg is not None or args.output_dir is None:
            parser.error('--station-url requires --output-dir and no explicit pjlg file')
        target = args.output_dir / (station_file_id(args.station_url) + '.pjlg')
    else:
        if args.pjlg is None or args.output_dir is not None:
            parser.error('supply an output .pjlg file or --station-url with --output-dir')
        target = args.pjlg
    write_atomic(target, convert_png(args.png))
    print(f'Wrote {HEADER.size + PAYLOAD_SIZE} bytes: {target}')


if __name__ == '__main__':
    main()
