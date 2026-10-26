# Android phone fork — 2026-10-26

Upstream: codepdbh/nfsu2-recompiled at a8c7a39ce7b75b7a49368445a9a2b7ca5cf6e1f3 (v0.14.10).
Fork: https://github.com/KuRue/nfsu2-recompiled, branch android/phone-presets.
Local checkout: /home/ubuntu/projects/nfsu2-native.

## First changes

- Explicit Battery (50% resolution, 30 FPS), Balanced (75%, 60 FPS), Quality
  (native resolution, 60 FPS) launcher profiles. These are configuration starting
  points, not measured performance guarantees. All use the existing two-buffer
  CPU/GPU setting. Existing defaults and user choices are preserved until selection.
- Custom individual settings remain editable; profile recognition uses the actual
  three settings rather than a stale stored profile name. Other options are preserved.
- Read-only PC-game preflight: `python3 scripts/check_android_game.py /path/to/game`.
  Requires the exact supported executable SHA-256 and the runtime's nonempty folders.

## Prepared locally

- pcrecomp sibling /home/ubuntu/projects/pcrecomp, base 35548b8 with all 14
  patches from patches/pcrecomp applied in order on branch nfsu2-runtime.
- DXVK native c8dc91fabd00cac11d697ccf07426e798393cd40 cloned at
  /home/ubuntu/projects/nfsu2-dxvk-native; prepare.py output in build/dxvk-source.
- 54 built-in shaders generated and validated for Vulkan 1.1.
- libadrenotools and nested submodule checked out at upstream's pinned revisions.

## Verification

- `cd android && bash gradlew --no-daemon --console=plain :app:compileDebugJavaWithJavac`: PASS.
  This compiles the launcher, NOT the complete game APK.
- Host graphics_memory_check and graphics_com_check: PASS. SDK CMake 3.30.5,
  Ninja and GCC 15.2 were used because host clang++ is unavailable. Upstream's
  compact if statements trigger GCC misleading-indentation warnings; host check
  configured with -Wno-error=misleading-indentation (warnings retained).
- No newly built APK installed, no phone settings changed, no gameplay benchmark.
- No background servers/emulators started.

## Blocker and next work

No local SPEED2.EXE found; /sdcard/nfsu2 is absent on the connected physical
phone. Need the user's PC installation. The WebView cache is not used as the
native game's source. Validate the executable before generating local game C.
Do not commit game data, generated C, analysis artifacts or compiled game libraries.

Once supplied: preflight, follow README generation steps (merge vtable/RTTI
address seeds into work/seeds_all.json), lift, build ARM64 debug APK with one
Gradle build at a time, and test a reproducible race for sustained frame times,
heat and audio. Set CMAKE_BUILD_PARALLEL_LEVEL=4 to bound native compilation.
Do not overwrite the release-installed native app without accounting for its
signing certificate: a debug-signed APK cannot update a differently signed app.
Prefer a separate development application ID for initial device testing.

Keep game logic and DXVK initially. Profile D3DX application and guest dispatch
before native rewrites; upstream docs/ANDROID_PERFORMANCE_2026_10_16.md already
identifies these as significant costs. No graphics/asset enhancements are yet
implemented. Quality profile increases resolution only.

## Upstream updates

Keep origin = KuRue fork; upstream = codepdbh source. Fetch upstream and merge a
chosen tag/commit into this branch normally; never force-push. Inspect changed
build dependencies and reapply/review pcrecomp patches against their documented
base if upstream changes them. Rebuild shaders and rerun validation after renderer
updates. Native updates do not automatically inherit the website's features.
