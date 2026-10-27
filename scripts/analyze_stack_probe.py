#!/usr/bin/env python3
"""Summarize NFSStackProbe logcat lines from a NFS_STACK_PROBE development build.

Usage: analyze_stack_probe.py LOGCAT_FILE [--coverage] [--live-out FILE]
Reports the lifted functions live at each capture-point kind (they would need continuation
form), chains that cross a host callback (0xffffffff) and so cannot be captured as-is, and with
--coverage how many of the live functions the experimental emitter already accepts."""
import collections
import re
import sys

LINE = re.compile(r'NFSStackProbe(?:\(\s*\d+\))?: (\S+) depth=(\d+):((?: [0-9a-f]{8})+)')
chains = collections.defaultdict(list)
for line in open(sys.argv[1], errors='replace'):
    m = LINE.search(line)
    if m:
        chains[m.group(1)].append([int(v, 16) for v in m.group(3).split()])
if not chains:
    sys.exit('no NFSStackProbe lines found')
live = set()
for site, items in sorted(chains.items()):
    funcs = {v & 0x7fffffff for chain in items for v in chain if v not in (0xffffffff, 0xeeeeeeee) and v}
    crossing = sum(0xffffffff in chain[1:] for chain in items)
    depths = sorted(len(c) for c in items)
    print(f'{site}: {len(items)} unique chains, depth {depths[0]}-{depths[-1]}, {len(funcs)} functions, '
          f'{crossing} cross a host callback')
    live |= funcs
print(f'all sites: {len(live)} distinct live functions')
if '--live-out' in sys.argv:
    with open(sys.argv[sys.argv.index('--live-out')+1],'w') as out:
        out.writelines(f'{v:08x}\n' for v in sorted(live))
if '--coverage' in sys.argv:
    import json
    import pathlib
    sys.argv = [sys.argv[0]]
    root = pathlib.Path(__file__).resolve().parents[1]
    catalog = {f['address']: f for f in json.load(open(root / 'work/catalog.json'))['functions']}
    print(f'{len(live & catalog.keys())} of them are catalog functions')
    for v in sorted(live - catalog.keys()):
        print(f'  not a catalog entry: {v:08x}')
