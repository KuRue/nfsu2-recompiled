#!/usr/bin/env python3
"""Check upstream or prepare an isolated integration worktree; never merge/push."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args], text=True).strip()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare', type=Path, help='New worktree directory for reviewing an upstream merge')
    args = parser.parse_args()
    config = json.loads((ROOT / 'config/phone-update.json').read_text())
    if git('remote', 'get-url', 'upstream') != config['upstream']:
        parser.error('upstream remote differs from the configured source')
    subprocess.run(['git', '-C', str(ROOT), 'fetch', 'upstream', 'main'], check=True)
    current, upstream = git('rev-parse', 'HEAD'), git('rev-parse', 'upstream/main')
    print('Phone commit:', current)
    print('Upstream commit:', upstream)
    print('Phone-only / upstream-only commits:', git('rev-list', '--left-right', '--count', f'{current}...{upstream}'))
    print(git('log', '--oneline', f'{current}..{upstream}'))
    if args.prepare:
        if git('status', '--porcelain'):
            parser.error('Commit or stash changes before preparing integration')
        if git('branch', '--show-current') != config['branch']:
            parser.error('Prepare from the configured phone branch')
        branch = f'integration/upstream-{upstream[:12]}-phone-{current[:12]}'
        subprocess.run(['git', '-C', str(ROOT), 'worktree', 'add', '-b', branch, str(args.prepare.resolve()), current], check=True)
        print('Review worktree:', args.prepare.resolve())
        print('Upstream is NOT merged. Review changes and merge the upstream commit above there.')

if __name__ == '__main__':
    main()
