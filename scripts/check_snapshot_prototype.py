#!/usr/bin/env python3
"""Experimental fresh-process continuation proof; never reads real game saves."""
import argparse
import hashlib
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true', help='Run with AddressSanitizer and UndefinedBehaviorSanitizer')
    args = parser.parse_args()
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []
    with tempfile.TemporaryDirectory(prefix='nfsu2-snapshot-prototype-') as folder:
        directory = Path(folder)
        binary = directory / 'prototype'
        subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                        *flags, '-isystem', str(ROOT / 'android/app/src/main/cpp'), str(ROOT / 'experimental/save_states/prototype.cpp'), '-o', str(binary)], check=True)
        build = hashlib.sha256(binary.read_bytes()).hexdigest()
        def run(mode, *args):
            return subprocess.check_output([str(binary), mode, build, *map(str, args)], text=True).strip()
        print(run('selftest'))
        baseline = run('baseline')
        state = directory / 'checkpoint.nfsproto'
        print(run('capture', state))  # This process has exited before restore starts.
        restored = run('restore', state)
        if baseline != restored:
            raise RuntimeError(f'Restored execution differs: {baseline!r} versus {restored!r}')
        expected = f'result={(3 * sum(range(1, 101))) ^ 0x12345} ticks=100 flags={(3 * sum(range(1, 100)) + 100) & 1}'
        if restored != expected:
            raise RuntimeError(f'Fixture result differs from independent expected value: {restored}')
        original = state.read_bytes()
        # Unsupported resources must fail before replacing a previous valid state.
        refused = subprocess.run([str(binary), 'capture-unsupported', build, str(state)], capture_output=True, text=True)
        if refused.returncode == 0 or state.read_bytes() != original:
            raise RuntimeError('Rejected capture modified the previous snapshot')
        if list(directory.glob('*.partial-*')):
            raise RuntimeError('Snapshot staging files were not cleaned up')
        corrupt = directory / 'corrupt.nfsproto'
        corrupt.write_bytes(original[:-1])
        rejected = subprocess.run([str(binary), 'restore', build, str(corrupt)], capture_output=True, text=True)
        if rejected.returncode == 0:
            raise RuntimeError('Truncated state unexpectedly restored')
        print('Fresh-process restore matches uninterrupted run:', restored)
        print('Experimental integer fixture only; actual NFSU2 save states are NOT implemented.')

if __name__ == '__main__':
    main()
