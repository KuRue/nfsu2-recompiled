# Optional native graphics enhancements

The launcher exposes independent Texture packs, Texture filtering, Sharpening,
Lighting look, and Mirror / car reflections controls. Changes take effect on the
next launch. Existing installs retain original visuals until enabled.

Suggested S25 Ultra starting point: Balanced phone preset, HD packs on,
8× anisotropic filtering, Subtle sharpening, Night lighting + balanced color, High reflections.
Reduce reflections first if sustained frame rate or heat becomes a problem.
Higher reflection dimensions quadruple each affected target's pixels. This is
not a measured sustained-performance guarantee.

## What changes

- GPU texture replacement uses NTXP v1, keyed by the original tightly packed
  level-zero CRC32 + Adler32, dimensions and format. The original game resources,
  upload/readback behavior, UI metrics and base game files remain intact.
- `Internal storage/nfsu2/TEXPACKS/uhud.pak`: LunicAura106's Upscaled HUD.
- `Internal storage/nfsu2/TEXPACKS/world.pak`: separately converted world textures.
  New HD Textures Pack v1.1 by I_NEED_SPEED / Azazel is the tested HD source.
  AJLethal's PLUS World is an alternate art pack, largely original resolution.
  Assets and credits: [../tools/texpacks/SOURCES.md](../tools/texpacks/SOURCES.md).
- Optional 4×/8×/16× anisotropic minification improves roads viewed at an angle.
- Subtle/Crisp sharpening is a bounded, four-neighbor unsharp filter in DXVK's
  final presentation pass, after the game's gamma ramp. It adds no render target.
- Night lighting enables the game’s car lighting, headlights, car shadows and
  light glow, plus mild shadow lift, contrast and saturation grading in the
  presentation pass. It is not new light sources, shadow maps, ambient occlusion, or a
  physical lighting overhaul. It also affects the HUD because it is a final pass.
- Reflections On enables the game’s car environment mapping and road reflections,
  and changes the supported game's own mirror dimensions 256→512
  and car cube edge 128→256, guarded against unexpected executable values.
  The runtime overrides preserve saved game settings; switch the launcher options
  back to Original to use those saved values again. Road reflection resolution and existing game reflection logic are unchanged.

Missing/invalid packs and unsupported allocations fall back to original textures.
Replacement GPU resources plus retained originals have a conservative 256 MiB
budget per game process; later matches fall back once that budget is exhausted.
Restarting the game clears the cache. Texture files are indexed at startup and
payloads are loaded on a matching upload, not on every draw. Pack index maximum:
512 MiB file, 65,536 entries, 4096-pixel edges, DXT1/3/5 or A8R8G8B8, valid mips.

## Convert separately obtained assets

Never replace the original TRACKS bundles. Keep mods outside the base game.

```sh
# Extract your TexMod pack's DDS payloads first; filenames retain _0xHASH.dds.
python3 tools/texpacks/texmod_pack.py /path/to/base-game /path/to/dds \
  /path/to/base-game/TEXPACKS/world.pak
# Alternate: compare a mod's bundle with the unmodified bundle.
python3 tools/texpacks/world_pack.py /path/to/base/TRACKS/STREAML4RA.BUN \
  /path/to/mod/TRACKS/STREAML4RA.BUN /path/to/base/TEXPACKS/world.pak
adb push /path/to/base-game/TEXPACKS /sdcard/nfsu2/
```

Converters include original upload hashes and complete replacement mip chains.
Do not commit or redistribute generated packs with the source. A different base
edition may not match these hashes; mismatches safely retain original textures.

## Validation

Host NTXP tests exercise CRC/Adler identity, padded rows, mip sizing and malformed
index/payload bounds. Existing graphics-memory and COM lifetime checks cover the
bridge's guest/native memory and ownership behavior. All 54 DXVK shader variants
are compiled and validated for Vulkan 1.1 during preparation. Device smoke-test
results are recorded in HANDOFF.md. No controlled long-race benchmark is implied.
