# Native Android graphics upgrades

Historical assessment from the English UI update, 2026-10-26.
The subsequent implementation is documented in [ANDROID_HD_GRAPHICS.md](ANDROID_HD_GRAPHICS.md);
it adds optional HD packs, filtering, presentation sharpening/grading and
mirror/car-reflection sizing. The assessment below predates those changes.

Original status: assessment only, 2026-10-26. No new graphics feature or asset pack is enabled
by the English UI update. Current rendering is D3D9 through DXVK Native/Vulkan.
The existing resolution/FPS presets are not texture or model upgrades.

## Recommended first work: optional texture replacement

Start with sharper HUD/fonts, then road/building textures. Preserve the original
assets, offer an on/off setting and a memory budget, and compare identical race
scenes for image quality, frame time and thermal behavior. Loading larger textures
is not automatically a performance improvement.

The web project already provides a concrete reference: UHUD, with a 101 MB
`TEXPACKS/uhud.pak` and GPU-only replacement in `runtime/web/texrep_web.c`.
Its loader does not exist in this native fork: a filename copied into TEXPACKS
will not activate the web feature here. The mod's Windows installer/plugin is
not used by the web implementation, either.

References:
- https://github.com/th0mk/nfsu2-web/blob/main/docs/web/VPS.md#texture-packs
- https://nfsmods.xyz/mod/4319 (LunicAura106's Upscaled HUD)

Native implementation investigation should establish stable original texture
identities and replacement format handling first. Keep guest resource sizes,
CPU locking, uploads, device reset handling and COM resource ownership correct.
A possible design substitutes a separate GPU texture when binding a known
original texture, instead of pretending guest-owned pixels have different sizes.
Cache matching results; do not hash every texture on every draw call. This design
is a proposal, not a validated integration. Confirm the pack's redistribution
terms before bundling it; personal local imports need no public asset hosting.

PC TexMod/ASI injectors should not be assumed compatible with native ARM64.
They need explicit native support or a separate, supported offline conversion.
Direct replacement of game BUN/BIN assets also requires validating format,
layout, sizes, and the mod's intended game build. Keep backups before edits.

## Filtering, antialiasing and reflections

Inspect the game's actual sampler/device settings and GPU bottlenecks before
adding filtering, antialiasing or higher-resolution reflection options. Anisotropic
filtering is a promising candidate for sharper roads at oblique angles, but no
forced filtering or AA override is implemented or benchmarked here yet. Windows
HD-reflections plugins are not drop-in native Android features.

## Higher-polygon models

This means replacement geometry and LOD assets, not a universal quality slider.
Cars must preserve the game's material/part conventions, customization points,
wheels, damage behavior and collision associations. City geometry and visibility
changes introduce additional streaming/memory/draw-call costs. Evaluate one
compatible model at a time after the baseline and texture-loader work.

## Ray tracing

There is no ray-traced rendering path in this fork. Vulkan support alone does
not enable ray tracing. A real implementation must verify the driver's required
extensions/features and add scene acceleration structures, geometry/transform
updates, ray shaders or ray queries, material/light integration and synchronization.
Useful real-time results may also need denoising and a substantial memory/frame
budget. Device hardware marketing is not a successful Vulkan feature probe.

Reference: https://docs.vulkan.org/guide/latest/extensions/ray_tracing.html

Treat this as a separate renderer research project after practical raster
upgrades. Screen-space shader effects sometimes called “RT” are not equivalent
to tracing the full game's geometry. No ray-tracing toggle is offered or promised.
