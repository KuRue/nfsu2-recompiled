#!/usr/bin/env python3
"""Fresh-process logical audio mixer restore proof; synthetic assets only."""
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
with tempfile.TemporaryDirectory(prefix='nfs-audio-proof-') as tmp:
    base = pathlib.Path(tmp)
    exe = base / 'runner'
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if args.sanitize else []
    subprocess.run(['g++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', *flags, str(ROOT / 'experimental/save_states/audio_runner.cpp'), '-o', str(exe)], check=True)
    def run(*a, ok=True):
        r = subprocess.run([str(exe), *map(str, a)], capture_output=True, text=True)
        assert (r.returncode == 0) == ok, r.stdout + r.stderr
        if not ok:
            assert 'Sanitizer' not in r.stderr and 'runtime error:' not in r.stderr, r.stderr
        return r.stdout.split()
    baseline = run('baseline')
    assert len(baseline) == 16 and len(set(baseline)) > 8
    state = base / 'state'
    for at in range(17):
        run('capture', at, state)
        assert run('restore', state) == baseline[at:], f'restored audio diverges at block {at}'
    run('capture', 7, state)
    saved = state.read_bytes()
    run('restore', state, 'variant', ok=False)  # edited asset must be refused
    rejected = 1
    def reject(data):
        global rejected
        state.write_bytes(bytes(data))
        run('restore', state, ok=False)
        rejected += 1
    for i in range(len(saved)):
        reject(saved[:i])
    reject(saved + b'x')
    flipped = bytearray(saved); flipped[-1] ^= 1; reject(flipped)
    def mutate(offset, value):
        data = bytearray(saved)
        struct.pack_into('<I', data, offset, value)
        struct.pack_into('<I', data, 16, zlib.crc32(data[20:]))
        reject(data)
    # Payload @20: block, nextVoice, buffer count, (id, crc)*3, voice count, voices of 7 words.
    voices = 20 + 12 + 3 * 8 + 4
    mutate(20, 5000)                 # block index beyond limit
    mutate(24, 1)                    # voice IDs not below allocator
    mutate(32, 99)                   # unknown buffer
    mutate(36, 0)                    # wrong asset CRC
    mutate(voices, 0)                # zero voice ID
    mutate(voices + 8, 700 << 16)    # position past buffer end while playing
    mutate(voices + 12, 0)           # zero step
    mutate(voices + 16, 257)         # volume
    mutate(voices + 20, 2)           # loop flag
    mutate(voices + 28, 1)           # duplicate voice ID
    print(f'Audio state proof passed: 17 fresh-process checkpoints match baseline PCM; {rejected} rejected inputs')
