# Current validation and limits

This page describes **v0.14.14**, published 2026-10-31, and the documentation refresh
after it. See [release notes](releases/v0.14.14.md) for the complete change list.

## Shipped and exercised

- ARM64 debug-channel APK, Android 10+, same package/signature as the prior fork release.
- Native game loading, racing/free roam, graphics and live-session pause/resume.
- Native nfsu2.xyz connection with visible remote names, minimap and world-map markers.
- Actual Present-based FPS display; clean Quit Game return and process teardown.
- Xbox Bluetooth motor detection/routing and user-confirmed simple controller vibration.
- Source, graphics resource/device, networking, haptics, input/cursor and release identity checks.

Phone observations primarily used Samsung SM-S938U1; results are not universal device
certification. Earlier reports contain measurements at different scenes/settings.
The state cache suppressed many repeated setters in a live trace, but no matched
cache-on/off FPS improvement is claimed.

## Needs broader validation

- The newest weak-only sustained traction / strong-only isolated impact balance.
  The user reported earlier tuning improved overall feel but cues remained similar.
- Physical right-stick/R3 access to Quit Game and other mouse-only screens. Regression
  tests cover cursor logic; the requested user check was deferred.
- Other controller models/connections, GPU drivers, sustained performance and cross-client
  race/lobby combinations. Switch reachability does not establish all-mode compatibility.
- Current 32-bit source target parity; v0.14.14 downloads are ARM64 only.

## Not shipped / not claimed

- Durable snapshots after process death: experimental save-state code is off.
- Ray tracing, upscaling or a high-polygon replacement asset pack.
- Measured tire contact forces, perfectly tagged impacts or exact PS2 rumble reproduction.
- Xbox trigger-motor control, arbitrary physical vibration frequency or calibrated 256-level intensity.
- A universal FPS target, elimination of loading hitches, or availability guarantees for external servers.

## Validation boundaries

The release builder verifies package, version and signer; checksums identify files.
The game preflight verifies the supported EXE and folder presence, not every asset.
Standalone tests cover their scenarios, not all gameplay. Driver probes cover queried
capabilities, not full race stability. CPU/GPU demand bars estimate cost, not utilization.

[Index](README.md) · [Troubleshooting](TROUBLESHOOTING.md)
