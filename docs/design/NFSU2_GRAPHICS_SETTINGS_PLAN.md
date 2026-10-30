# Graphics settings overhaul — proposal

Status: implemented 2026-10-30.

## Why the current screen feels confusing

1. **Two kinds of “preset” are mixed with individual settings.**
   - The Performance preset changes resolution, FPS and frame queue.
   - Night effects, Atmosphere and Treatment look like presets but are stored
     as ordinary rows. Each one silently writes several concrete rows, so the
     panel can show both “Atmosphere: Rain haze” and “Fog: Off” at the same
     time while the summary flips to a different value.
   - Lighting look is a third kind: a presentation color grade, not a scene
     setting, but its name sounds like the game’s lighting toggles.

2. **The same setting appears in the launcher and in the in-game panel.**
   Bloom, sharpening, filtering, HD packs, the look preset, performance profile,
   resolution and FPS are editable in both places. The launcher and the panel
   group them differently, so the same value is described two ways.

3. **Several names describe implementation, not effect.**
   - “Lighting look” is a screen color grade.
   - “Night effects” is light trails + overbright + motion blur.
   - “Treatment” is bleach bypass + depth blur.
   - “Atmosphere” is fog + rain.
   - “Reflections” (2×) is reflection render-target resolution, while
     “Reflection detail”, “Reflection updates” and “Road reflections” are
     different scene settings.
   - “World detail” and “Car geometry” are both LOD levels.

4. **A few legacy values are still applied invisibly.**
   `PERF_LEVEL` and `TINT` are not shown anywhere, but `displayOverrides()`
   still sends `g_performancelevel` and `g_tintingenable` at launch. A user can
   change every visible row and still not know what those two globals do.

5. **Presets do not explain their scope.**
   Performance preset touches 3 values; the scene bundles touch 2–3; Lighting
   look touches the final image only. Nothing on screen says which is which.

## Current placement

| Value | Launcher Visual settings | In-game panel | Real effect |
|---|---|---|---|
| Performance preset | yes | Performance | resolution + FPS + frame queue |
| Internal resolution | yes | Performance | presented buffer size |
| FPS limit / frame pace / frame queue | yes | Performance | frame scheduling |
| Lighting look | yes | Effects | final-image color grade |
| Bloom / Sharpening | yes | Effects | final-image shader |
| Night effects | yes | Effects | light trails, overbright, motion blur |
| Atmosphere | yes | Effects | fog, horizon fog, rain |
| Treatment | yes | Effects | bleach bypass, depth blur |
| World / car LOD, shadow, lights, reflections, crowds, particles, fog, rain, etc. | no | Detail | game renderer globals |
| HD packs / filtering | yes | Textures | texture loading / samplers |
| Reflections (2×) | yes | Textures | reflection target dimensions |
| Texture decode | yes | Textures | decode path |
| Widescreen / minimap | yes | Display | display/HUD layout |
| Intro movies | yes | — | not graphics |

## Proposed information architecture

Five tabs, one purpose each. Every row gets a subtitle saying where it acts
and a badge for when it applies.

### Performance — frame delivery

- **Frame profile** — Battery / Balanced / Quality / Custom.
  A one-time action or a named combination that writes **only** resolution,
  FPS limit and frame queue. The row states that scope.
- Internal resolution
- FPS limit
- Frame pace
- Frame queue — **NEXT START**

### Image — screen-wide finish

Subtitle: “Applied after the game’s own rendering.”

- **Color grade** (renamed from Lighting look) — Original / Night / Neon /
  Filmic / Midnight
- Bloom
- Sharpening

No scene toggles here. This removes the “is Lighting look the same as Night
effects?” question.

### Scene — the game’s renderer

Subheaders keep the list scannable:

**World**
- World LOD
- Crowds
- Particles

**Car**
- Car LOD
- Car shadow
- Headlights
- Car reflection detail
- Car reflection updates
- Road reflection detail

**Light & weather**
- Light trails
- Light glow
- Overbright
- Motion blur
- Fog
- Depth blur
- Bleach bypass
- Rain

Every row is a concrete game global. No bundles live here.

### Textures — assets and samplers

- HD texture packs — **STREAMING**
- Texture filtering
- Reflection resolution (2×) — **NEXT START**
- Texture decode — **NEXT START**

### Display — output and HUD

- Widescreen — **NEXT START**
- Minimap

Move **Intro movies** to the launcher’s Game settings; it is not graphics.

## Presets: fewer, and explicitly actions

Recommendation: **remove Night effects, Atmosphere and Treatment as settings.**
They are the main source of overlap. Their concrete components stay in Scene.

Keep two named starting points:

1. **Frame profile** — performance only. It may stay a stored preference
   because users expect “Balanced”; it shows Custom after any of its three
   values is changed.
2. **Color grade** — a single screen-finish preset. It does not touch the scene.

If we want scene presets later, add a separate **Presets** section with one
action and a confirmation:

- “Night” → light trails + overbright + glow + fog
- “Rain” → fog + rain + road reflections
- “Cinematic” → bleach + depth blur + motion blur

Each action lists the rows it will change before applying, and the panel shows
“Custom” afterward. A preset is never a parallel stored value that can disagree
with the concrete rows.

## Launcher vs in-game ownership

Recommendation: **in-game is the source of truth for live settings.**

- Launcher keeps only **launch setup**: Vulkan driver, benchmark, texture
  decode, widescreen, frame queue, and the starting frame profile/resolution/
  FPS. These must be known before boot.
- Launcher stops showing duplicate live rows and bundles. Add a short
  “Graphics” summary that says “Change in game” with the current tab values,
  rather than a second editing surface.
- If keeping pre-launch editing is important for safety (a bad resolution can
  prevent boot), keep only Performance/Textures/Display launch rows there and
  label them “Used at launch”.
- Remove `PERF_LEVEL` and `TINT` from `displayOverrides()`. Either expose a
  real “Light tint” row or leave the game’s registry value alone. Remove the
  dead `DISPLAY_PRESET`, `DISPLAY_OWNER` and `DETAIL` keys and the unused
  `applyDisplayPreset()`/`displayPresetLabel()` path.

## Row anatomy

Each row:

- Title: the effect, not the internal name.
- Value: lime, as today.
- Subtitle: “Screen filter”, “World renderer”, “Car renderer”, “Asset
  loading”, “Frame timing”, or “Display”.
- Badge: **LIVE** (default), **NEXT START**, **STREAMING**.
- Optional info action: one or two sentences on what it changes, the default,
  and whether it persists.

Preset rows additionally say **“Sets: …”** so their scope is visible before
tapping.

## Naming changes

| Current | Proposed |
|---|---|
| Lighting look | Color grade |
| Night effects | removed (components in Scene) |
| Atmosphere | removed (Fog + Rain in Scene) |
| Treatment | removed (Bleach bypass + Depth blur in Scene) |
| World detail | World LOD |
| Car geometry | Car LOD |
| Reflection detail | Car reflection detail |
| Reflection updates | Car reflection updates |
| Road reflections | Road reflection detail |
| Reflections (Textures) | Reflection resolution (2×) |
| Texture packs | HD texture packs |
| Texture decode | Texture decode |
| Frame queue | Frame queue |
| Performance preset | Frame profile |

Keys stay the same; this is a label/IA change, not a preference migration.

## Migration

- Concrete preference keys and values stay unchanged; no save-game changes.
- Existing bundle preferences are already expanded to concrete rows by
  `setDisplayBundle()`, so removing the bundle UI does not lose a choice.
- Remove the bundle rows from the panel and launcher; keep the concrete rows.
- Remove the hidden `PERF_LEVEL`/`TINT` overrides and dead preset code.
- Keep `GameOptions.syncDisplayBundles()` only as a one-release migration
  helper if old bundle prefs are read anywhere; otherwise delete it.
- Update HANDOFF and the UI theme plan after the migration.

## Implementation phases

1. **Approve IA and naming** (this document).
2. **Clean up dead/hidden settings**: remove `PERF_LEVEL`/`TINT` overrides,
   dead display presets and unused keys; confirm boot still matches the
   registry values.
3. **Panel reorg**: five tabs, subheaders, row subtitles/badges, remove the
   three scene bundles, rename labels. No behavior changes to the concrete
   writers.
4. **Launcher reorg**: move Intro to Game, remove duplicate live rows and
   bundles, keep launch-only rows, add the “Change in game” summary.
5. **Optional**: scene preset actions with confirmation; continuous
   Bloom/Sharpening sliders; per-section reset.
6. **Phone verification**: every tab over a bright and a dark track, light and
   dark HUD, controller focus, no native Display entry, and a preset applying
   exactly the listed rows.

## Open decisions

1. Do we keep any scene presets, or only the Frame profile and Color grade?
   Recommendation: only the two.
2. Should the launcher keep live visual rows, or only launch-only rows plus a
   read-only summary? Recommendation: launch-only.
3. Do we expose “Light tint” and a game “Performance level”, or remove the
   hidden overrides? Recommendation: remove both unless a visible row is added.
4. Do Bloom and Sharpening become continuous sliders? Recommendation: keep
   three steps for now; the shader already supports continuous values if we
   want them later.
5. Should Minimap stay under Display or move to a separate HUD section?
   Recommendation: Display.

## Implementation notes (2026-10-30)

Implemented:
- Five tabs: Performance, Image, Scene, Textures, Display.
- Removed Night effects, Atmosphere and Treatment as settings. Scene now holds
  the concrete World / Car / Light & weather rows.
- Renamed Color grade, World LOD, Car LOD, Car reflection detail/updates,
  Road reflection detail, Reflection resolution (2×), HD texture packs.
- Frame profile is scoped to resolution, FPS and frame queue and shows Custom
  after any of those changes.
- Every row has a subtitle (screen filter, renderer, frame timing, etc.) and
  the existing LIVE / NEXT START / STREAMING badge.
- Fog writes both g_fogenable and g_horizonfogenable, replacing the old
  Atmosphere bundle.
- Launcher Graphics setup now has Performance, Launch-only and In-game
  sections. It no longer duplicates live Image/Scene rows or scene bundles;
  the In-game row is a read-only summary with a "Change in game" action.
- Intro movies moved to Game.
- Removed hidden PERF_LEVEL/TINT overrides and the dead display-preset code.

Verified on phone: launcher Graphics setup and In-game summary, Image/Scene
tabs with World/Car/Light & weather subheaders, and Fog applying both fog
globals live. Screenshots: /tmp/opencode/overhaul_*.png.
