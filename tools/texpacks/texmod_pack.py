#!/usr/bin/env python3
"""Convert extracted TexMod-named DDS files into a bounded native NTXP pack.

Input DDS names end in _0xXXXXXXXX.dds (TexMod's uncomplemented CRC32).
Extract/decode your separately obtained mod first. Never modifies game files.
"""
import argparse
from pathlib import Path
import re
import zlib
import bundle_reader as reader
from world_pack import load


def build(game, dds_dir, output):
    replacements = {}
    for path in Path(dds_dir).glob('*.dds'):
        match = re.search(r'_0x([0-9a-fA-F]{8})\.dds$', path.name)
        if match:
            value = reader.dds(path)
            if value: replacements[int(match[1], 16)] = value
    entries = {}; data = bytearray(); used = set(); larger = 0
    for path in sorted(Path(game).rglob('*')):
        if path.suffix.lower() not in ('.bun', '.lzc', '.bin'): continue
        try: textures = load(path)
        except (ValueError, IndexError, AssertionError): continue
        for name, (ow, oh, ofmt, base, *_) in textures.items():
            crc = zlib.crc32(base); texmod = crc ^ 0xffffffff
            if texmod not in replacements: continue
            w, h, fmt, levels, payload = replacements[texmod]
            if w < ow or h < oh or w * oh != h * ow or max(w, h) > 4096: continue
            key = crc, zlib.adler32(base)
            if key in entries: continue
            entries[key] = dict(key=key, ow=ow, oh=oh, ofmt=ofmt, w=w, h=h,
                                fmt=fmt, levels=levels, off=len(data), size=len(payload),
                                name=f'New-HD/{name:08X}')
            data += payload; data += bytes(-len(data) % 16)
            used.add(texmod); larger += w > ow
    if not entries: raise ValueError('No compatible texture matches')
    reader.write(str(output), entries, data)
    print(f'{len(entries)} replacements ({larger} higher-resolution); matched {len(used)}/{len(replacements)} DDS files; {len(data)/1048576:.1f} MiB')

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('game'); p.add_argument('dds_dir'); p.add_argument('output')
    a = p.parse_args(); build(a.game, a.dds_dir, a.output)
