#!/usr/bin/env python3
"""Build and archive a signed phone APK locally. Does not install or publish."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def output(*command):
    return subprocess.check_output(command, cwd=ROOT, text=True).strip()
def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()
def dependency(path):
    return {'commit': output('git', '-C', str(path), 'rev-parse', 'HEAD'),
            'dirty': bool(output('git', '-C', str(path), 'status', '--porcelain'))}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version-code', type=int, required=True)
    parser.add_argument('--version-name', required=True, help='Three-part release version, e.g. 0.14.11')
    args = parser.parse_args()
    if args.version_code <= 25 or not re.fullmatch(r'\d{1,9}\.\d{1,9}\.\d{1,9}', args.version_name):
        parser.error('Use a version code above 25 and a three-part numeric name')
    if output('git', 'status', '--porcelain'):
        parser.error('Commit source changes before building an auditable artifact')
    config = json.loads((ROOT / 'config/phone-update.json').read_text())
    commit = output('git', 'rev-parse', 'HEAD')
    destination = ROOT / 'build/phone-artifacts' / f'{args.version_code}-{commit[:12]}'
    if destination.exists():
        parser.error(f'Artifact already exists: {destination}')
    sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'Android/Sdk')))
    tools = sdk / 'build-tools/36.0.0'
    for path in [ROOT / 'src/recomp/gen', ROOT / 'build/dxvk-source', ROOT.parent / 'pcrecomp', tools / 'apksigner', tools / 'aapt']:
        if not path.exists():
            parser.error(f'Missing local build prerequisite: {path}; see docs/ANDROID_UPDATES.md')
    # Keep the existing private signing key outside the repository. Never generate a replacement.
    if not (Path.home() / '.android/debug.keystore').is_file():
        parser.error('Existing phone signing key missing; restore it, do not generate another')
    (ROOT / 'build').mkdir(exist_ok=True)
    with (ROOT / 'build/phone-update.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        log = ROOT / 'build/phone-update-build.log'
        env = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL='4')
        with log.open('w') as stream:
            subprocess.run(['bash', 'android/gradlew', '-p', 'android', '--no-daemon', '--console=plain', '--max-workers=2',
                            f'-PphoneVersionCode={args.version_code}', f'-PphoneVersionName={args.version_name}', 'assembleDebug'],
                           cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
        apk = ROOT / 'android/app/build/outputs/apk/debug/app-debug.apk'
        certificates = output(str(tools / 'apksigner'), 'verify', '--print-certs', str(apk))
        signer = re.findall(r'certificate SHA-256 digest: ([0-9a-f]+)', certificates)
        if signer != [config['certificate_sha256']]:
            raise RuntimeError('Signing certificate differs from the installed phone channel; refusing to archive')
        badging = output(str(tools / 'aapt'), 'dump', 'badging', str(apk))
        identity = re.search(r"package: name='([^']+)' versionCode='([^']+)' versionName='([^']+)'", badging)
        expected = (config['package'], str(args.version_code), args.version_name + '-phone-dev-physics')
        if not identity or identity.groups() != expected:
            raise RuntimeError('APK identity/version differs from the requested phone channel')
        manifest = {'source_commit': commit, 'upstream_commit': output('git', 'rev-parse', 'upstream/main'),
                    'package': expected[0], 'version_code': args.version_code, 'version_name': expected[2],
                    'certificate_sha256': signer[0], 'apk_sha256': digest(apk),
                    'dependencies': {'pcrecomp': dependency(ROOT.parent / 'pcrecomp'),
                                     'dxvk': dependency(ROOT / 'build/dxvk-source')},
                    'generated_sources': 'Local proprietary input required; not included in this artifact'}
        destination.mkdir(parents=True)
        shutil.copy2(apk, destination / 'NFSU2-phone-dev-arm64.apk')
        shutil.copy2(log, destination / 'build.log')
        (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(destination)
        print('Verified package/signature; APK archived. Nothing installed or published.')

if __name__ == '__main__':
    main()
