#!/usr/bin/env python3
"""Run game-data-free phone regressions on a clean checkout (Linux/JDK17/C++17)."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
def run(*command):
    print('+', ' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=ROOT, check=True)

def main():
    with tempfile.TemporaryDirectory(prefix='nfsu2-source-checks-') as folder:
        build = Path(folder)
        for name in ['frame_pacer', 'frame_metrics', 'physics_probe', 'physics_haptics',
                     'gamepad_state', 'force_feedback', 'audio_mixer', 'texture_pack']:
            executable = build / name
            run(os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                '-pthread', ROOT / 'tests' / (name + '.cpp'), '-o', executable)
            run(executable)
        java = ROOT / 'android/app/src/main/java/com/nfsu2/androidevolved'
        run('javac', '-d', build, java / 'ReleaseVersion.java',
            ROOT / 'tests/java/com/nfsu2/androidevolved/ReleaseVersionTest.java')
        run('java', '-cp', build, 'com.nfsu2.androidevolved.ReleaseVersionTest')
        # Parse scripts without creating tracked-tree bytecode or invoking builds/network.
        for name in ['phone_upstream.py', 'build_phone_update.py', 'check_phone_source.py']:
            path = ROOT / 'scripts' / name
            compile(path.read_text(), str(path), 'exec')
    print('Phone source checks passed; no game input, Android SDK, signing key or network required.')

if __name__ == '__main__':
    main()
