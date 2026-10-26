# Maintaining the phone fork

The GitHub default branch is android/phone-presets, the maintained native phone
branch. main remains the historical upstream baseline. Fetch upstream/main for
reviews; do not reset or force-push the phone branch. See ANDROID_UPDATES.md for
the isolated integration workflow and verified local APK archives.

## Signing-key backup

The existing phone development signing key remains outside Git. A byte-for-byte
backup was created on the build machine at
~/.local/share/nfsu2-signing-backup/phone-debug.keystore, with directory permissions
0700 and file permissions0600. A certificate export from the backup matches the
pinned config/phone-update.json SHA256. verification.json records the verification
without exposing private-key material. The source ~/.android/debug.keystore is
also permission0600. Never commit either file or send it through logs/chat.

This is a same-machine recovery copy, not protection against disk or machine loss.
The owner should keep an additional copy in their encrypted backup/password
manager. No key was uploaded to GitHub or any other external service. The current
key is a development key; establishing a public production channel needs a separate
signing decision. Do not replace the existing key for updates to the installed app.

## Automated checks

.github/workflows/phone-source-checks.yml runs the same
scripts/check_phone_source.py command used locally. It runs on phone/integration
pushes, pull requests and manual dispatch, on Ubuntu24.04 with runner JDK17.
Checkout is pinned by commit, repository access is read-only, credentials are not
persisted, and no repository secrets are needed. It does not build or upload APKs,
proprietary inputs, game assets, lifted C, signing keys or device logs.

The suite compiles/runs eight independent C++ regressions and one Java regression,
then parses the workflow helper scripts. Build outputs use a temporary directory
removed when the command exits. Assertions stay enabled. These checks do not
validate Android UI, Vulkan drivers, installation, actual races, or perceived
haptics. Device checks remain necessary for changes to those areas.

A failing workflow should be fixed before integrating upstream. An APK release
still requires a clean committed build, identity/signature verification, on-device
validation, save backup and explicit approval to publish. No release is published
by this workflow. Additional-device and longer-duration testing remain open work.
