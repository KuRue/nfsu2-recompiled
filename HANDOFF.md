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
