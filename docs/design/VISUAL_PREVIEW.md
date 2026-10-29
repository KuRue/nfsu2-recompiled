# Launcher visual preview — implementation proposal

User wants a real game-rendered preview in the left settings pane that updates with each change.
Not implemented by the 2026-10-29 layout change. No screenshot filters or synthetic approximation
have been added as a substitute for actual geometry/texture/reflection changes.

## Findings from current source

- AndroidManifest.xml isolates GameActivity and KeepAliveService in :game; LauncherActivity
  cannot directly call the renderer running in that process.
- GameActivity sends visual configuration via nativeVisualOptions at boot. main.cpp writes
  environment variables for packs/filtering/sharpening/looks/bloom/etc.; display overrides also
  arrive at boot. These are not a general live settings API.
- Existing texture readback in d3d9_bridge.cpp is not a launcher preview frame-export path.
- A still image can compare resolution, textures, geometry, grading and reflections, but cannot
  demonstrate frame pacing/FPS or temporal effects such as trails and motion blur.

## Proposed work

1. Establish an isolated repeatable car/street scene using the user's installed game assets.
   Never restore or mutate their career/live session to generate the demo. Prove scene boot and
   rendering in the game runtime before adding UI that promises a preview.
2. Add a renderer-thread settings request API with monotonically increasing request IDs.
   Separate mutable post-processing uniforms from resources requiring rebuild/reload (resolution,
   render targets, textures, drivers). Driver changes need process restart, not an instant update.
3. Export a small final-color render on demand through explicit IPC to the launcher. Serialize
   render/reload jobs, debounce rapid changes, and discard stale results. Cache by scene, complete
   settings, asset version and renderer version. Keep previous frame while preparing a new one.
4. Display the image above CPU/GPU demand, with Preparing/Unavailable states and no fake result.
   Retain the pinned Play/Resume footer and scroll the preview/summary on smaller screens.
5. Offer a brief opt-in loop for temporal effects. FPS/pacing options need a motion sample or a
   benchmark, not an altered still image. Stop preview rendering when hidden/paused.

Validation: compare exported images to actual gameplay with the same configuration, test rapid
changes and cancellation, verify no career writes/live-session interference, bound memory/heat,
and exercise texture/target rebuilds plus process death. First result will need warm-up; cannot
promise every option will update instantly without implementing and measuring this pipeline.
