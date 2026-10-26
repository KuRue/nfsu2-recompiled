# Phone fork updates

The phone app lives in KuRue/nfsu2-recompiled on android/phone-presets. Upstream
is codepdbh/nfsu2-recompiled. Keep phone changes in the fork; never replace the
phone branch with upstream main. The older KuRue/nfsu2-android repository is a
historical wrapper/handoff project, not this native app's update source.

## Check and review upstream

From a clean phone checkout:

```
python3 scripts/phone_upstream.py
python3 scripts/phone_upstream.py --prepare ../nfsu2-upstream-review
```

The check fetches upstream/main and reports commit divergence. Prepare creates
an integration branch/worktree at the committed phone HEAD; it does not merge,
push, build or publish. It refuses dirty/wrong-branch preparation. Review the
reported upstream commit in that worktree, merge it there, resolve conflicts,
and test before merging the integration branch back into the phone branch.
Retain the exact upstream commit in the review notes. Never auto-merge game code.
Generated sources and local dependencies are not copied to a new worktree: follow
the README build pipeline there with the required local game input and prepared
pcrecomp/DXVK dependencies. There is no new upstream-only commit as of2026-10-26
(a8c7a39ce7b75b7a49368445a9a2b7ca5cf6e1f3), so no integration was needed today.

## Build a reviewable phone APK

On this prepared Linux build machine (Python3.11+, JDK17, SDK build-tools36.0.0,
NDK28.2.13676358, CMake3.30.5), commit changes, then run:

```
python3 scripts/build_phone_update.py --version-code 26 --version-name 0.14.11
```

Use an increasing Android version code for subsequent releases. The script uses
the existing local generated C, sibling pcrecomp, prepared build/dxvk-source and
the existing ~/.android/debug.keystore. These are required local prerequisites;
a clean GitHub checkout alone cannot build the game. No game inputs, lifted C,
textures, keystore or APK are committed. Do not upload proprietary build logs or
inputs to public CI. Do not run another Gradle build concurrently.

The script builds one ARM64 debug-channel APK, verifies the package and pinned
certificate in config/phone-update.json, and archives APK/build log/manifest under
ignored build/phone-artifacts/<code>-<commit>/. The manifest records APK SHA256,
source/upstream commits and dependency commits/dirty flags. It is an auditable
local build process, not a promise of bit-for-bit reproducibility across machines.
The output is neither installed nor published. Reusing the same destination is
refused. A changed signer is an error, never something to bypass for convenience.

## Preserve data and signing identity

The installed phone channel is com.nfsu2.androidevolved.dev. Its signer certificate
SHA256 is pinned in config/phone-update.json. Keep a secure backup of the existing
private keystore outside Git; a replacement key cannot update this installed app.
The current channel uses a debug key and remains a private development channel.
A future public production signing/channel decision must be explicit.

Install a verified APK over the current app, without uninstalling or clearing data:

```
adb -s PHONE_SERIAL install -r build/phone-artifacts/CODE-COMMIT/NFSU2-phone-dev-arm64.apk
```

Matching package/signature preserves app preferences/private data; game saves and
texture packs remain under /sdcard/nfsu2. Back up saves before a game-code update:
an upstream change can alter save compatibility even when Android preserves files.
Archive the last working APK and manifest. On this debuggable channel an older
APK may be restored with `adb install -r -d OLD.apk`; Android may refuse a downgrade.
Do not uninstall to work around a refusal: that can delete private settings. Restoring
an APK does not undo save changes; restoring backups is a separate explicit action.

## In-app checks and publishing boundary

Debug phone builds check releases in KuRue/nfsu2-recompiled, with cache isolated
from upstream. They open the fork release page for manual review, not an arbitrary
APK asset. Stable numeric tags such as v0.14.11 are compared with the base installed
version, ignoring only our fixed development suffix. Prereleases are excluded.
Release notes resolve the base tag. The upstream production build keeps its original
update source. No release exists in our fork yet; the app reports none available.

After device validation, a manually approved release can attach the verified
phone APK, checksum and clear package/channel notes. Do not attach an upstream
production APK to a phone-channel release. Publishing is not part of these scripts.
Future automation can run source checks without proprietary inputs; public APK CI
requires a separate decision about private inputs and signing credentials.
