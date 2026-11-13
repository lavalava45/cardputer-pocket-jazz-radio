#!/usr/bin/env python3
"""Verify frozen v0.7z10 source bytes against SHA256-SOURCES.txt (offline)."""
from pathlib import Path
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'firmware'
MANIFEST = SOURCE / 'SHA256-SOURCES.txt'


def verify() -> int:
    if not MANIFEST.exists():
        raise RuntimeError('Missing frozen source hash manifest')
    tracked = set()
    for entry in MANIFEST.read_text('utf-8-sig').splitlines():
        if not entry.strip() or entry.startswith('#'):
            continue
        sha, separator, name = entry.partition('  ')
        if not separator or len(sha) != 64:
            raise RuntimeError(f'Invalid manifest entry: {entry[:70]}')
        name = name.strip().replace('\\', '/')
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts or name in tracked:
            raise RuntimeError(f'Unsafe or duplicate manifest path: {name}')
        tracked.add(name)
        path = SOURCE / relative
        if not path.is_file():
            raise RuntimeError(f'Missing archived source file: {name}')
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual.lower() != sha.lower():
            raise RuntimeError(f'Archived file altered: {name}')
    extra = sorted(
        p.relative_to(SOURCE).as_posix()
        for p in SOURCE.rglob('*') if p.is_file()
        and p.name != MANIFEST.name
        and p.relative_to(SOURCE).as_posix() not in tracked
    )
    if extra:
        raise RuntimeError(f'Unlisted source files: {extra[:8]}')
    print(f'PASS: {len(tracked)} frozen firmware source files match SHA-256')
    return len(tracked)


if __name__ == '__main__':
    try:
        verify()
    except Exception as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
