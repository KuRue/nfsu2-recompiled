#!/usr/bin/env python3
"""Fresh-process logical graphics device restore proof; procedural assets only."""
import argparse
import pathlib
import struct
import subprocess
import tempfile
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='nfs-graphics-proof-') as tmp:
    base = pathlib.Path(tmp)
    exe = base / 'runner'
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if args.sanitize else []
    subprocess.run(['g++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', *flags, str(ROOT / 'experimental/save_states/graphics_runner.cpp'), '-o', str(exe)], check=True)
    def run(*a, ok=True):
        r = subprocess.run([str(exe), *map(str, a)], capture_output=True, text=True)
        assert (r.returncode == 0) == ok, r.stdout + r.stderr
        if not ok:
            assert 'Sanitizer' not in r.stderr and 'runtime error:' not in r.stderr, r.stderr
        return r.stdout.split()
    baseline = run('baseline')
    assert len(baseline) == 14 and len(set(baseline)) > 10
    state = base / 'state'
    for at in range(15):
        run('capture', at, state)
        assert run('restore', state) == baseline[at:], f'restored frames diverge at frame {at}'
    run('capture', 8, state)
    saved = state.read_bytes()
    run('restore', state, 'variant', ok=False)  # edited asset must be refused
    rejected = 1
    def reject(data):
        global rejected
        state.write_bytes(bytes(data))
        run('restore', state, ok=False)
        rejected += 1
    for i in list(range(0, len(saved), 7)) + [len(saved) - 1]:
        reject(saved[:i])
    reject(saved + b'x')
    flipped = bytearray(saved); flipped[-1] ^= 1; reject(flipped)
    def mutate(offset, value):
        data = bytearray(saved)
        struct.pack_into('<I', data, offset, value)
        struct.pack_into('<I', data, 16, zlib.crc32(data[20:]))
        reject(data)
    # Payload @20: frame, nextHandle, blend, texture, target, scissor*4, tint, count, textures.
    tex = 20 + 4 * 11
    mutate(20, 5000)            # frame beyond limit
    mutate(24, 2)               # handle allocator would reuse IDs
    mutate(28, 3)               # blend mode
    mutate(32, 9)               # bound texture missing
    mutate(36, 1)               # asset texture bound as render target
    mutate(40, 70)              # scissor inverted
    mutate(48, 65)              # scissor beyond screen
    mutate(tex, 3)              # duplicate texture ID
    mutate(tex + 4, 0)          # zero width
    mutate(tex + 4, 129)        # too wide
    mutate(tex + 12, 2)         # unknown origin
    mutate(tex + 16, 0)         # asset texture without asset
    mutate(tex + 16, 7)         # unknown asset
    mutate(tex + 20, 0)         # wrong asset CRC
    print(f'Graphics state proof passed: 15 fresh-process checkpoints match baseline frames; {rejected} rejected inputs')
