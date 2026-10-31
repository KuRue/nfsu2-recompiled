# Maintaining the fork

## Repository and branch ownership

- Canonical repository: [KuRue/nfsu2-recompiled](https://github.com/KuRue/nfsu2-recompiled).
- Default/canonical branch: `android/phone-presets`.
- Current release development branch: `android/visual-effects`; v0.14.14 was tagged there.
- `main` is an older upstream baseline, not the current Android fork.
- `upstream` tracks codepdbh/nfsu2-recompiled. Review/integrate deliberately; do not
  reset or force-push the fork to upstream.
- KuRue/nfsu2-android is the historical wrapper/handoff repository, not the native app source.

Keep the default branch current with validated release work using a normal merge or
fast-forward, and preserve release tags. Always pass `-R KuRue/nfsu2-recompiled` to
release/issue commands to avoid acting on the checkout's implicit upstream repository.

## Checks and local inputs

[phone-source-checks.yml](../.github/workflows/phone-source-checks.yml) runs on
canonical/release-development and integration pushes, PRs and manual dispatch.
It uses read-only repository access and pinned checkout, with no APK signing secrets.
The standalone suite needs Linux, Python 3, C++17 and JDK 17; experimental continuation
jobs add isolated test dependencies. CI does not ship proprietary generated game C,
game assets, keystores, device logs or APKs.

Run the checks appropriate to a change and device-test affected behavior. Source
checks are not substitutes for Vulkan, controller, audio, multiplayer or perceived
haptics validation. [Build instructions](BUILDING.md) and
[signed release workflow](ANDROID_UPDATES.md) describe the prepared local environment.

## Signing and dependencies

Preserve the existing development signer, pinned certificate and off-repository
backup. Store private-key backups securely outside Git and off the build machine;
a local recovery copy alone does not cover disk/machine loss. Never print key
material, upload a keystore or silently replace the signer.

Keep pcrecomp patches, DXVK revision/preparation, libadrenotools submodules and font/
free-roam/dependency license notices intact. Generated outputs are local inputs,
not new source to commit. Release manifests record dependency identity; do not
claim an APK is reproducible merely because its tag is known.

## Documentation policy

The [documentation index](README.md), user guides and status page describe shipped
behavior. Release notes describe changes relative to the preceding fork release.
Dated reports/designs retain evidence but are labeled technical history. Do not
promote a proposal, emulator-only observation or experimental state capture into a
release capability. Distinguish actual measurements from estimates and user feel
from motor API acceptance. Update menu labels/paths against source when changing UI.

The full pre-overhaul README and earlier maintenance/update/status pages are in
[the archive](archive/). HANDOFF.md records ongoing implementation/verification;
keep concise user-facing instructions in the guides instead of copying the log.
