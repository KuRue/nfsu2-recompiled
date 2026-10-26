# Logical graphics device state (experimental)

`graphics_state.h` saves a device as frame index, handle allocator, current state (blend mode,
bound texture, bound render target, scissor, tint) and its textures. Asset textures save only
`{id, size, asset ID, pixel CRC}` and are recreated from their source on restore; a changed asset
is refused. Render-target textures, drawn at run time, have no source and carry their pixels.
No host GPU objects (Vulkan images, DXVK handles, descriptor sets) are saved: everything is
rebuilt from the logical description.

Run: `python3 scripts/check_graphics_state.py [--sanitize]`

A software rasterizer draws 14 frames using two procedural assets and a render target that is drawn
into on frames 2 and 5 and sampled every later frame. State persists across frames: a scissor,
tint and freed texture. Captures at every frame boundary 0-14 restore in a fresh process and
produce identical frames. 622 bad inputs are rejected: truncations, CRC damage, an edited asset,
missing/duplicate handles, allocator reuse, asset bound as target, invalid blend/scissor/size/origin.

Limits: captures only between frames, RGBA8 2D textures, one state block. The real game path
(Direct3D 9 through DXVK to Vulkan) adds vertex/index buffers, shaders, samplers, depth/stencil,
many render states, lost-device semantics and dynamic buffers. These would be recorded at the D3D9
API layer the same way, with GPU-resident results (render targets, depth) read back at capture.
