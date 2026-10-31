# Documentation

Current user guides describe **fork v0.14.14**. Start here rather than following
commands from a dated investigation or the upstream project's old README.

## Playing

- [Installation, updates, saves and Resume](INSTALLATION.md)
- [Controllers, touch controls and vibration](CONTROLS.md)
- [Graphics and optional HD packs](GRAPHICS.md)
- [Multiplayer and network modes](MULTIPLAYER.md)
- [Troubleshooting and reporting issues](TROUBLESHOOTING.md)
- [Validation status and known limits](STATUS.md)
- [v0.14.14 release notes](releases/v0.14.14.md)

## Building and maintenance

- [Build prerequisites, local generation and checks](BUILDING.md)
- [Signed updates and upstream review](ANDROID_UPDATES.md)
- [Branches, release checklist and dependency/signing policy](FORK_MAINTENANCE.md)
- [DXVK Android preparation and third-party notices](../ports/dxvk_android/README.md)
- [Texture conversion tools and asset credits](../tools/texpacks/SOURCES.md)

## Technical records

These preserve evidence and earlier implementation stages. They are not current
setup instructions or promises that an experimental feature ships in the APK.

| Area | Records |
| --- | --- |
| Recompilation and architecture | [Binary report](NFSU2_BINARY_REPORT.md), [lifting](LIFT_REPORT.md), [runtime](ANDROID_RUNTIME.md), [threading](THREADING.md), [function recovery](FUNCTION_RECOVERY.md) |
| Graphics | [Graphics development history](ANDROID_HD_GRAPHICS.md), [D3D9 usage](D3D9_USAGE.md), [DXVK device tests](ANDROID_DXVK_DEVICE_TEST.md) |
| Controls and haptics | [Controls/HUD history](ANDROID_CONTROLS_AND_HUD.md), [physics estimates](ANDROID_PHYSICS_HAPTICS.md), [legacy forces](ANDROID_PHONE_FEEDBACK.md), [controller analysis](HAPTICS_CONTROLLER_ANALYSIS.md), [PS2 investigation](HAPTICS_PS2_INVESTIGATION.md) |
| Measurements | [Phone performance](ANDROID_PHONE_PERFORMANCE.md), [older performance investigation](ANDROID_PERFORMANCE_2026_10_16.md), [audio/pacing](AUDIO_FRAME_PACING.md) |
| Save-state experiments | [Session history](ANDROID_SESSION_RESUME.md), [durable-state prototype](ANDROID_DURABLE_STATE_PROTOTYPE.md), [experimental source](../experimental/save_states/) |
| Design history | [UI](design/NFSU2_UI_THEME_PLAN.md), [graphics settings](design/NFSU2_GRAPHICS_SETTINGS_PLAN.md), [free roam](design/NFSU2_ROAM_INTEGRATION.md), [superseded preview design](design/VISUAL_PREVIEW.md) |

Additional dated tests remain in this directory. The [archive](archive/) contains
superseded README/update/maintenance/status pages. HANDOFF.md at the repository
root is a development log, not the user manual.
