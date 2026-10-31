# Graphics

## Where to change settings

**Graphics setup** in the launcher contains frame profiles, resolution/FPS choices,
driver tools and launch-only settings. Once playing, open the in-game **Graphics**
side panel to compare supported changes in the actual scene. The previous screenshot
preview is superseded by this panel; there is no animated demo preview.

The original PC **Display** category is removed so it cannot overwrite the port's
settings. The panel separates Performance, Image, Scene, Textures and Display.

| Group | Controls |
| --- | --- |
| Performance | Frame profile, internal resolution, FPS limit/counter, frame pace, frame queue |
| Image | Color grade, bloom, sharpening |
| Scene | World/car LOD, crowds, particles, shadows, headlights, car/road reflections, light effects, weather and post effects |
| Textures | Installed HD packs, filtering, reflection resolution, texture decode |
| Display | Widescreen correction, minimap position |

The **CPU/GPU demand sliders are estimates**, not measured utilization or guaranteed
FPS. Enable the small top-right FPS counter to observe actual presented frame rate.
It is off by default and can be changed live.

## Live, streaming and next start

Rows marked **NEXT START** wait for a fresh game launch. These include widescreen,
frame queue, reflection resolution and texture decoding. Merely backgrounding and
resuming the same process does not apply them.

HD packs are marked **STREAMING**: toggling changes replacement use as textures
are loaded; it does not promise every already-loaded asset is immediately replaced.
Most other panel settings apply live. Read the running/next-start indication rather
than assuming a saved launcher value is active in an older retained session.

## Visual choices

Color grades are Original, Night, Neon night, Filmic and Midnight. Bloom and sharpening
are separate controls. Presentation grading also affects the HUD. Scene settings expose
the game's own effects; these are not ray tracing, new physical light sources or a
replacement high-polygon car pack. Upscaling is currently unavailable.

Higher reflection resolution increases mirror/car environment-target cost; it is
separate from reflection detail and update settings. Higher world/car LOD increases
work independently of internal pixel resolution.

## Performance tuning

Change one setting at a time in the same scene. Reduce resolution for pixel-heavy
work; reduce car/world detail and reflection work for geometry/submission cost.
Lowering the FPS cap can reduce sustained load. Avoid comparing loading screens,
different crowds or cold/warm runs as if they were matched benchmarks.

**Game → Nearby cars** accepts 0–32, defaults to3, and requires a fresh game launch.
Zero keeps remote map markers without rendering remote cars. High counts can be
expensive. Renderer state caching and distributed car loading reduce some overhead,
but neither removes all crowd or asset-loading hitches.

## Optional HD packs

Keep the base game files intact. The native replacement format is NTXP, not an
arbitrary TexMod archive/ZIP. Separately obtained textures need conversion with
[the texture tools](../tools/texpacks/SOURCES.md). Expected locations:

```text
/sdcard/nfsu2/TEXPACKS/uhud.pak
/sdcard/nfsu2/TEXPACKS/world.pak
```

Select HD texture packs in the graphics panel after installing compatible packs.
Missing/invalid/unmatched entries fall back to original textures. Different base
assets can have different identities, so a pack's presence alone does not prove a
match. Replacement memory is budgeted; high-cost packs may not replace every texture.
No texture packs or replacement game assets are bundled with the release.

## Driver tools

Start with the system Vulkan driver. On supported ARM64 configurations the launcher
can import compatible custom driver ZIPs; no driver is supplied. **Test driver**
checks capabilities, not complete race compatibility. CPU texture decode is a
compatibility fallback, not generally a speed improvement. **Clear shaders** forces
shader rebuilding and can cause temporary stutter afterward.

[Controls](CONTROLS.md) · [Troubleshooting](TROUBLESHOOTING.md) · [Index](README.md)
