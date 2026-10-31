# Signed updates and upstream review

This guide is for maintainers. Players should follow
[Installation → Update](INSTALLATION.md#update-an-existing-installation).

The canonical fork branch is `android/phone-presets`. Current release work also
lives on `android/visual-effects`; upstream remains
[codepdbh/nfsu2-recompiled](https://github.com/codepdbh/nfsu2-recompiled).
The separate KuRue/nfsu2-android repository is a historical wrapper/handoff project.

## Review upstream without replacing the fork

From a clean canonical-branch checkout:

```sh
python3 scripts/phone_upstream.py
python3 scripts/phone_upstream.py --prepare ../nfsu2-upstream-review
```

The first command fetches upstream/main and reports divergence. `--prepare` creates
an isolated integration worktree from the current committed phone HEAD and refuses
a dirty or wrong-branch checkout. It does not merge, build, push or publish.
Review the reported upstream commit, merge in that worktree, resolve conflicts,
and test before integrating back. Never reset/force-push the phone branch to upstream.

Worktrees do not inherit untracked game C, prepared dependencies or signing files.
Follow [Building](BUILDING.md) and keep proprietary artifacts local.

## Build an auditable signed artifact

Commit the intended version/defaults and release notes before building. On the
maintained build machine with the existing signing key and prerequisites:

```sh
# Example for a release after v0.14.14; choose the actual unused version/code.
python3 scripts/build_phone_update.py --version-code 30 --version-name 0.14.15
```

The script refuses a dirty source tree, missing key/prerequisites, mismatched
package/certificate/version, concurrent artifact build or reused destination.
It builds the ARM64 debug channel, verifies its identity/signature, and archives
APK, build log and manifest under `build/phone-artifacts/CODE-COMMIT/`.
The manifest records source/upstream/dependency identities and APK SHA256. This is
an auditable local build, not a bit-for-bit reproducibility claim.

The certificate is pinned in [config/phone-update.json](../config/phone-update.json).
Use the existing private key outside Git. A new debug keystore cannot update the
published package. Never commit/upload keys, game inputs, generated C or proprietary
build logs. The current public fork releases remain debug-signed development builds.
A production signing identity would be a separate migration decision.

## Install and roll back

Back up regular career saves and stop the game before an install that would
interrupt it. Install without uninstalling or clearing data:

```sh
adb -s PHONE_SERIAL install -r /path/to/verified.apk
```

Same package/signature preserves Android preferences/private data. Keep the previous
APK and manifest. For this debuggable channel, `adb install -r -d OLD.apk` may permit
a downgrade; Android can refuse it. Do not uninstall as a workaround without
understanding data loss. Downgrading an APK does not roll back save-file changes.

## Publish deliberately

Use explicit repository arguments: this checkout's implicit `gh` repository may
resolve to upstream rather than the fork.

1. Run relevant source/graphics checks and device tests. Record what is still unverified.
2. Verify versionCode increases, package/signature continuity, supported ABI and ZIP integrity.
3. Write notes against the **last fork release**, including upgrade steps and known limits.
4. Commit/push source, tag the exact artifact source commit and attach only intended assets.
5. Upload to a draft in `KuRue/nfsu2-recompiled`; compare remote asset sizes/digests with local files.
6. Publish with the user's authorization, verify the latest-release metadata and public download,
   then record tag, source hash, signer, checksums, tests and limitations.

v0.14.14 ships APK, SHA256SUMS, build manifest and a copy of comprehensive notes.
Source checks do not automatically publish APKs. In-app checks open the fork release
page for manual review/install; development suffixes are ignored when comparing
stable numeric release versions. Experimental prereleases are not normal update targets.

[Index](README.md) · [Maintenance](FORK_MAINTENANCE.md)
