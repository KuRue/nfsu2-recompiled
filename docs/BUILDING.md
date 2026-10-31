# Building and checking the fork

The APK cannot be built from a clean clone alone. Local generated game C and
prepared native dependencies are required. Neither game inputs nor generated C
are committed. These instructions describe the maintained Android path; older
Windows-host investigations remain in the [technical records](README.md).

## Source-only checks

On Linux with Python 3, a C++17 compiler and JDK 17:

```sh
python3 scripts/check_phone_source.py
```

This runs standalone pacing, physics/haptics, input/cursor, networking, resource
and Java regressions. It needs no game data, Android SDK, GPU, signing key or
network service. It does not prove Android UI, races, rumble feel or GPU compatibility.

Graphics bridge/resource tests require the prepared DXVK headers and CMake:

```sh
bash tests/android_graphics/run.sh
```

Put the Android SDK CMake directory on PATH if CMake is not installed system-wide.
That script uses clang++ and builds in a user cache. Experimental state proofs and
additional sanitizer checks are defined in
[the CI workflow](../.github/workflows/phone-source-checks.yml); they do not enable
save states in a release build.

## Android toolchain

The release build machine uses Linux, JDK 17, SDK Platform 35, build-tools 36.0.0,
NDK 28.2.13676358 and CMake 3.30.5. Set `ANDROID_HOME` to your SDK. Shader preparation
also requires Python 3, `glslangValidator` and `spirv-val`. Keep one Gradle build at
a time on memory-limited machines; native compilation can be expensive.

```sh
git clone --branch android/phone-presets --recurse-submodules \
  https://github.com/KuRue/nfsu2-recompiled.git
cd nfsu2-recompiled
git submodule update --init --recursive
python3 -m venv .venv
. .venv/bin/activate
python -m pip install capstone pefile unicorn keystone-engine
```

The canonical branch tracks the current fork. To reproduce a release's source,
check out its tag and update submodules. A matching tag alone does not supply
proprietary inputs, generated sources, compiler caches or a signing key.

## Prepare pcrecomp

Android CMake expects a sibling `../pcrecomp`. Use the pinned baseline and all 14
patches from this repository, not an arbitrary latest checkout:

```sh
git clone https://github.com/sp00nznet/pcrecomp.git ../pcrecomp
git -C ../pcrecomp checkout 35548b8d0fd8aa853da67edcf7cd6134f6b9cdc7
task_repo_root="$PWD"
git -C ../pcrecomp apply "$task_repo_root"/patches/pcrecomp/*.patch
```

Apply once to that clean baseline. Do not reapply over an already patched tree.
The published v0.14.14 build manifest records the prepared dependency identity;
local patch application can produce the same files without the same Git commit.

## Generate game C locally

Use the [supported executable and folder layout](INSTALLATION.md). The preflight
is read-only; normalization writes an analysis copy, preserving the original.

```sh
GAME=/absolute/path/to/your/game
python scripts/check_android_game.py "$GAME"
mkdir -p work
python scripts/normalize_exe.py "$GAME/SPEED2.EXE" work/SPEED2.analysis.exe
python ../pcrecomp/tools/cpp/vtable_scan.py work/SPEED2.analysis.exe --seeds work/vtable_seeds.json
python ../pcrecomp/tools/cpp/rtti.py work/SPEED2.analysis.exe --seeds work/rtti_seeds.json
python - <<'PY'
import json
from pathlib import Path
addresses = set()
for name in ('vtable_seeds.json', 'rtti_seeds.json'):
    for row in json.loads((Path('work') / name).read_text()):
        value = row['address'] if isinstance(row, dict) else row
        addresses.add(int(value, 0) if isinstance(value, str) else value)
Path('work/seeds_all.json').write_text(json.dumps(sorted(addresses)))
PY
python scripts/run_lift.py catalog work/SPEED2.analysis.exe work/seeds_all.json work/catalog.json
python scripts/run_lift.py lift work/SPEED2.analysis.exe work/catalog.json src/recomp/gen --split 400
python scripts/patch_display_menu.py
```

Run `patch_display_menu.py` after each lift: the port's graphics panel owns display
settings. Catalog/lift outputs are local binary-derived artifacts; keep them out
of commits and public CI artifacts. Other executable variants need explicitly
validated hook/layout work; do not assume an alternate EXE is interchangeable.

The generation sequence is a local build workflow, not a claim of byte-for-byte
APK reproducibility. If it produces a different catalog or missing functions,
inspect the dependency/input versions and [lifting record](LIFT_REPORT.md) before
publishing an artifact.

## Prepare DXVK and shaders

```sh
git clone https://github.com/Joshua-Ashton/dxvk-native.git ../nfsu2-dxvk-native
git -C ../nfsu2-dxvk-native checkout c8dc91fabd00cac11d697ccf07426e798393cd40
python ports/dxvk_android/prepare.py ../nfsu2-dxvk-native build/dxvk-source
python ports/dxvk_android/shaders.py build/dxvk-source
```

The preparation script applies this fork's Android/visual modifications. Reprepare
when those scripts change; do not assume an old generated dependency includes new
shader/runtime patches. See [the DXVK port guide](../ports/dxvk_android/README.md)
for details and dependency notices.

## Build a local APK

From the repository root with the prerequisites present:

```sh
bash android/gradlew -p android assembleDebug --console=plain
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk`, ARM64 by default.
The debug variant uses the fork package/update repository. A contributor's local
key will normally differ from the published fork key and cannot update an existing
fork installation. Do not generate/replace the maintained signing key or uninstall
a user's app to bypass an incompatible signature.

For a local 32-bit experiment, use `-PnfsAbi=armeabi-v7a`; optional
`-PnfsArmv7Tune=cortex-a53` is specific CPU tuning, not a generic compatibility
promise. Custom Vulkan driver loading is ARM64-only. The v0.14.14 published asset
is ARM64; current 32-bit gameplay parity needs separate validation.

Keep `nfs.resumable` off for ordinary builds. `-Pnfs.resumable=ON` is experimental,
not a way to promise durable Resume. For auditable signed artifacts, use
[the update workflow](ANDROID_UPDATES.md).

[Index](README.md) · [Maintenance](FORK_MAINTENANCE.md)
