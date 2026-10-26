#!/usr/bin/env python3
"""Fresh-process read-only resource reconstruction; only temporary fixture files."""
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
with tempfile.TemporaryDirectory(prefix='nfs-file-proof-') as tmp:
    base = pathlib.Path(tmp)
    exe = base / 'runner'
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if args.sanitize else []
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', *flags, str(ROOT / 'experimental/save_states/file_runner.cpp'), '-o', str(exe)], check=True)
    root = base / 'root'
    (root / 'data').mkdir(parents=True)
    source = root / 'data/input.bin'
    original = b'abcdefghijklmnopqrstuvwxyz'
    source.write_bytes(original)
    state = base / 'state'
    def run(mode, ok=True):
        result = subprocess.run([str(exe), mode, str(root), str(state)], capture_output=True, text=True)
        assert (result.returncode == 0) == ok, result.stdout + result.stderr
        if not ok:
            assert 'AddressSanitizer' not in result.stderr and 'runtime error:' not in result.stderr, result.stderr
    run('capture')
    saved = state.read_bytes()
    run('restore')
    rejected = 0
    def reject(data):
        global rejected
        state.write_bytes(data)
        run('restore', False)
        rejected += 1
    for i in range(len(saved)):
        reject(saved[:i])
    reject(saved + b'x')
    broken = bytearray(saved)
    broken[-1] ^= 1
    reject(broken)
    def mutate(offset, value):
        data = bytearray(saved)
        data[offset:offset + len(value)] = value
        struct.pack_into('<I', data, 16, zlib.crc32(data[20:]))
        reject(data)
    # Payload: count, first id, position, path length, path, content length/content.
    mutate(24, struct.pack('<I', 0))
    mutate(28, struct.pack('<I', 65537))
    mutate(36, b'../a/input.bin')
    mutate(36, b'/ata/input.bin')
    mutate(36, b'data/./put.bi\x00')
    second_id = 20 + 4 + 16 + len(b'data/input.bin') + len(original)
    mutate(second_id, struct.pack('<I', 100))
    state.write_bytes(saved)
    for changed in [b'ABC' + original[3:], original[:-1], original + b'!']:
        source.write_bytes(changed)
        run('restore', False)
        rejected += 1
    source.unlink()
    run('restore', False)
    source.symlink_to(base / 'outside')
    (base / 'outside').write_bytes(original)
    run('restore', False)
    source.unlink()
    (root / 'data').rmdir()
    external = base / 'external'
    external.mkdir()
    (external / 'input.bin').write_bytes(original)
    (root / 'data').symlink_to(external, target_is_directory=True)
    run('restore', False)
    (root / 'data').unlink()
    (root / 'data').mkdir()
    source.write_bytes(original)
    run('restore')
    assert source.read_bytes() == original
    assert not list(base.glob('*.partial-*'))
    print(f'File resource proof passed: fresh-process positions, independent handles, EOF/seek; {rejected + 3} rejected inputs/resources')
