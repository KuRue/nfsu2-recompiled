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

## Look presets, bloom and dithering (2026-10-29)

The final presentation pass now carries the full display-referred look, all
after the game's gamma ramp, so nothing here changes scene rendering:

- `Lighting look`: **Original**, **Night lighting + balanced color**, **Neon
  night** (cooler shadows, warmer highlights, a little more contrast and
  colour) and **Filmic night** (lifted blacks, soft highlight shoulder, gentle
  desaturation). Every look except Original also enables the game-side night
  lighting (car lights, headlights, car shadows, glow).
- `Bloom and glow`: Off / Subtle / Strong. An eight-tap bright-pass glow in the
  same final pass — neon signs, headlights and bright HUD elements bleed a
  little light. It is not a physical bloom chain (no extra render targets) and
  it affects the HUD because the pass sees the finished frame. It is the first
  option to drop if thermals or frame rate matter.
- Ordered 4×4 dithering runs whenever any effect is enabled; it removes
  banding in the night sky on the 8-bit swap chain without visible noise.
- Vignette strength follows the look (none / 5% / 11% / 16%).

Recommended S25 Ultra starting point: HD packs on, 8× anisotropic filtering,
Subtle sharpening, **Neon night** or Night lighting, Subtle bloom, high
reflections. Bloom adds fill-rate cost per pixel; measure with the built-in
benchmark before keeping Strong.

These are display-referred grading effects, not new light sources, shadow maps,
ambient occlusion or a physical lighting overhaul.

## Deeper blacks, car reflections and the game's own night effects (2026-10-29, later)

Reading the game's saved registry (`/sdcard/nfsu2/android_registry.dat`) showed
every visual feature off by default, including `g_carenvironmentmapenable` **and
`g_carenvironmentmapupdatedata`** — the port force-enabled the env map but never
its updates, so car paint sampled an empty cube and looked matte. The launcher's
Reflections switch now forces the update flag too.

`Lighting look` presets were retuned: Night lifts shadows far less and applies a
small black point; Neon deepens blacks and raises contrast/saturation; a new
**Midnight** preset goes furthest (black point 3.2%, contrast 1.10, saturation
1.13, cool shadows, magenta highlights). Vignette follows the preset.

New **Night effects** option (Off / Glow, light streaks, tinting, particles /
…plus motion blur) forces the game's own `g_lightstreaksenable`,
`g_overbrightenable`, `g_tintingenable`, `g_particlesystemenable`,
`g_crowdenable` and, at level 2, `g_motionblurenable`. These are the original
game's effects, not new rendering code. Also fixed: night lighting/car lights
were only enabled when the look value was exactly 1, so Neon/Filmic never turned
them on.

Measured on the phone (benchmark, same scene): Midnight + effects + reflections
update 91.0 FPS average, 1% low 54.7, 49,370 draw calls/frame; Night + motion
blur 97.8 FPS, 1% low 60.0, 46,439 calls (baseline without effects: ~117 FPS,
42k calls). Drop to 85% resolution or turn effects off if that matters more than
the look.

## Registry audit: what else the game was hiding (2026-10-29)

`android_registry.dat` is the game's own settings store; every visual feature
was off. Mapping of the remaining keys and what the port does now:

| Key | Saved | Port behaviour |
|---|---|---|
| `g_carenvironmentmapenable` / `updatedata` / `g_roadreflectionenable` | 0 | forced with **Reflections** (update flag is what fixes matte paint) |
| `g_carlightingenable` / `headlight` / `carshadow` / `lightglow` | 0 | forced with any **Lighting look** |
| `g_lightstreaksenable`, `g_overbrightenable`, `g_tintingenable`, `g_particlesystemenable`, `g_crowdenable` | 0 | forced with **Night effects** |
| `g_motionblurenable` | 0 | **Night effects** level 2 |
| `g_fogenable`, `g_horizonfogenable` | 0 | **Atmosphere** level 1 |
| `g_rainenable` | 0 | **Atmosphere** level 2 |
| `g_bleachbypassenable`, `g_depthoffieldenable` | 0 | tested (see screenshots), not exposed yet |
| `g_worldlodlevel`, `g_carlodlevel`, `g_performancelevel` | 0/0/1 | left alone: forcing 2 changed nothing measurable in the benchmark, so the value mapping is unconfirmed |
| `g_texturefiltering` | 1 | left alone: anisotropic filtering is already forced at the sampler level |
| `g_cardamageenable` | 0 | cosmetic damage only |
| `g_vsyncon` | 0 | irrelevant: the port paces frames itself |

Measured (same benchmark, 2340×1080, Midnight + effects + reflections):
no atmosphere 91.0 FPS avg; fog 95.5; fog + rain 96.7; adding bleach + DOF
90.4. Differences are inside the benchmark's run-to-run variance.

## Visual treatment and detail level (2026-10-29, later still)

Two more launcher options:

- **Visual treatment**: Original / **Bleach bypass** / **Bleach + depth of
  field**, forcing `g_bleachbypassenable` and `g_depthoffieldenable`. This is
  the game's own stylised film look; the depth-of-field stage is the second one.
- **Detail level**: Original (the game's own setting) / **High** / **Maximum**,
  pinning `g_worldlodlevel`, `g_carlodlevel` and `g_performancelevel` to 1 or 2.
  Confirmed on the benchmark: Maximum lifts draw calls from ~49k to 51.7k
  (about 5% more geometry) and costs roughly 8% frame rate in the same scene.
  The game's own Display menu names this row "Level Of Detail"; the registry
  dump and log now print the real forced value.

The override log line used to print "= 1" regardless of the forced value; it now
prints the actual number.

## Level Of Detail is a preset; the full display map (2026-10-29)

Watching the game's own writes showed the Display page's **Level Of Detail**
slider is a four-step preset that writes the Customize Display Settings values
when you leave the page:

| Preset | Values written |
|---|---|
| Low | everything 0, `g_performancelevel=1` |
| Medium | headlights 1, car shadow 2, crowds 1, light glow 1, tint 1, performance 2, world LOD 1 |
| High | + env map 1, env updates 1, car LOD 1, DOF 1, fog + horizon fog 1, light trails 1, motion blur 1, overbright 1, particles 1, performance 3, road reflection 2, world LOD 2 |
| Ultra | + bleach bypass 1, env map 2, rain 1, performance 4, road reflection 3 |

The Customize submenu rows map to these keys (observed values in brackets):
Car Reflection Update Rate → `g_carenvironmentmapupdatedata`;
Car Reflection Detail → `g_carenvironmentmapenable` (0-2);
Car Shadow → `g_carshadowenable` (0/1/2, 2 shown as "Neon");
Car Headlight → `g_carheadlightenable`;
Car Geometry Detail → `g_carlodlevel` (0-2);
Crowds → `g_crowdenable`; World Detail → `g_worldlodlevel` (0-2);
Road Reflection Detail → `g_roadreflectionenable` (0-3);
Light Trails → `g_lightstreaksenable`; Light Glow → `g_lightglowenable`;
Particle System → `g_particlesystemenable`; Motion Blur → `g_motionblurenable`;
Fog → `g_fogenable` + `g_horizonfogenable`; Depth Of Field → `g_depthoffieldenable`;
Full Screen Anti-Aliasing → `g_fsaalevel` (not persisted by the port);
Over Bright → `g_overbrightenable`; Bleach (bypass) → `g_bleachbypassenable`;
Rain → `g_rainenable`; Texture Filtering → `g_texturefiltering`; Vsync → `g_vsyncon`.

The launcher now has a **Game display settings** section: a Detail preset picker
(Low/Medium/High/Ultra applying exactly the table above) plus one row per
setting. With "In-game display menu: Ignored", the game's own page still shows
the launcher values but its writes are discarded, so the two cannot conflict.

## Verified on the phone (2026-10-29)

Preset High applied from the launcher writes world 2, car geometry 1, car shadow
2, env map 1 + updates, road reflections 2, light trails/glow/particles/motion
blur/fog/DOF/overbright on, performance 3, tint 1. The game log shows
`Display overrides: g_worldlodlevel=2,g_carlodlevel=1,...` and
`Visual override g_worldlodlevel = 2` for each key, so the runtime receives
them. Changing a value in the game's own Customize page while locked leaves
`android_registry.dat` byte-identical — the in-game menu cannot fight the
launcher.

Benchmark with preset High plus the Midnight look and Subtle bloom:
77.8 FPS average, 1% low 37.4, 55,185 draw calls/frame, render thread 4.15 ms.
That is the heaviest configuration measured; 85% resolution or a lower preset
buys back frame rate.

## Launcher redesign: Underground neon (2026-10-29)

The launcher now uses the Underground neon theme: deep-violet night background,
violet primary and cyan secondary accents, condensed uppercase headings, card
rows, and themed dialogs (`NeonDialog`) replacing the stock Android alert look.
Benchmark moved to the top of Visual settings as a cyan-accent card showing the
last result; wording shortened ("Visual settings", "Controls", "Saves",
"Support", "Performance preset"); the old "Phone preset" name and the
"Ignored · launcher owns the look"/"Game setting" wording are gone. The display
detail rows now always carry concrete High-preset defaults, so the launcher is
the single source of truth.
