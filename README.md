# NFSU2 Android Evolved — native Android fork

Play Need for Speed: Underground 2 through a native Android recompilation with
DXVK/Vulkan rendering. This fork adds phone-focused graphics controls, an
Underground 2-style launcher, native nfsu2.xyz free roam, controller support and
physics-based vibration. It does not run the browser version inside a WebView.

**[Download v0.14.14](https://github.com/KuRue/nfsu2-recompiled/releases/tag/v0.14.14)** ·
[Release notes](docs/releases/v0.14.14.md) · [All documentation](docs/README.md) ·
[Report a problem](https://github.com/KuRue/nfsu2-recompiled/issues)

[![Phone source checks](https://github.com/KuRue/nfsu2-recompiled/actions/workflows/phone-source-checks.yml/badge.svg?branch=android%2Fphone-presets)](https://github.com/KuRue/nfsu2-recompiled/actions/workflows/phone-source-checks.yml)

## Get started

1. Use an **ARM64 Android 10+** device with a Vulkan 1.1-capable driver.
2. Install the ARM64 APK from this fork's release page.
3. Copy your supported PC game files into **Internal storage/nfsu2** (`/sdcard/nfsu2/`).
4. Open **NFSU2 Native Dev**, grant file access, and check the Game Files panel.
5. Choose your game language and settings, then press **Play**.

The APK includes no game data. The supported `SPEED2.EXE` is 4,800,512 bytes with
SHA-256 `f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
See the [installation guide](docs/INSTALLATION.md) for the folder layout, saves,
updates and permission details.

## What this release offers

- Gray/black/lime launcher and touch controls styled after Underground 2 menus.
- In-game graphics side panel, color grading, bloom, sharpening, detail/reflection
  controls, optional separately installed HD packs and a small FPS counter.
- Native nfsu2.xyz free roam with player names and map markers; local LAN and the
  game's separate Online server selection remain available.
- Controller triggers and driving actions, plus a **right-stick pointer / R3 click**
  fallback for mouse-only game menus.
- Controller-first vibration, independent strong/weak rumble on compatible pads,
  phone fallback, and separate traction/impact test buttons.
- Save backup/restore, live-session Resume, diagnostics and manual update checking.

## Read the guides

| Task | Guide |
| --- | --- |
| Install, update, locate saves, understand Resume | [Installation](docs/INSTALLATION.md) |
| Controller mapping, touch layout, vibration | [Controls](docs/CONTROLS.md) |
| Live graphics, restart-only settings, HD packs | [Graphics](docs/GRAPHICS.md) |
| nfsu2.xyz, local LAN, Online mode | [Multiplayer](docs/MULTIPLAYER.md) |
| Missing files, performance, crashes, input or sound issues | [Troubleshooting](docs/TROUBLESHOOTING.md) |
| Build from source and run regressions | [Building](docs/BUILDING.md) |
| Review upstream and publish signed updates | [Maintaining the fork](docs/FORK_MAINTENANCE.md) |

## Release identity and limits

v0.14.14 is an **ARM64, debug-signed development build**, package
`com.nfsu2.androidevolved.dev`. It updates previous releases of this fork and
installs beside the upstream production app. Keep the existing app installed
when updating so its settings are preserved.

Performance varies with device, scene and settings. Crowded multiplayer and cold
asset loads can still hitch. Vibration depends on the controller/Android driver;
physics cues estimate slip and impacts rather than measuring tire forces. The
latest controller-pointer interactions and haptic distinction need broader user
validation. [Current validation and known limits](docs/STATUS.md).

**Save states are not shipped.** Resume retains a live process when Android keeps
it available; it does not restore a disk snapshot after process death. Ray tracing,
upscaling and replacement high-polygon assets are not included. A 32-bit source
build exists, but this release download is ARM64 only.

## Source and credits

The canonical fork branch is `android/phone-presets`; `android/visual-effects`
contains the current release work. `main` retains an older upstream baseline.
The separate KuRue/nfsu2-android repository is a historical wrapper/handoff project.

Based on [codepdbh/nfsu2-recompiled](https://github.com/codepdbh/nfsu2-recompiled),
[pcrecomp](https://github.com/sp00nznet/pcrecomp), and
[DXVK Native](https://github.com/Joshua-Ashton/dxvk-native). Native free roam uses
[th0mk/nfsu2-web](https://github.com/th0mk/nfsu2-web). Thanks also to
[manuelpllull](https://github.com/manuelpllull) for R36S/widescreen work.

Keep the repository and dependency license notices. The project's own tooling is
MIT-licensed; dependencies retain their own licenses. EA owns the game and its
associated assets/trademarks. Supply your own game files; no original game binaries,
music, models or converted texture packs are distributed as repository inputs.

Historical architecture, experiments and development reports remain accessible
through the [documentation index](docs/README.md), including the
[previous README](docs/archive/README-before-v0.14.14-docs.md).
