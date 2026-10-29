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

## Photogenic still: the game's showcase camera (2026-10-29)

The first preview (Sol) captures the chase camera 90 frames into a solo quick-start race: HUD, countdown and the car small at the bottom. The game has a camera made for posing the car.

- Camera factory: `sub_005F1FD0(this=player, type, flag)` switches on `type` through the byte table at `0x5F2F84`. Mapped by joining the cleanup table at `0x7E50A8` to the constructor after each unwind state: type 2 DebugWorld (`0x455B10`), 3 TrackRouteEditor (`0x44DBF0`), 4-14 and any out-of-range type OrbitCar (`0x4531A0`), 17 AutoPilot (`0x44ED50`), 18 Ice (`0x453C00`), 19 CopView (`0x4519E0`), 20 Showcase (`0x452520`), 21 DynoCar (`0x452600`).
- `sub_00501290(bool)` (stdcall, one argument) is the game's own toggle: true creates the showcase camera (type 20) for the player at `[0x8900AC]` and calls `0x6022A0`; false restores the normal camera.
- Phone, 2026-10-29: `debug.nfs.showcase 1` calls it once, 60 racing frames into a quick-start race. The game cuts to a low front three-quarter shot of the car against lit downtown buildings, which is the composition wanted for the preview.
- The race HUD (positions, laps, minimap, music popup) stays up. The player's HUD mode is the byte at `player+0x28`; `sub_005EA800(1)` (the `JOY_EVENT_CYCLE_HUD` handler, 0xBC) steps it through 0, 1, 3, 2. `debug.nfs.hudcycle 1` sets 0, 1, 2, 3 for 3 s each from 6 s into a race, to find which value hides the HUD; that run was interrupted before the screenshots.

Next: pick the HUD-off value, then in preview mode switch to the showcase camera once the car is on the grid, hide the HUD, wait for the camera to settle and capture. Optionally choose the SkipFE track (`0x7FAA18`) for the most iconic backdrop. The capture stays a single still per settings key; boot and race load (about 5.5 s of the 8 s) remain the cost.
