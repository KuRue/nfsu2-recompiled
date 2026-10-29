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

## Phone force feedback — 2026-10-26

User requested game force feedback through the phone's vibration motor. Added
native DirectInput force capabilities and effect objects, magnitude/gain/duration/
repeat/envelope handling, device gain/pause/actuator commands and a maximum128
live effects. A neutral virtual joystick provides the endpoint without a physical
pad too. Existing physical input and touch visibility remain separate. Condition
forces (spring/damper) create no motor output; constant/ramp/periodic forces map
to scalar vibration, not directional torque or waveform frequency reproduction.
No artificial collision/engine signals. Normal VIBRATE manifest permission added.

Java PhoneFeedback targets the handset default motor, polls25Hz and uses finite
65ms pulses scaled to Light25/Medium50(default)/Strong100, plus Off. Lifecycle
pause cancels the motor AND stops active guest effects. Missing motor disables
capability. Added launcher Test phone vibration180ms pulse. Full details:
docs/ANDROID_PHONE_FEEDBACK.md. Packet/duration mixer regression tests PASS;
startup guest-pointer/partial/envelope/malformed parameter checks PASS; APK build
PASS (18s) and signature verification PASS. Version0.14.10-phone-dev-haptics APK
SHA256 d53469d979d665bfae74b1e15150c83e028f2f2b00acd45d5e97ba3fee2d4911.

Old phone port37329 disconnected; user supplied192.168.1.169:33001. Installed
same dev package there, retained all graphics/HUD/control prefs. User physically
confirmed the test pulse (Yes). Game process2361 subsequently created/started
real sine/square/spring force objects and updated their parameters; PhoneFeedback
logged motor amplitudes19 and127 at Medium. Samsung vibrator_manager recorded
65ms game-use pulses at0.07 and0.50 amplitude under the correct app UID10698,
including explicit cancellation and supersession. This verifies the real game-
effect-to-Android-service path, beyond the launcher test pulse. Evidence (ignored):
build/verification/haptics-phone-{logcat,vibrator}.txt. Did not remotely drive the
phone after game launch. User can tune Phone vibration in the launcher next launch.

While phone was disconnected, booted s25u_sim and copied a separate base data
clone for testing. Its vibration service accepted test pulse; startup guest force
parameter checks passed and game reached title. No phone-feel inference from
emulator. Emulator disappeared before final phone reconnection; rebooted it solely for
cleanup and restored its MOVIES test backup from MOVIES_test_hold. Emulator is
shut down at task completion.
No task server or persistent logcat/watch process was started.

## Haptics follow-up: short impacts and steering grip cues — 2026-10-26

User could feel sidewalks but not grip or car/wall hits. Previous implementation
muted all condition effects and sampled only currently active effects25Hz. Added
a bounded32-entry transient queue retaining game force peaks85ms, so very brief
forces can survive UI polling and effect Stop/Unload/release. Device StopAll,
pause/unacquire and gain mute still silence output. Individual effect Stop can
leave its already-triggered85ms cue; this is a deliberate vibration adaptation.

Parsed multi-axis DICONDITION coefficient/saturation payloads with checked sizes.
Light cornering feedback uses these condition forces and physical steering
magnitude above a dead zone. Resistance changes also produce brief cues. This
approximates steering load, not actual tire-slip telemetry, and cannot guarantee
an impact signal if the game sends no corresponding force. Kept Medium strength,
all prior phone/graphics/controller settings and data. Startup logs on process7741
confirmed a real damper condition with force7058; previous build discarded it.

Extended force tests PASS:5ms effects retained across40ms polling/object deletion,
cue expiry, StopAll/gain silence, grip at nonzero steering and resistance-change
cues, bounded queue; original timing/gain/envelope tests pass too. Startup parser
checks now include2-axis conditions and truncated condition rejection. Android
build PASS18s, APK signature PASS. Installed same dev package at192.168.1.169:33001,
version0.14.10-phone-dev-haptics2. SHA256:
f0ae39799d4063bcc385dbe792c9abd708542ce6b4f88d3eee72ad70f814d7ff.
Asked user to compare real wall/car impacts and corners. No subjective confirmation
yet; record separately if received. No task servers/emulators/watchers started.

User confirmed wall/car impacts work after short-pulse retention, but couldn't
separate hits from sidewalk rumble; traction felt unchanged. Telemetry confirmed
condition output was very weak (amplitude3–4 at Medium). Increased its nonlinear
steering response while retaining a centered dead zone. Added distinct priority
patterns for strong force peaks: quiet30ms, tap75ms, gap60ms, tap75ms, quiet10ms.
Background force output is suppressed for this250ms pattern. Repeated updates
strengthen rather than restart the pattern; strong-curb peaks may also trigger it
because the game force stream carries no explicit collision label. Unit tests
verify road hum is suppressed in the gaps, two pulses occur, hum resumes, and
retriggering doesn't starve the onset. Final APK hash above is for this build.
Installed and launched, physical distinction/stronger-cornering feel not yet
confirmed. No claim of actual tire-slip feedback. Evidence from initial updated
phone run: build/verification/haptics2-phone-logcat.txt. No task background jobs.

## PS2 rumble investigation — 2026-10-26

User says final haptic tuning still feels unchanged; asks to examine PS2 behavior.
Reviewed PCSX2's actual DualShock2 motor decoding and a PC NFSU2 telemetry/custom
FFB reference. Research notes and pinned sources:
docs/HAPTICS_PS2_INVESTIGATION.md. PS2 protocol has large low-frequency byte motor
and small high-frequency binary motor; NFSU2's specific PS2 event logic remains
unknown without its executable or motor trace. No PS2 game image obtained.
Current cornering cue is still steering load, not tire slip. Recommend validating
read-only vehicle telemetry/collision signals and preserving separate haptic
channels before phone synthesis. Candidate external offsets are not validated;
no changes to APK or phone in this research turn, and no false claim of PS2 parity.
No processes/devices/services started. This supersedes optimistic assumptions
about the perceptual effectiveness of the prior pattern tuning.

## Vehicle physics feedback implementation — 2026-10-26

User authorized work on telemetry-driven haptics after steering-force patterns
felt unchanged. Added a checked read-only physics reader for the pinned default
build. Reads run on the guest-owning Present thread; Java never dereferences live
guest data. Probe validates pointer bounds/overflow, finite/ranged fields and
orthogonal body axes. User completed straight, stationary steering, slide, curb,
wall test scenarios.343 captured4Hz samples show coherent player pose/velocity,
wheel speed, suspension, RPM and pedal data. Radius median0.2958 in108 coherent
straight samples, slide angles up to49.14°, stopped velocity/wheels zero; peak
coarse horizontal acceleration~134m/s². Handbrake candidate stayedzero and is not
used. Full research, algorithms and limitations: docs/ANDROID_PHYSICS_HAPTICS.md.
Raw initial capture: build/verification/physics-probe-phone.txt (ignored).

Implemented independent slip estimate (body lateral angle / axle wheel surface
speed mismatch), softer suspension-change surface channel and planar impulse
impact estimate. Slip does not use steering input. Gate low speeds, vertical
motion, pauses/frozen race clock, invalid/stale data, car/race changes, long frame
gaps and pose discontinuities. Coast/straight wheel-radius calibration reduces
per-car mismatch. No collision-event tag or direct tire-force/contact field has
been established: hard landings/extreme maneuvers can still confuse estimates.
Quick-race validation does not prove all free-roam/replay/camera/other-car modes.

Java feedback packets carry amplitude/channel/impact id. Each new impact sends
one finite Android waveform (15ms quiet,65ms strong,45ms quiet,35ms softer), with
210ms priority over other channels; repeated polls don't restart the waveform.
Slip has pulsed chatter; surface stays quieter. New launcher Phone feedback source
(default Vehicle physics · slip estimates); previous Game force effects is kept
for comparison. Medium strength and all existing user preferences preserved.
Unsupported f4bb0d99 build must use legacy mapping until its layout is validated.

Host reader and haptic scenario tests PASS with-Wall-Wextra-Werror: pointer bounds,
invalid axes/numerics, rolling/stopped silence, normal cornering and ordinary
braking vs sliding/lock/spin, suspension without false wall, impact priority/id,
clock freeze, teleports, car changes and calibration. Android assembleDebug PASS
20s, APK signature PASS. Installed at192.168.1.169:33001, same dev package,
version0.14.10-phone-dev-physics. APK SHA256:
1e7f88d493c09a1dfe4ca96f0d78ae05d2c9399b0ac6bfd668d72552143cbad9.

Final live process32198 proves separate channels: straight rolling grip0/surface0,
coast radius~0.299; slide19.26° yields grip147, slide48.93° yields209; curb/surface
channel values remain much lower. Java logged12 physics impact waveforms in the
first~60s capture, including Medium amplitude127. Service evidence and diagnostic
logs: build/verification/physics-haptics-{phone,vibrator}.txt (ignored). These are
output-path observations, not subjective feel confirmation. Asked user to compare
stationary steering, slide, curb and wall in this build; no answer yet. Native
code and Android motor event path validated; no PS2-equivalence claim.
No task server/emulator/watcher/logcat background process started. Leave physical
game available and close only the device stream at completion.


## 2026-10-26: quieter hills after physics-feedback user test

User says the physics build works much better and hits feel good. Traction is
still uncertain, with odd feedback over small hills. Logs in ignored
build/verification/physics-hills-feedback.txt show low-slip straight running
triggering surface levels17–33, often with ~1% wheel mismatch. Changed only the
surface estimator: subtract a per-wheel120ms derivative trend, then threshold
residual at.35 and scale over1.2. Slow heave/pitch/weight transfer should be quiet;
sharp curbs remain. Impact and slip algorithms unchanged. Host regression includes
smooth1Hz suspension cycles versus sharp curb changes; all tests pass. Subjective
hill-filter validation pending. No claim of direct tire-force measurements.

## 2026-10-26: phone performance measurement pass

User confirms hill filter feels better; leave haptics at this setting. User approved
next performance pass. Baseline16 race-like windows (>30k calls) show57.6–60FPS,
median59.35, graphics bridge median3.725ms; current thermal status0. This is not a
sustained thermal result. Added bounded120-frame presentation interval percentiles
and late count, plus lifecycle-scoped10s Android thermal/battery/charging logs.
No preset changes justified yet; no claim of a speedup. docs/ANDROID_PHONE_PERFORMANCE.md
explains evidence and limitations. Host quantile regression passed. Requested a
~5minute normal race after installation; physical sustained validation pending.

Performance build47ee486 installed successfully (signature verified). Launched
PLAY, confirmed thermal log status0/battery32.1C/unplugged and frame quantile logs
in PID29828. Initial loading window includes1s stalls (expected mixed startup);
next window p50=16.67,p95=16.76,p99=19.86,max43.71ms. These are startup/menu data,
not sustained race validation. No capture background processes started.

## Sustained performance result

User replied done. Pulled PID29828 logs10:20:21–10:26:19 into ignored
build/verification/performance-sustained.txt.116 moving-car race-like windows:
median59.6FPS, minimum56.9; median window p95 17.265ms/p99 18.79ms;
11/13920 intervals>25ms, maximum41.51ms. Early/late thirds both59.6FPS,
p95/p99 effectively stable. All37 thermal samples status0, unplugged;
battery32.1→33.3C. After-run thermalservice status0, skin36.8C/AP43.5C.
Kept Balanced75%/60 and buffers unchanged: no sustained degradation observed.
Full methodology and caveats in docs/ANDROID_PHONE_PERFORMANCE.md. Performance
pass complete for this phone/run; occasional brief hitches remain. No new build,
background process, device stream or emulator started for result collection.

## 2026-10-26: fork-aware manual update workflow

Added scripts/phone_upstream.py (fetch/report; optional isolated review worktree,
no merge/push) and build_phone_update.py (clean-source versioned local ARM64 build,
pinned package/certificate validation, APK/hash/dependency manifest archive; no
install/publish). Config pins existing installed development certificate. In-app
debug checks and release notes now use KuRue fork, separate cache, numeric release
tags with known installed suffix handling; debug links go to release page for
review. Production source unchanged. Default dev version26/0.14.11. README marks
upstream downloads; docs/ANDROID_UPDATES.md explains prerequisites, signing backup,
save preservation, qualified downgrade and manual publication boundary. No fork
releases published; gh confirms zero. Upstream a8c7a39 has no unmerged commits.
Java version-comparison regression and Python compile checks passed. No game data,
keys or lifted C added. Next: run full artifact script and install/smoke-check it.

Update workflow validation complete: initial APK smoke check caught invalid slash
in SharedPreferences filename; fixed in d810f05, reinstalled without clearing data.
Launcher manual check now explicitly reports no newer release in KuRue fork.
Game launched PID8361 and presents frames. English/Balanced/physics/HD packs still
selected/detected. nfsu2_game.xml hash stayed5923cb91… through verification and
launch. Existing installed code25 APK pulled before update into ignored
build/phone-artifacts/25-installed-before-updater for rollback. New installed
code26/0.14.11 APK a19bb2d99ac8ee63e526ca30034eadc04b30c42de680c34663e57ccbe20646c2.
Final archive script3347ee8 fixes prepared DXVK metadata (uses PINNED_REVISION and
ANDROID_PREPARE_SHA256 rather than accidentally resolving parent Git). Full script
run passed, archive build/phone-artifacts/26-3347ee895de6. No releases published,
no game/key assets committed. No task background processes or emulator started.

## 2026-10-26: signing recovery, fork homepage and source CI

User authorized local signing backup, homepage cleanup and automated checks.
Created verified local key backup ~/.local/share/nfsu2-signing-backup/
phone-debug.keystore (0700 directory/0600 file), checked exact bytes and exported
certificate against pinned phone signer. Original key now0600 too. No key uploaded;
same-machine backup only, owner still needs independent encrypted backup.
README now leads with this fork's features/channel/build guides/limits; no public
release claimed and upstream APKs no longer presented as our downloads. Added
standalone source-check runner and read-only pinned-checkout GitHub workflow;
no game inputs/SDK/keys required, no APK or release publication. Maintenance docs
explain coverage and backup boundary. Next verify local suite and GitHub run,
and switch fork default branch to phone branch so homepage reflects this work.

Verified GitHub source CI run40343408201 succeeded on clean Ubuntu24.04/JDK17,
including all eight C++ and Java regressions, with no game/SDK/key prerequisites.
Fork default branch and description confirmed via gh. No APK rebuild needed for
README/CI and whitespace-only braces. No background process/device cleanup needed.

## 2026-10-26: Backbone free-roam and face-button correction

User reports X look-back/Y camera and inability to enter races/garages. Game's
DirectInput face mapping bypassed intended keyboard mapping. Phone driving now
explicitly sends X=C(camera), Y=Enter(free-roam accept), R3=B(look-back), and masks
only those three joystick buttons while driving to avoid duplicate actions. Menus
retain joystick defaults; triggers/A/B untouched. Refresh now publishes updated
pad state on mode changes, including held buttons. Host suite passes with added
coverage for override/duplicate suppression/menu transitions/release in DI and
keyboard modes. Default version27/0.14.12. Physical Backbone test pending.

Controller build27/0.14.12 installed successfully, package/signer verified by
archive workflow. Local source suite and targeted refreshed mapping regression
pass; GitHub run40344204827 success. Launched app after install. User must confirm
Y enters a free-roam race/garage, X camera and R3 look-back with physical Backbone.
No claim of physical confirmation yet. No task background processes/emulator.

## 2026-10-26: repair unfinished live-resume implementation

Recovered failed provider changes after user requested exact snapshot resume.
Fixed malformed worker code and unsafe persist() that discarded ownership while
thread still referenced an activity-local object. Worker now process-lifetime;
start idempotent across hosts. Launcher no longer kills live guest for Resume;
uses singleTask activity, retains foreground service with End session action.
Removed automatic Enter-through-startup fake resume. Killed process is not offered
as restorable based on marker. Restart explicit confirmation; foreground launcher
polls process state every2s. Service failure logged. Version28/0.14.13.

IMPORTANT: this only retains live memory. User's durable snapshot request remains
unimplemented; docs/ANDROID_SESSION_RESUME.md identifies native continuations and
host graphics/audio/file/thread resources requiring serialization. Do not claim
survival of process death/reboot/update. User was explicitly informed in commentary.

Built/signature verified/installed preliminary code28 on phone45041. Live
Home→launcher→Resume kept PID31233 and one CRT startup; rendering/audio resumed.
Killed only that test-started PID; stale Resume correctly says session ended,
then PLAY. Mid-race exact-position physical validation still pending. No career
data restored/overwritten. Added process-worker regression; standalone header
needed explicit cstdint. Final build pending latest UI poll/header changes.

Final source suite passes including new worker lifetime/idempotence regression;
Android assembleDebug succeeds and signer matches pinned phone certificate.
Final APK installed after 2s launcher-status refresh. Kept current graphics/user
preferences. No persistent capture process/emulator started. Durable snapshot work
NOT complete; do not relabel live RAM retention as a disk save state.

## 2026-10-26: durable continuation architecture proof

User asked to continue toward exact disk save states. Audited generated call
conventions: RECOMP_CALL uses native C continuations/dummy guest return addresses;
function-local register/flag state must be spilled by a new optional lift backend.
Added isolated experimental/save_states/{snapshot.h,prototype.cpp,README.md} and
scripts/check_snapshot_prototype.py. The handwritten integer fixture uses real
GuestMemory and three explicit frames. Capture process exits mid-leaf (ticks49,
sum3725), fresh restore matches uninterrupted output result71787/ticks100/flags0,
independently checked by arithmetic formula. Codec validates compatibility/CRC,
bounds/overlap/wrap, all truncations, corrupt/version/identity failures; unsupported
host resource capture fails before replacing existing file. Atomic staging/fsync/
rename protocol. CI gets separate proof step. No proprietary inputs or production
runtime behavior changed; no APK/install. Real NFSU2 disk snapshots still NOT done.
Docs/ANDROID_DURABLE_STATE_PROTOTYPE.md records generator next milestone and host
resource inventory (threads/GPU/audio/files/network/time). Do not claim prototype
restores actual races or supports x87/SSE/multiple threads. No background process,
device stream, emulator or game-data mutation started in this prototype turn.

## 2026-10-26: generated integer continuations vs x86 reference

User asked to continue durable save-state work. Added experimental emitter using
existing patched pcrecomp Lifter.lift_instruction for integer semantics, explicit
call/return/tail transitions and per-instruction spilled registers/lazy flags.
Real guest return addresses replace dummy C-call continuation dependence. Original
synthetic assembly7 programs matches Unicorn2.1.4 across8 GPRs/flag mask0xcd5 and
CRC of full1MiB guest window including stack.191 checkpoints captured before every
executed instruction, separate restore process each, match reference. Covers
nested/recursive/register-indirect/tail calls, carry across calls,signed branches,
ret4 cleanup. Malformed/unknown/floating/narrow/segmented/import instructions fail
closed. Actual NFSU2 snapshots still NOT implemented, installedapp unchanged.

Pinned Python dependencies capstone5.0.6/unicorn2.1.4 isolated in ignored
build/continuation-venv. CI separate job prepares public pcrecomp35548b8 + existing14
patches; verified four key source files exactly match local6e85308 and records
SHA256 manifest. No external pcrecomp changes/push, no production lift/backend
changes, no proprietary inputs. No device/game file access/background processes.
Docs experimental/save_states/GENERATED_CONTINUATIONS.md cover model/limits and
next work (FP/SIMD/imports/scheduler/resources). Local first191-checkpoint test passed;
final expanded rejection suite/clean CI pending.

## 2026-10-26: selected FP/SIMD state and synthetic host-call checkpoint

Extended isolated snapshot codec to schema2: raw x87 double/int64-shadow words,
stack depth/control, MMX/XMM/MXCSR and fifth per-frame FPU comparison local. Added
restricted supported FP/vector whitelist in experimental generator, spilling all
extended fields each step. Added x87 stack/arithmetic/comparison-across-call,
exact-int64>2^53, SSE/MMX across-call and control-register fixtures. Added declared
synthetic import service with requested/completed/return phases serialized; fake
service only modifies serialized guest memory. Tests require result74/effectcount2
and exercise both pending phases at fresh-process restoration, without duplicate
logical effects. This does NOT solve external file/network exactly-once behavior.

Final standard suite14 programs/245 dispatch checkpoints passes against Unicorn
(GPRs/flags/full guest-memory CRC plus XMM lanes/control registers). Existing codec
proof passes schema2 including extended bit-pattern roundtrip and invalid metadata,
and its ASan/UBSan run passes. Generated sanitizer suite pending. Schema1 rejects;
format experimental, no installed app state files. Actual game snapshots/imports,
complete x87 precision/alias/exception semantics, scheduler and graphics/audio/file
resource reconstruction remain unfinished. No Android runtime/app/game-data
changes, no device access or background processes. Docs updated with exact limits.

Final generated ASan/UBSan suite passes all245 dispatch checkpoints with fail-fast
sanitizers. It exposed a pinned x86-host unaligned uint64 memory cast; isolated
runner now uses byte-aligned alias access types (as ARM runtime already does),
with strict aliasing disabled. Pinned dependency/production files untouched.
CI now runs both standard and sanitized generated suites. No APK build needed.

## 2026-10-26: checkpointable logical thread/event/timer model

User requested continuing toward durable snapshots. Added isolated logical_scheduler.h,
scheduler_runner.cpp, thread_fixtures.S and scripts/check_logical_scheduler.py.
Complete synthetic process captures per-thread CPU/FP/SIMD images, round-robin
cursor, virtual clock, stable event/timer IDs, handle allocator and wait deadlines.
Synthetic service IDs2/3/4 extend pending host protocol with wait/set/sleep. State
codec now recognizes declared synthetic IDs1..4, unknown IDs still rejected. Shared
CPU harness factored to cpu_test_runtime.h; atomic byte envelope helpers shared.
NFSSCHED/schema1 bundles CPU schema2 capsules with consistent shared-memory copies.

Six scenarios96 dispatcher checkpoints match uninterrupted final process CRC/time,
with independent expected outputs/counters for auto/manual events, one-shot and
periodic timers, timeout and sleep. Different live x87 values survive wait/context
switch; runnable/blocked/sleeping/finished captures all exercised. Permanently
blocked wait captured and clean deadlock reported before/after restore. Graph
checks reject duplicate/wrong IDs, allocator reuse, unknown wait/timer references,
stale deadlines, wrong states, inconsistent memory and recomputed-CRC malformed
metadata. No actual Windows scheduler oracle or production native-thread capture.

New unaligned-double fixture exposed direct raw floating casts in pinned lifter;
experimental emitter now substitutes byte-aligned floating aliases, keeping data
unaligned for regression. Production/pinned source unchanged. Prior245 CPU tests
and codec sanitizer regression passed before final normalization; final clean CI
will rerun. New96 scheduler proof passes standard and fail-fast ASan/UBSan locally.
CI gains scheduler sanitizer step. Docs LOGICAL_SCHEDULER.md describe limits.
No device/game-data access, Android rebuild/install, background server or emulator.
Actual game snapshots still unfinished: mutexes/critical sections, callbacks,
files/audio/D3D resources, heap/protection metadata and coordinated capture remain.

## 2026-10-26: read-only file resource reconstruction proof

Experimental `file_resources.h`/`file_runner.cpp` save logical file handles (ID, safe relative
path, position, content) instead of descriptors. Fresh-process restore with shifted descriptor
layout reopens via `openat`+`O_NOFOLLOW`, validates content, restores independent positions
(mid/EOF) and seek. 206 malformed states/changed or symlinked resources rejected. Standard and
fail-fast ASan/UBSan pass locally; CI step added. Limits in FILE_RESOURCES.md: read-only,
64 KiB, no write journaling/mmaps/real game file layer. No device/game data/APK changes.

## 2026-10-26: logical mutex/critical-section ownership proof

Scheduler snapshot schema 2 adds mutexes (owner, recursion, abandoned) with acquire/release
synthetic services: recursive entry, timed/polled contention, FIFO-by-creation handoff, and
abandonment (0x80) on owner exit. Three new scenarios bring the proof to 9 scenarios/271 fresh-process
checkpoints, standard and ASan/UBSan; existing 245 continuation checkpoints still pass. Graph
validation rejects orphaned/finished owners, recursion mismatch and lost-wakeup waiters. Docs in
LOGICAL_SCHEDULER.md. Production app unchanged; no device/game data used.

## 2026-10-26: writable file handle restore

File resource schema 2 adds writable handles: saved bytes are authoritative, staged next to the
target, published by renameat only after all read-only files validate, then reopened O_RDWR at
the saved offset. 302 bad inputs/resources rejected (symlink/dir/missing targets, mode, handle
content disagreement); failed restores leave writable targets untouched. Standard + sanitizers pass.
Multi-file publish isn't group-atomic; see FILE_RESOURCES.md. Production unchanged.

## 2026-10-26: logical audio mixer state proof

Experimental audio_state.h saves mixer voices (buffer ID+asset CRC, 16.16 position/pitch,
volume, loop, playing) without sample data. 17 block-boundary fresh-process restores render
PCM identical to the uninterrupted run; 157 bad inputs (incl. edited asset) rejected.
Standard + ASan/UBSan pass; CI step added. Real DirectSound/stream decoder/device-queue state
not covered; see AUDIO_STATE.md. Production unchanged.

## 2026-10-26: logical graphics device state proof

Experimental graphics_state.h saves device state (blend/texture/target/scissor/tint), asset
textures by ID+CRC (recreated on restore), and render-target pixels. 15 frame-boundary
fresh-process restores reproduce identical frames incl. persistent RT content; 622 bad inputs
rejected. Standard + sanitizers pass; CI step added. Real D3D9/DXVK state (buffers, shaders,
depth, samplers) not yet covered; see GRAPHICS_STATE.md. Production unchanged.
All standalone subsystem proofs now exist (CPU, scheduler/mutex, files, audio, graphics).
Next: integrate with the actual game (quiesce threads at a frame boundary, hook real APIs).

## 2026-10-26: real-game coverage and hybrid plan

scripts/continuation_coverage.py: 46% of functions/88% of instructions already accepted by the experimental emitter; gaps are known instruction groups. Plan recorded in docs/ANDROID_DURABLE_STATE_PROTOTYPE.md: convert only functions live on stacks at capture points (per-frame Present, worker waits), keep the rest native. Next step needs phone: sample real guest stacks at those points. Phone (adb) not connected at this time.

## 2026-10-27: emitter covers the phone's live functions
- The stack probe ran on the phone. See the durable-state prototype doc for the results.
- The emitter covers 511 of the 521 live functions and passes 19 programs and 330 checkpoints, including under the sanitizers.
- Decided: career and profile files are never rolled back on restore.
- Step 2 in progress: a `-Pnfs.resumable=ON` build records resumable frames and logs capture feasibility (`NFSResumable`). It needs a phone session; after that, implement resume labels and the real capture/restore.

## 2026-10-27: in-process main-thread rebuild passed
Phone session on the resumable dev build with `debug.nfs.rebuild=3`. Main thread rebuilt at Present 64 times in 189.3 s (3.005 s average, no skipped interval), 4 frames each, same game pid, then kept presenting 78 s more. No Resume failed, tombstone, or restart. Present 26,869/26,869 capturable. Background misses were short: EnterCriticalSection `0x57d8c0` (3) and mutex waits `0x481fd0` (213), `0x46ae00` (27), `0x463c80` (6), plus DLL `0x1102f024` (5). Those four executable functions were not in the resumable list during that session. Controller layout was not played. Stress property cleared and the dev process stopped afterward. Details in docs/ANDROID_DURABLE_STATE_PROTOTYPE.md.

## 2026-10-27: one-shot capture file, restore still later

The four executable blockers (`0x463c80`, `0x46ae00`, `0x481fd0`, `0x57d8c0`) are in `docs/data/resumable-functions.txt` (539 addresses, 20 new call sites). DLL `0x1102f024` stays out. At Present, `debug.nfs.capture=1` waits until every other live thread is parked, then writes one `NFSCAP1` file under `<game root>/captures/`. The park is recorded before the machine lock drops, including network `released()`. Timer idle and pre-start suspend are outside guest code and have no frames. The file holds registers, resumable frames, logical events/mutexes/timers, and committed guest memory. Graphics, audio, heap metadata, and file contents are marked omitted and a non-zero `careerRollback` is rejected. Career files are not opened. There is no restore.

`scripts/check_phone_source.py` covers the format and the quiesce rule. `guest_runtime.cpp` compiles with the NDK with `NFS_RESUMABLE` on and off. `assembleDebug -Pnfs.resumable=ON` succeeded in 5m 46s; the debug APK is `android/app/build/outputs/apk/debug/app-debug.apk` (134,317,528 bytes, 2026-10-27 15:05 UTC). The four functions are rewritten in that build. `192.168.4.2:5555` refused the connection, so this APK is not installed and no phone capture exists. `debug.nfs.rebuild` was not turned on. Controller check is still outstanding.

## 2026-10-27: frame pace, video steps, benchmark — not run on the phone

The phone was away. No install, no adb, no capture restore.

Frame pacing keeps its grid when a frame is late by less than one period, and starts a new grid only when a full period has been missed. Waits under 1.5 ms are skipped on device so a short sleep cannot blow the slot. Steady pace (the new default) locks the cap to whole refreshes: 90 FPS on a 120 Hz panel is presented at 60. Exact pace keeps the selected cap. The log now includes jitter, the mean absolute deviation from the median.

Neural upscaling and frame generation are not in the build. Adreno Neural Fusion (super resolution and 2× frame generation) needs Snapdragon 8 Elite Gen 6 or newer, plus depth, motion vectors, and camera jitter. This phone is Adreno 830, and the Direct3D 9 game does not produce that motion data. FSR 4 and Arm's neural upscalers are other GPUs. A color-only spatial upscaler was not wired: it would replace the compositor scale with an untested full-screen pass. The graphics screen says this. Sharpening remains the existing four-neighbor present filter.

New launcher steps the runtime already honors: 85% and 60% internal resolution, 40 FPS, and Steady/Exact pace. Run a benchmark starts a fresh session, waits for three frames of at least 30,000 D3D calls (the October 26 race-like heuristic), records 20 seconds, writes `benchmark-report.txt` in the app files directory, and returns to the launcher. Apply is the only way the suggestion is stored. Nothing here was measured on the phone.

## 2026-10-27: display follows the paced cap — not run on the phone

The launch path no longer pins the fastest mode up to 120 Hz. The paced rate is passed to the native window as a fixed-source frame rate, so a multiple such as 120 Hz is not an acceptable substitute for 60. The window's preferred refresh is the slowest mode that repeats that rate evenly: 30 and 60 fps prefer 60 Hz when it exists, 40 fps stays on 120 Hz because 40 does not divide 60, and steady 90 becomes a 60 Hz request. Android 15 touch boost is turned off so a finger on the screen does not force 120 Hz. Uncapped still prefers the fastest mode at or under 120. The pacer keeps using the panel's fastest rate to size its grid; a 60 Hz mode and two 120 Hz refreshes are the same 16.7 ms. Samsung Game Booster can ignore the request. No phone session, so the active mode is unchecked.

## 2026-10-27: performance hint for the guest thread — not run on the phone

While a frame cap is set, the presenting guest thread reports its busy time to Android's performance hint and asks for power-efficient scheduling. The target is the paced period, the same length the frame pacer waits. The sleep that fills the rest of the frame is not counted, and the report runs after the machine lock is released for that wait. A gap longer than four periods is a pause and is not reported. Uncapped closes the session. The hint API needs Android 13, and the efficiency preference needs Android 15. This binary is built for API 29, so the entry points are loaded at runtime. Only the presenting thread is in the session. No phone session, so nothing shows that clocks fell or that frames still held.

## 2026-10-27: Wi-Fi multicast lock follows LAN sockets — not run on the phone

The lock used to be acquired for the whole time the game was in front, so Wi-Fi power save stayed off during solo play. It is now held only while a datagram socket has broadcast enabled or is bound to UDP port 9999, which is how LAN games announce themselves. Pause still releases it, and resume acquires it again only if such a socket is still open. The configured EA Nation host does not acquire it. Releasing a lock that is not held is skipped. No phone session, so nothing shows that LAN broadcasts still arrive or that the radio sleeps during a solo race.

## 2026-10-27: quiet frontend presents at 30 FPS — not run on the phone

The October 26 sustained log already showed the idle frontend at about 611 D3D calls, presenting at the 60 FPS cap, for most of the minute before a race. The cheap 3D frontend stayed under 2,800 calls. Loading windows were about 9,000 and up. The lightest race window was about 24,000. Eight consecutive frames under 8,000 calls now present at 30 FPS. One frame at or above 8,000 restores the user's cap, and an uncapped race stays uncapped. The game's frame step is set to the same period, so the menu does not run in slow motion. The display request is left on the user's cap, because 30 divides both 60 and 120. The per-frame call count no longer uses the counter that the 120-frame log clears, which had made the benchmark's busy-frame check overflow after the first window. No phone session, so nothing shows that menus landed on 30 or that a race start returned to the cap. A menu window's interval log can sit near 33 ms and those samples count as late against a 60 FPS cap; `menu=` on that line is the count.

## 2026-10-27: phone session, idle frontend at 30 FPS

Installed the debug APK built from `8520994` (resumable off) on SM-S938U1 at `192.168.4.2:5555`. Saved options were uncapped, 100% resolution, 16× filtering, HD packs, night look, reflections on, sharpen off. The idle frontend, about 612 D3D calls, settled at 30.0 FPS: p50 33.33 ms, p95 33.37 ms, jitter 0.01 ms, `menu=120`, `late=0/120`. The log said `Menu pace 30 FPS` and `Performance hint on, target 33333332 ns`. That target is the 30 FPS menu period. A race was not started, so nothing shows the cap returning or the hint closing while uncapped.

The window's preferred refresh was 120 Hz. The panel stayed on the 1440×3120 60 Hz mode, with a 60 Hz frame-rate override for this app. Thermal status stayed 0. The solo menu produced no multicast-lock log. The game was left open on that frontend.

## 2026-10-27: instant self-driving benchmark
The launcher benchmark needs no menus or input. Before the CRT entry the runtime sets the retail game's developer SkipFE flag (`0x864FAC`), which skips the front end and starts a two-lap circuit (red RX-8, no profile loaded) about 4 s after launch. At each Present the player's requested mode (`[0x8900AC]+0x230`) is set to 4, auto pilot; the retail handler for `JOY_EVENT_AUTO_PILOT` (0xB5, `0x603327`) shows the field, but no control is bound to that event. Recording starts when the current mode (`+0x22C`) reads 4 and lasts 60 s. A panel shows current/average FPS, 1% low, worst frame, a frame-time graph, battery temperature, thermal state, panel mode and cap; touch controls and the status box stay hidden. Uncapped runs are judged against the panel refresh. Dev switches: `debug.nfs.skipfe 1`, `debug.nfs.autopilot 1`; `am start ... GameActivity --ei benchmarkSeconds 60` runs one directly.

Phone (S25 Ultra, 100%, uncapped): 6,997 frames, average 116.6 FPS, 1% low 78.0, p99 11.46 ms, worst 27.7 ms, 19 late, advice keep. The panel mode read 60 Hz while the game presented about 120 FPS, so half the frames are not shown; see the 60 Hz frame-rate override noted earlier.

## 2026-10-28: race-load deadlock, static trace so far
Seen once in about 12 benchmark loads (2026-10-27). Thread 1 held critical section `0x864F00` and waited for `0x10806348`; thread 6 the reverse. `0x864F00` is the "bFile System" lock: object `0x864EFC` (+4), wrappers `0x6F55C7` Initialize, `0x6F55DD` Enter, `0x6F55E8` Leave, `0x6F55D2` Delete. It is taken in `0x579B20`, `0x57C7A0` (calls the pool allocator `0x440E50` and `0x6F7C1B`/`0x6F77BC` while held), `0x57C920`; `0x57CA10` lazily initializes it. `0x10806348` fits an async I/O object's lock at +0x28: `0x6F76F2(handle, callback)` locks it and calls the callback while holding it if the I/O already finished; `0x6F767C` (close) locks it briefly, then the manager lock `[0x8AD610]+0xC`. Callback `0x5755D0` only closes the handle, so it is not the file-system side. Next: reproduce with build `963aba2`, whose stall dump gives each held lock's acquiring return address and each waiter's call site, then find the callback that re-enters the file system. The watchdog now cancels this deadlock after 20 s (it used to hang).

## 2026-10-28: offline save-state work (phone unavailable)
Captures (schema 2) now hold open files (path, access, position; career files flagged and never rolled back), heap bookkeeping, DirectSound objects, the D3D9 object list and initialized critical sections. `debug.nfs.load <path>` loads a capture into the same session (see the "Load within a session" section of docs/ANDROID_DURABLE_STATE_PROTOTYPE.md for checks, order and the phone test). Critical sections now lock on their guest fields. `debug.nfs.d3dinventory 1` logs D3D9 objects by kind and pool, plus recorded/derived creation counts, to decide how a cross-session restore handles graphics. None of this has run on the phone yet. The host runtime check (tests/android_runtime) no longer compiles (widescreen and AAudio code lacks host guards); the graphics host checks pass and tests/android_graphics/run.sh now runs both.

## 2026-10-28: host runtime check restored (phone unavailable)

The portable runtime now links the real widescreen implementation, includes its declarations
without a graphics backend, and keeps AAudio pause/resume inside the Android guard. Host-only
unused callbacks are explicitly marked; warnings remain errors. The host entry test exercises
pause/resume before loading the game.

Passed `tests/android_runtime/run.sh /home/ubuntu/projects/nfsu2-game/SPEED2.EXE` with SDK CMake
on PATH: actual lifted CRT reached the expected Direct3DCreate9 HLE boundary. Startup checks
passed 100 scheduler handovers (integer/FS/x87/XMM/TLS/last-error isolation), critical-section
recursion/owner/guards/deferred retirement/reinitialization, heaps, input, sound caps, timers,
FILETIME and generated callbacks. `scripts/check_phone_source.py` and normal
`bash android/gradlew assembleDebug` (run from android/) passed too. No phone install or
snapshot load was performed. Cross-thread lock contention and same-session capture loading
remain unverified by this startup smoke test. Next offline work: capture/restore per-thread
TLS and last-error state; graphics reconstruction still needs the phone inventory.

## 2026-10-28: capture/restore thread-local state (phone unavailable)

Schema 3 appends the 1,088 process TLS allocation flags plus all slot values and lastError for
every captured thread. Park records copy locals before releasing the machine. The presenting
thread restores locals immediately; workers consume pending locals under the machine lock,
even when idle outside guest code. Socket waits now take pending rewinds on lock reacquisition.
A further capture/load is blocked until pending workers have consumed the previous load, so
stale park records cannot be recaptured. Exiting threads clear pending entries. Schemas 1/2
still decode for inspection but are refused by the loader before changing state.

Passed format round trips (different main/worker/timer locals, first/last slots), schema 1/2
compatibility, valid-checksum invalid TLS flags/truncated values, and ASan/UBSan. Host real-CRT
startup passed, including deliberately clobbering then restoring both threads' complete TLS
arrays/last-error and checking isolation over 100 handovers. Phone source checks passed.
`bash gradlew assembleDebug -Pnfs.resumable=ON` passed from android/ (18 seconds on final run).
No phone install/load performed. Next: exercise capture/load during a race when the phone
returns, and collect D3D inventory for cross-session graphics reconstruction. The debug load
still requires the same build/executable, thread roles and graphics object list; full exact
cross-session Resume is not ready.

## 2026-10-28: immutable D3D9 program reconstruction helper (offline)

The bridge now records shader definitions via native GetFunction and declarations via
GetDeclaration, rather than scanning guest memory for the first END. Shader comments and
SM1 constant values may contain that bit pattern. Definitions are size-bounded, checked for
complete token spans/sentinels, and copied independently of guest input/output pointers;
those transient arguments are cleared in the creation recipe. Recording failures log rather
than breaking the successful game Create call.

New d3d9_program_state.h recreates VS/PS/declarations using aligned driver input and an owned
staging object: failure/abandonment releases it, while a later full restore can transfer it.
This helper is NOT wired into the loader or serialized in NFSCAP1 yet. No graphics contents
or device bindings are restored by this change. The old design assumption that default-pool
render targets/dynamic buffers can be recreated empty is now explicitly marked unproven.

The graphics test script runs a new mock-COM program test alongside memory and lifetime checks.
Checks cover shader models 1/2/3, END in comments/constants, ps_1_4 instruction lengths,
declarations, bad sizes/truncation, failed/null driver outputs, pointer removal and ownership.
ASan/UBSan and the phone source checks pass. The resumable Android debug build passes.
No phone install, real-DXVK program recreation, race or snapshot load was performed.
Next offline step: serialize program recipes with proxy identity/reference metadata, then
capture/recreate texture and buffer contents and derived surfaces before replaying device state.
Cross-session exact Resume is still unfinished.

## 2026-10-28: immutable program definitions persisted in schema 4 (offline)

Captures now contain all VS/PS/declaration definitions with guest proxy/table/device identities
and guest reference counts. The bridge exports sorted complete records and refuses capture
if a program recipe is missing. Shared portable graphics_program.h validates definitions without
D3D headers. Decode bounds counts/bytes before allocation, then checks complete matching program
coverage, device dependencies and valid aligned proxy metadata. Schema 1–3 remains readable
for inspection but live loading requires schema-4 program metadata. Same-session loading also
requires unchanged program definitions/proxy metadata/reference counts; it does not replace
native graphics objects yet.

Format tests cover round trips, legacy decoding and valid-checksum malformed program sections.
Graphics tests cover actual bridge export -> NFSCAP1 encode/decode -> mock native recreation.
The graphics script also exits the writer and recreates all three program kinds in a new reader
process; ASan/UBSan runs pass. Format ASan/UBSan, phone source checks, real-CRT host startup and
`bash gradlew assembleDebug -Pnfs.resumable=ON` pass (19s on final build). No phone install,
real GPU recreation or race capture/load performed. Next: texture/buffer content capture and
recreation, derived surfaces and device state, then commit the complete native object graph
while retaining captured guest proxies. Exact cross-session Resume remains unfinished.

## 2026-10-28: VB/IB contents captured and restored, schema 5 (offline)

The resumable bridge shadows successful writable buffer uploads, merges partial ranges and
invalidates old contents on DISCARD. Failed Unlock, GPU ProcessVertices writes and default-pool
Reset invalidate shadows; retirement frees them. Normal builds retain no shadows. Capture
refuses active locks and write-only buffers without complete initialized bytes; readable
buffers use a READONLY fallback. This conservative full-allocation rule can block captures
of partially filled dynamic buffers; phone measurement is still needed. Limits are 64 MiB per
buffer and 256 MiB aggregate contents/shadows; the overall capture size cap still applies.

Schema 5 stores all vertex/index buffer metadata, guest identities/references and bytes.
Legacy schemas decode for inspection but cannot live-load without buffer data. Live-load
preflight checks native descriptors, identities/references and active locks before replacement;
then restore uploads captured bytes and restores shadows. A failed upload after replacement
begins stops the runtime, with no partial-load rollback. Fresh-native recreation is also
implemented with staged ownership, but not wired into cross-session object-map replacement.

Passed: graphics memory/COM/program/buffer checks; schema round trips and 1–4 compatibility;
valid-checksum malformed buffer metadata/counts/sizes/truncation; fresh-process mock VB/IB
archive reconstruction; ASan/UBSan for format and writer/reader graphics tests; source checks;
actual lifted CRT host startup; resumable debug APK build (8s final run). Buffer tests cover
write-only no-readback, DISCARD/NOOVERWRITE, same-session bytes, failed Unlock, active locks,
GPU-write/reset invalidation, failed recreation/upload cleanup and reference mismatches.
No phone install, real-DXVK buffer restoration or race capture/load performed. Next: texture
contents, derived surfaces and device state, then staged reconstruction of the whole graphics
graph. Cross-session exact Resume remains unfinished.

## 2026-10-28: texture contents captured and restored, schema 6 (offline, continuing Sol's buffer work)
Textures and cube maps now follow the VB/IB pattern: guest-format shadows at Unlock (whole-level locks complete a level, partial locks update complete levels), GPU writes/Reset/failed Unlock invalidate, render-target and depth textures are transient, READONLY readback only when the native format is the game's own (no DXT fallback, no HD pack, not default-pool), otherwise capture refuses. Schema 6 stores them; 1-5 still decode. Same-session load replays levels through the guest lock path so DXT fallback and packs reapply. Also fixed before it shipped: the texture list must be sorted (objects is an unordered_map). Passed: all graphics host checks including the new texture_state (also ASan/UBSan), capture format tests (also ASan/UBSan), source checks, host startup to Direct3DCreate9, normal and resumable APK builds. No phone run. Next: volume textures and standalone surfaces, then device state (render states, bindings, transforms), then cross-session recreation of the whole graphics graph.

## 2026-10-28: device state captured and replayed, schema 7 (offline)
Logs show the game uses no volume textures, offscreen/render-target surfaces or copy calls, so those are left out (a capture refuses unknown kinds through validation). Device state (render/stage/sampler states, textures, transforms, viewport, material, lights, clip planes, scissor, streams, indices, declaration/FVF, shaders, all constants, render targets, depth) is read at Present through the device getters and replayed through the setters on load, mapping HD-pack replacements back to the game's textures. Schema 7 adds it with a presence word; runtime captures always include it. Passed: new device_state mock test (also ASan/UBSan), format tests (also ASan/UBSan), all graphics checks, source checks, host startup, both APK builds. No phone run. Remaining for cross-session restore: recreate the whole graphics graph in a fresh process (device, programs, buffers, textures, derived surfaces, state blocks) and rebind guest proxies to the new natives.

## 2026-10-28: release candidate 0.14.13 prepared (offline)

Normal builds again use the long-tested host-mutex critical sections; only
`-Pnfs.resumable=ON` builds lock on the guest fields that save states need
(8b65a05). Phone source checks, the snapshot prototype and the graphics state
proof pass locally; CI on 8b65a05 was still running when recorded. Both normal
and resumable APKs build.

Release candidate (normal build, save states not included):
android/app/build/outputs/apk/debug/app-debug.apk
versionCode 28, versionName 0.14.13-phone-dev-physics,
SHA-256 9f1e9c1a227ecb77091e9f5927cd84c56ffb2a85846f8c37c600174061f54746.

Draft changelog (first fork release; trim before publishing):
- Controller defaults: Backbone Start/Select mapped (pause and world map)
- Skip intro: movies auto-skip on resume
- Self-driving benchmark with opponents
- Frame pacing improvements
- Deadlock recovery: a rare maxed-settings free-roam lockup now exits cleanly
  to the launcher instead of hanging
- Status box fix
- Earlier fork work: phone presets, English launcher, save backup/restore,
  themed option menus, pause-on-app-switch and RESUME split button

Before release: install the RC on the phone, play one normal race and one
benchmark run, then publish (ask the user first).

## 2026-10-28: release candidate 0.14.13 phone test (partial)

Installed today's RC APK on the SM-S938U1 after checking its local SHA-256
against the recorded 9f1e9c1a…f54746. Version on device: 0.14.13-phone-dev-physics
(versionCode 28, dev package com.nfsu2.androidevolved.dev).

Normal race: PASSED. Quick Race → Circuit → Resort Loop, default game options
(3 laps, forward), Peugeot 206, countdown, race HUD with a running timer.
Throttle and steering respond in the physics log (forward=2.90 m/s under
throttle, lateral=-0.99 while steering, suspension 0.29); presentation
95–104 FPS at 37–42k D3D9 calls/frame on the 120 Hz panel.

Benchmark: COMPLETED. "Run a benchmark" from Graphics and display; logcat
confirms "Benchmark report /data/user/0/com.nfsu2.androidevolved.dev/files/
benchmark-report.txt" written at 17:29:41 (~60 s after start) and the summary
dialog appeared. The numbers were NOT read back: wireless ADB dropped
("No route to host") immediately afterwards and the phone left the network.
Screen-off timeout was raised to 2 h for the session and has not been restored.

Test input notes: the frontend polls key state per frame, so instant
`adb input tap`/keyevent presses are ignored; a held press (~500 ms) works and
real finger taps exceed this. The overlay's top-right icon opens the layout
editor, which swallows touches until CANCEL — avoid it in keyboard-driven tests.

Next: reconnect (port may have rotated), read benchmark-report.txt, restore the
screen timeout, then ask the user before publishing the release.

## 2026-10-29: benchmark results (phone)

Two benchmark runs completed on the RC APK (dev package, vc28,
0.14.13-phone-dev-physics):

- Run A: 117.8 avg FPS, 1% low 56.1, min 22 / max 231; render thread 2.06 ms;
  43,475 calls/frame; 7,071 busy frames; p50/p95/p99 8.41 / 9.66 / 11.02 ms.
- Run B: 116.3 avg FPS, 1% low 73.9, min 20 / max 193; render thread 2.31 ms;
  44,123 calls/frame; 6,980 busy frames; p50/p95/p99 8.51 / 10.18 / 11.98 ms.

Both runs flag "The AI car was stuck for 3 s". That is a borderline threshold:
the report needs >3.0 s below 1 m/s, and the source comment already documents
the course slowing the car under 3 m/s for about 3.3 s every run, so the flag
is deterministic, not an intermittent hang. The figures are therefore slightly
conservative, not broken.

Combined with the earlier normal-race pass (countdown, HUD, throttle and
steering verified, 95–104 FPS at 37–42k calls), both required post-install
plays are done. Screen-off timeout restored to 30 min. Publish decision is
with the user: publish as-is, or tune the stall threshold first.

## 2026-10-29: benchmark stall flag fixed in both layers and verified

The "AI car was stuck" warning was firing every run because the course itself
slows the car under 1 m/s for 3.0-4.4 s. The limit is now 4.5 s in
frame_benchmark.h AND in BenchmarkReport.summary() (the dialog message was
hardcoded at >3 s, so the C++ constant alone changed nothing user-visible).
frame_benchmark.cpp and BenchmarkReportTest.java updated: 4.0 s must not warn,
5.0 s must. Host checks pass (python3 scripts/check_phone_source.py).

Phone verification on the rebuilt RC (SHA-256
6ec21f90ccbf54a725c0f94d48e90dc76126c49c3557cb589bfc730b4d2696d3,
68,618,911 bytes): clean dialog, no warning. Average 117.0 FPS, 1% low 82.1,
min 39 / max 212 FPS, render thread 2.37 ms, 42,715 calls/frame, 7,023 busy
frames, p50/p95/p99/max 8.49 / 9.95 / 11.00 / 25.92 ms, late 14 / 7023.

Build gotcha: incremental packaging can leave a ~65 MB hole in the APK (same
entries, 133 MB vs 68 MB). Delete app/build/intermediates/apk/debug then
rebuild to repack.

Release: ready; waiting for the user's publish approval.

## 2026-10-29: v0.14.13 published

Release: https://github.com/KuRue/nfsu2-recompiled/releases/tag/v0.14.13
Tag v0.14.13 (annotated) -> 7ec05d3 on android/phone-presets.
Asset NFSU2-AndroidEvolved-v0.14.13-arm64.apk, 68,618,911 bytes,
sha256 6ec21f90ccbf54a725c0f94d48e90dc76126c49c3557cb589bfc730b4d2696d3
(digest matches the tested APK; download URL returns HTTP 200).
First fork release, debug-signed dev build (com.nfsu2.androidevolved.dev),
installs beside upstream.

## 2026-10-29: presentation effects — look presets, bloom, dither (branch android/visual-effects)

The final DXVK swap-chain blit now implements four look presets (0 Original,
1 Night balanced, 2 Neon, 3 Filmic), an eight-tap bright-pass bloom (Off /
Subtle / Strong) and ordered dithering, with per-look vignette. All of it is
display-referred and runs after the game's gamma ramp; the game-side night
lighting still follows NFSU2_NATIVE_LOOK > 0.

Plumbing: `native_visual` slots x = sharpen, y = look preset, z = bloom
strength; `NFSU2_NATIVE_BLOOM` sets the new option, and `NFSU2_NATIVE_LOOK`
now carries 0..3 instead of a boolean. Launcher gains "Bloom and glow" and two
more look presets automatically through GameOptions.ALL.

Validation: `prepare.py` re-run against the pinned DXVK native-1.9.2b checkout
(c8dc91f); all 54 built-in shaders regenerate and pass spirv-val for Vulkan 1.1;
phone source checks pass; APK builds (68.6 MB after the packaging repack).

Pending: on-device A/B screenshots and a benchmark delta for Subtle/Strong
bloom. Commit branch android/visual-effects before any merge.

## 2026-10-29: AAudio survives Bluetooth/headphone route changes

Symptom (user): connecting Bluetooth headphones mid-race "froze" the game. Logs
show the platform disconnects the shared AAudio stream on the route change
(`AAudioServiceEndpointPlay: callbackLoop() exiting, result = -899` =
AAUDIO_ERROR_DISCONNECTED) and stops calling the data callback for good; the
port had no error callback and never reopened the stream, so audio died and one
run showed a 1.68 s presentation stall.

Fix: register AAudioStreamBuilder_setErrorCallback (DISCONNECTED / INVALID_STATE
/ TIMEOUT set a flag), then reopen the stream from the guest thread — a cheap
per-present tick (serviceGuestSoundTick) plus the existing 1 s statistics pass as
fallback, throttled to one attempt per 500 ms. Reopened streams log
"AAudio stream reopened after a device change"; mixer/voice state is host-side
and survives.

Verification status: the disconnect was reproduced on the phone (01:19:50) with
the old build. Post-fix route-change test is pending: no A2DP device was
connected during the session (Buds disconnected; A2DP suspended), so toggling
Bluetooth changed no media route. Build with the fix is installed on the phone.

## 2026-10-29: black levels, car reflections, game-side night effects

Complaints: dark areas too bright; cars look matte. Registry dump showed the
game had `g_carenvironmentmapupdatedata=0` (env map enabled but never filled),
and our look presets still lifted shadows. Fixes: force env-map updates with
Reflections; retune Night/Neon grading and add Midnight; new Night effects
option forcing glow/streaks/tinting/particles/crowd/overbright (+motion blur at
level 2); fixed LOOK>1 not enabling night lighting. Phone benchmark after:
Midnight+effects 91.0 avg / 54.7 1% low / 49,370 calls; Night+motion blur 97.8 /
60.0 / 46,439. Screenshots in /tmp/opencode/vis_midnight_*.png and
vis_nightfx2_*.png.

## 2026-10-29: registry audit, Atmosphere option

Audited all 28 saved settings. Added Atmosphere (fog+haze / +rain), forcing
g_fogenable, g_horizonfogenable and g_rainenable. Tested g_bleachbypassenable
and g_depthoffieldenable via a temporary registry edit (screenshots in
/tmp/opencode/vis_cinematic_*.png): not exposed yet, pending the user's taste.
LOD/performance_level forcing showed no measurable change (draw calls within
run variance), so they stay untouched until the game's menu mapping is known.
Docs table records every key and its current behaviour.

## 2026-10-29: Visual treatment + Detail level options

Added "Visual treatment" (bleach bypass; +depth of field at level 2) and
"Detail level" (pins world/car LOD and performance level to 1 or 2). Benchmark
with Detail=Maximum: 87.4 avg / 54.2 1% low / 51,678 draw calls vs 95.5 avg /
49k without it, so the values are being applied. Remaining: confirm which key
the game's Display "Level Of Detail" row writes (expected g_performancelevel,
default 1=Medium) and whether it also writes the world/car LOD keys — needs one
manual change in the game's menu, then a registry diff.

## 2026-10-29: full display mapping + launcher display section

Mapped every Customize Display Settings row to its registry key by changing
values in the game and diffing android_registry.dat (writes flush when leaving
the Display page). LOD is a 4-step preset; the captured bundles are in
docs/ANDROID_HD_GRAPHICS.md. Added a "Game display settings" launcher group:
Detail preset picker (applies the captured bundle to the individual rows) plus
17 individual settings, and the locked mode ignores all g_* writes. Phone went
off Wi-Fi before on-device verification of the new UI; next: reconnect, apply
High, confirm the override log and a benchmark screenshot.

## 2026-10-29: display presets verified on device

High preset from the launcher: override log confirms world 2 / car 1 / shadow 2
/ env 1 / road 2 / perf 3; in-game edits while locked leave android_registry.dat
unchanged. Benchmark with High + Midnight + bloom: 77.8 avg / 37.4 1% low /
55,185 calls (heaviest config). Grading now stacks with presets (individual
rows take precedence over the older bundle switches).

## 2026-10-29: launcher redesign (neon theme, IA, benchmark card)

Underground neon palette, themed dialogs, benchmark card at the top of Visual
settings with last-result summary, shortened labels, Performance preset rename,
concrete display defaults (High), "Game setting" sentinels removed. Benchmark
result dialog re-themed with the neon graph; in-race overlay recoloured.
Verified on device (screenshots in /tmp/opencode/neon_*.png).
Next: in-game Display menu removal/disable (RE), remaining toasts, theme polish.

## 2026-10-29: in-game Display menu removed

The Options carousels are built in SPEED2.EXE code, not in FrontB.lzc. Guest
0x4B6230 is the frontend Options carousel (Audio, Display, Gameplay, Player,
Controls, Trailers, Credits, plus conditional LAN/Online Options); guest
0x4C3000 is the pause-menu carousel (Audio, Display, Gameplay, Player). The
localized labels are case-insensitive hashes; "Display" is 0x4204D527. The
Display screen hash 0x8A006328 occurs only in those two constructors, so
dropping the carousel entry makes the page unreachable.

The Android build compiles the static recompiled C in src/recomp/gen. That
directory is gitignored, so the removal is applied by
scripts/patch_display_menu.py after every lift (wired into RELIFT=1
scripts/rebuild-run.sh and documented in README). It keeps the Display item
constructed for the SEH cleanup state and skips only the append call; the
orphan is 0x48 bytes per screen construction.

Verified in a quick race:
- frontend Options: Audio -> Gameplay -> Player (no Display)
- pause Options: Audio -> Gameplay (no Display)
Screenshots: /tmp/opencode/opt_next1.png, /tmp/opencode/pause_opts2.png.
The launcher stays on the lime-green theme with the amber benchmark accent.

## 2026-10-29: themed dialogs and toasts

Every remaining stock AlertDialog now uses R.style.NeonDialog: graphics cache
reset/ready/error, restart-game confirmation, tilt steering, frame-rate limit,
type-in-the-game, control options, driving style, and save/load layout. The
dialog theme now sets colorControlActivated/Normal/Highlight so checkboxes,
radio buttons and SeekBars are lime instead of Material blue; the programmatic
forms set explicit text and hint colours.

Replaced the five remaining Toast.makeText calls with NeonToast: a small dark
green card with a lime border and the same typography. Verified on device:
cache reset dialog, control options, tilt steering (lime controls), frame-rate
limit, save-layout input, and the no-saved-layouts toast.

## 2026-10-29: compact on-screen driving controls

The racing HUD now shows only steering, pedals, nitro and handbrake. A single
lime-ringed expander at the top-left opens a utility bar with pause, map, SMS,
reset, camera, shift up/down, settings and menu/mode; the Xbox/PlayStation
layout also puts X/Y there while keeping A/B (handbrake/nitro). The bar is
persisted in touch_layouts_v1 as bar.expanded, defaults collapsed, and hides
while the control editor is open. Bar buttons use separate v2...bar.<scan> ids
so old saved layouts cannot scatter them.
Verified on device: collapsed cockpit, expanded bar, pause from the bar.

## 2026-10-29: tablet width cap

The launcher's two-pane content is capped at 1200dp and centred on wide
screens. Phones still use the full width. Tested with a 2560x1600 / 240dpi
display override: the cards no longer stretch edge to edge; the phone layout
was re-checked at 2340x1080 / 450dpi and is unchanged.

## 2026-10-29: dock drag + benchmark panel polish

The control editor now treats the expanded bar as a dock. While editing, the
dock moves below the toolbar and any round utility button can be dragged into
it (hidden until expanded) or out of it (always visible). Docked state and
positions are saved with the normal layout and named layouts; the expander and
steering/pedals stay on screen. Verified pause out -> free button -> save ->
pause back into the dock.

Benchmark live panel: lime border, lime BENCHMARK title, condensed type, dark
green panel, transparent popup background. The result dialog and graph were
already themed.

## Race intro hitch (open)

Normal quick race, uncapped FPS: frame log shows 272 ms and 545 ms frames
during the intro car sequence; the 1 Hz player physics log stays at
forward=0.00, so the visible jump is likely an AI car or animation catch-up.
The benchmark ran uncapped on a 120 Hz panel (late frames 2792/4707). Next:
repeat at 60 FPS / steady pace and trace the guest clock; consider clamping the
guest frame delta or pre-warming shaders/assets before the intro.

### Race intro hitch: root cause

The 549 ms intro frame aligns with the synchronous read of the 118 MB
TRACKS/STREAML4RA.BUN and the IntroNis06 bundle; HD texture installs near the
256 MiB budget add roughly 285 ms. With HD packs off the worst intro frame is
94 ms after a 264 ms base track load. Capping to 60 FPS does not remove it.
Next fix: cache or memory-map large .bun files / prefetch the track bundle
before the intro starts, and consider deferring HD installs while the intro
streams. Settings used for the test (60 FPS, HD off) were restored afterwards.

## 2026-10-29: race asset prefetch + intro measurement

Added a background prefetch for large read-only .bun/.big/.pak/.lzc assets when
the game opens them, plus a persistent `.android_last_track` marker so the last
track bundle is warmed at the next game start. Instrumented run: the 113 MiB
track bundle prefetch completed in 80 ms, HD installs measured 0-44 ms, and no
guest read exceeded 5 ms. The earlier 549-575 ms intro stalls only reproduced
while adb screenrecord was active; an instrumented run without recording had a
50 ms worst intro frame and the player physics log stayed at forward=0.00.
Keep the prefetch for cold starts; if the visual vibration still happens
without recording, capture it phone-side together with the track/car/FPS/HD
settings.

## 2026-10-29: dialog width/typography polish

The NeonDialog theme now uses a 68% minimum width instead of 78%, so alert
dialogs are less oversized on the phone. Added condensed title/body text
appearance styles for the framework dialog list/body where the platform reads
them; the AlertDialog title itself still uses the framework's own style.
Verified the narrower cache-reset dialog and the themed benchmark panel.

## 2026-10-29: reference-based native launcher theme foundations

Added MenuTheme and semantic menu_colors, silver translucent launcher sheets with dark text,
black/lime title bands, gray rail/marker dividers, oblique menu labels, outlined actions and
stateful rows. Value pickers use the same styling, a responsive capped width and Back footer.
Bundled Barlow Condensed Regular/Medium Italic (licensed approximation, not exact game font);
OFL is packaged under assets/licenses. Font roles and colors are ready for subsequent screens.
Main Support row uses Updates/diagnostics instead of a long build string; full version remains
in Support. Settings/navigation otherwise unchanged; no gameplay code changed.

Debug APK installed on 192.168.1.169:40615. Checked home, Visual settings, preset and driver
pickers, selected lime frame, white D-pad focus, D-pad Back activation and submenu navigation.
Build/source checks pass; fonts/license checked in APK; body/status contrast adjusted. No game
options changed. No tablet/large-font or full physical Backbone traversal performed this pass.
Remaining theme work: category-icon navigation/backdrop/logotype, shared framework dialog
migration, touch dock/editor and HUD/benchmark roles. See docs/design/NFSU2_UI_THEME_PLAN.md.

## 2026-10-29: user-refined dark-gray menu palette

User prefers neutral dark-gray background/panels with light-gray copy and green/lime accents,
superseding the previous pale silver-sheet palette. Applied across launcher, shared rows,
headers/actions and value picker resources; warning/error colors remain readable. Removed
all static header/section dots and replaced the custom rail painter with a plain divider.
Dots belong on interactive sliders; existing native SeekBar thumbs remain circular.

Debug build passed and installed on 192.168.1.169:40615. Home screenshot visually checked:
/tmp/nfsu2-dark-gray-launcher.png. Body/status contrast against #343434 is 5.43–9.25:1;
secondary text is 6.70:1. No settings, game logic or slider behavior changed. Palette and
no-header-dot rule are recorded at the top of docs/design/NFSU2_UI_THEME_PLAN.md.

## 2026-10-29: darker silver sheets, distinct file checks, rounded headers

Latest user feedback: the first silver version was closer; all-dark was too far. Restored
medium silver-gray sheets (darker than the original pale ones), keeping the dark-gray backdrop.
Body/value text is dark on the medium sheets. Header backgrounds are now rounded. Game-file
checks have a contrasting dark inset, light-gray labels and colored status glyphs; slightly
larger check text improves separation. Static header bars still have no dots. Latest direction
supersedes the preceding all-dark menu-panel preference; theme plan's top section updated.

Debug build passed and installed on 192.168.1.169:40615. Home screenshot checked at
/tmp/nfsu2-midgray-launcher.png: rounded headers, contrasting checks and Play fit on screen.
Body/secondary contrast on composited sheets is 4.66/4.53:1; file text 10.33:1. No game settings,
input behavior or gameplay code changed. Font remains the licensed approximation from prior pass.

## 2026-10-29: integrated black panel frames and dynamic submenu heading

User requested black borders with titles in them, plus replacing Options with the open
submenu name. MenuTheme now draws rounded black 4dp outer frames around gray sheets.
Title bands join the frame rather than floating inset; content padding is separate. The
same frame/header structure is used by home, left contextual panels and value pickers.
Right header refreshes through a shared group-title mapping (Options at root, current group
inside). Color/font preferences and dot-free dividers retained.

Debug build passed, installed and visually checked on 192.168.1.169:40615. Verified home,
Visual settings and Controls titles, framed vibration picker, Back dismissal and return home.
No game option changed. Screenshots: /tmp/nfsu2-framed-home.png,
/tmp/nfsu2-framed-submenu.png and /tmp/nfsu2-framed-picker.png. No gameplay code changed.
