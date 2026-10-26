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


## Website data and first full build — 2026-10-26

The user asked to obtain the data directly from nfsu2.xyz. Its public
/data-manifest and /data/<path> expose the original executable and base assets.
Downloaded 1,295 base files (2,847,509,714 bytes) to
/home/ubuntu/projects/nfsu2-game, excluding LANGPACKS/TEXPACKS. SPEED2.EXE matches
the supported SHA-256; all other files checked by manifest size only. Copied all
to the connected phone at /sdcard/nfsu2 and verified executable SHA-256 there.
The PC-copy blocker above is resolved; no manual copying by the user is needed.
The shell repository has the repeatable downloader native/download-game.py.

Removed SDATA from native and Python preflight checks: absent from the website
manifest and not a required base asset folder. No dummy files/folders added.
Debug builds now use com.nfsu2.androidevolved.dev and the label NFSU2 Native Dev,
so locally signed APKs can coexist with the upstream release. Java compiles and
the merged manifest confirms the development ID and label.

Generation commands run in this checkout:

```bash
python3 -m venv build/venv
build/venv/bin/pip install capstone==5.0.9 pefile==2024.8.26 unicorn==2.1.4 keystone-engine==0.9.2
mkdir -p work
build/venv/bin/python scripts/normalize_exe.py /home/ubuntu/projects/nfsu2-game/SPEED2.EXE work/SPEED2.analysis.exe
build/venv/bin/python ../pcrecomp/tools/cpp/vtable_scan.py work/SPEED2.analysis.exe --seeds work/vtable_seeds.json
build/venv/bin/python ../pcrecomp/tools/cpp/rtti.py work/SPEED2.analysis.exe --seeds work/rtti_seeds.json
```

Merged both seed lists by integer address into work/seeds_all.json (4,438 entries), then:

```bash
build/venv/bin/python -u scripts/run_lift.py catalog work/SPEED2.analysis.exe work/seeds_all.json work/catalog.json
build/venv/bin/python -u scripts/run_lift.py lift work/SPEED2.analysis.exe work/catalog.json src/recomp/gen
cd android
env CMAKE_BUILD_PARALLEL_LEVEL=4 bash gradlew --no-daemon --console=plain --max-workers=2 assembleDebug
```

Catalog: 27,742 functions, 393 seconds. Lift: 70 units, 9,001,374 C lines,
zero lift failures, 131 seconds. Lift statistics still include 2,910
RECOMP_UNIMPL sites (2,890 3dnow, 17 int, 2 override, 1 excluded); zero failures
is not a claim of complete instruction/runtime support. Generated files and
analysis inputs remain ignored and must never be committed.


## Full native APK and phone smoke test completed

- `assembleDebug`: BUILD SUCCESSFUL in 15m 2s (first full compile plus ThinLTO).
- APK: android/app/build/outputs/apk/debug/app-debug.apk, ARM64 only,
  com.nfsu2.androidevolved.dev, version 0.14.10-phone-dev.
- APK SHA-256: 9d298f04aa6bb3655066852f4b6cbb5be1b451402c478e76eebc90d84d60aabe.
- apksigner verify: PASS, v2 signature. aapt confirms development ID/label/ABI.
- User supplied new wireless port 192.168.1.169:37329; reconnected successfully.
- Installed via T3 agent-device alongside upstream native release and web shell.
  Granted its requested all-files permission in Android Settings. Launcher now
  recognizes executable and game folders. Selected Balanced preset, verified
  75% and 60 FPS in launcher; selected English (verified in launcher).
- Native boot on SM-S938U1, Adreno 830, Vulkan API 1.3; initial render resolution
  1756x810. Guest memory checks and actual runtime file checks pass. Intro movies
  render locally and audio diagnostics initially report xruns=0.
- Advanced via native touch controls through title, declined creating a test
  profile, saw 3D main menu/car and a race countdown scene with cars/environment.
  This is a rendering/startup smoke test, not proof of a completed playable race.
- Observed additional live menu/settings changes between checks; stopped remote
  input to avoid interfering. Later logs show an in-game reset to 1280x720;
  do not describe the whole session as fixed at the initial launcher resolution.
- Presentation logs report about 58-60 FPS in intros and 60 FPS in 3D menus.
  Do not call these sustained racing FPS, battery results or a web/native comparison.
- Evidence (local/ignored): build/verification/native-logcat.txt and native-screen.png.
- Game deliberately left available for the user; no emulator, logcat watcher or
  task HTTP server started. Completed Gradle's single-use daemon exits normally.

Next work: controlled racing input and repeatable performance/thermal baseline;
then profile bottlenecks before native rewrites, and evaluate optional graphics
assets separately. No texture/model/renderer quality enhancement has shipped yet.


## English native app UI — 2026-10-26

User requested translating the app itself and assessing visual upgrades. Translated
launcher/settings, phone presets, touch-editor toolbar and dialogs, tilt steering,
text entry, driver import/probe/errors, cache/reset dialogs, crash/report UI and
startup status messages. Preserved preference keys, layout IDs, language-file
identifiers and all user selections. Fresh installs now default game language to
English UK; existing saved language remains respected. Language names remain in
their own languages. Remote upstream release notes keep their supplied language;
`Novedades` in LauncherActivity is a parser marker, not untranslated app UI.

Changed the native Starting game status and Java's status-hide marker together,
so translating startup text does not break the delayed diagnostic overlay hide.
Build suffix: 0.14.10-phone-dev-en; same development application ID/signing key.
Incremental assembleDebug: PASS in 13s. APK signature verification: PASS. Installed
in place via agent-device on 192.168.1.169:37329. Verified English launcher labels,
English Battery/Balanced/Quality preset dialog, retained Balanced + English values,
and native game boot. Controller scan recognizes the user's Backbone One; the
native game correctly hides touch controls. Did not manipulate the controller or
force the touch editor open while the user could be playing; editor/error text
was verified in source and compiled, not exhaustively triggered on-device.

Graphics assessment: docs/ANDROID_GRAPHICS_ROADMAP.md. The web UHUD GPU-replacement
loader is not implemented in this native fork. Texture packs need native
integration/testing; no pack or ray tracing has been enabled by this translation.
Recommend optional HUD/font and environment textures, then measured filtering/
reflection improvements, before custom models or ray-tracing research.
No emulator, task server or log watcher started. Leave the game available to user.

## Optional HD/native visual enhancements — 2026-10-26

User authorized HD textures, lighting without ray tracing, sharpening and better
reflections. Implemented NTXP GPU binding replacements (guest dimensions/readback
unchanged), bounded 256 MiB conservative residency budget, original/HD switch,
4×/8×/16× anisotropic filtering, final-presentation subtle/crisp sharpening and
night grading. The night option also enables existing game car lighting,
headlights, car shadows and glow. Reflections On enables the game's car/road
reflection effects and doubles game-owned mirror dimensions 256→512 and car
cube edge 128→256, guarded for the exact executable. Road target size unchanged.
Registry query/write overrides preserve previously saved options for the Original
mode; all features default off for existing installs. No ray tracing or new models.

The phone had persisted lighting/reflection effects disabled. First HD smoke test
rendered a race, with about 58–60 FPS after loading, but those existing lighting
and reflection effects were still disabled. Added the runtime overrides afterward;
do not conflate this initial sample with the cost of fully enabled effects.

Assets separately obtained/converted, not committed: Upscaled HUD by LunicAura106,
574 larger textures, 101 MiB; New HD Textures Pack v1.1 by I_NEED_SPEED / Azazel,
90 compatible matches (70 larger), 212.6 MiB. Source credits and links retained in
tools/texpacks/SOURCES.md. Base game bundles unchanged. Full mip-chain conversion
preserved, TexMod CRC is zlib CRC32 XOR 0xffffffff, while NTXP keeps standard CRC32
plus Adler32. Alternate PLUS World converter included with AJLethal credit; that
pack has refreshed original-resolution art and is not the installed world pack.

Files installed on phone /sdcard/nfsu2/TEXPACKS/{uhud,world}.pak:
uhud SHA256 2a0eb5adb888f54d341dd429ca089280cacca0a03fdc5191b286133db0ba7df7
world SHA256 302e3b843457293dbb069fba4d600264b9f72a044505c59603b8d90bc3d707c1
Both fully passed native parser/payload validation locally. Final debug APK
0.14.10-phone-dev-hd SHA256:
18ce3ce7fe2f869dfdcf833ed1617fe06613b84621534fd0ac8a09d5f0535913.
Signature verified, installed over the same dev package on 192.168.1.169:37329.
Phone selections: Balanced 75%/60FPS, packs on, AF8, sharpen15, look1, reflections1.
Previous option XML backed up to /tmp/nfsu2-prefs-before-hd.xml; game language and
other settings retained. Backbone controller remains unchanged.

Validation: NTXP host malformed-index/hash/padding/mip tests PASS; existing host
memory/DXT and COM ownership/mode checks PASS; all 54 DXVK shaders compile and
validate Vulkan1.1; incremental APK builds PASS (15–19s), apksigner PASS. Generator
now retains both new binding hooks and preexisting handwritten reset/depth/vertex
hooks; regenerating the device methods exactly matches the checked-in file.
DXVK patch cache signature includes presentation shader/patch source and writes
SPIR-V headers only if changed. See docs/ANDROID_HD_GRAPHICS.md for limitations.

Evidence local/ignored: build/verification/hd-logcat.txt and hd-race.png (initial
race smoke test). No emulator, task server, watcher or persistent logcat started.

Final-effects verification: native process29231 logged all six runtime lighting/
reflection overrides, successfully allocated the 256-edge dynamic RT car cube,
512×512 mirror and original320×240 road targets. Final race screenshot:
build/verification/hd-final-race.png; full logs hd-final-logcat.txt. Short race
sample after loading reported59.8–60.0FPS (roughly30–36k bridge calls/frame).
This is a smoke sample, not a sustained thermal/battery benchmark. Pulled the
saved android_registry.dat after testing and byte-compared it to the earlier
copy: unchanged. No further remote input after launch; leave game running.

## Trigger pedals and widescreen HUD — 2026-10-26

User reported gas/brake only worked on face buttons and requested HUD near screen
edges. Found keyboard fallback was entirely disabled once DirectInput acquired
the pad, leaving unbound Z triggers ineffective; base game also lacked the HUD
DAT, so native widescreen had zero anchor entries. Added default trigger pedal
compatibility while driving, with launcher opt-out for manual analog bindings;
this mode is digital threshold gas/brake, not proportional throttle. Kept raw pad
axes, joystick buttons and steering. Split pure state/mapping from Android event
parsing and added regression tests for acquisition/holds/release/menu isolation.

Bundled 73 ThirteenAG MIT HUD anchors as a native fallback, external custom table
still preferred. Actual aspect drives edge displacement; existing minimap choice
now has anchors even with no DAT. License/source hash recorded in
 docs/ANDROID_CONTROLS_AND_HUD.md. No Windows DLL/ASI installed. Host pad and HUD
checks PASS, incremental Android build PASS (12s), APK signature PASS. HUD test
needs the known preexisting misleading-indentation warning demoted under GCC.

Installed same dev package with version suffix -phone-dev-controls, SHA256:
49c5c98c46bae21cadbdee1e43432e49702d45b1927f936d0da8dc6e3d4c77db.
Launcher verifies built-in HUD and RT/LT compatibility default. Left HD settings,
Balanced preset and all saved game files unchanged. Asked user to physically test
RT/LT because remote ADB cannot squeeze the Backbone's real triggers. No emulator,
server or watcher started. Device verification continues below.

Device smoke test: process7030 reports73 HUD offsets; race screenshot
build/verification/wide-hud-race.png shows map at upper-left edge, leaderboard at
upper-right edge and tachometer at lower-right edge at2340×1080 display /
1756×810 rendering. Backing panels/artwork are present. Short race sample~59–60FPS.
Physical controller callback emitted Controller pedal gas down at23:39:24; no
brake/release confirmation yet. Host checks cover those transitions, but do not
claim a completed physical brake test. Full local log controls-logcat.txt.

User subsequently replied “Working” to the explicit RT gas / LT brake physical
test request: both pedals confirmed. Implementation commit8259eec.
