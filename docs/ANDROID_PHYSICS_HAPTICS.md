# Vehicle-physics phone feedback

**Phone feedback source → Vehicle physics · slip estimates** is now the default.
The previous **Game force effects** mapping remains selectable. Phone vibration
Off/Light/Medium/Strong applies to both sources. Restart the game after changes.
The two sources are separate: DirectInput steering-force heuristics are not mixed
into physics mode. Missing/invalid physics data produces silence, not guessed
feedback. This implementation targets the pinned default PC executable; alternate
f4bb0d99 builds must use the legacy source until their layout is validated.

## Read-only data validation

Candidate layout was taken as research input from the MIT DevCrashDay/speed2vr
source (revision bac567eba8c0d30985e3d94be11e8a8d1cec4f74); no Windows plugin is
loaded. See HAPTICS_PS2_INVESTIGATION.md for source links and original limits.
physics_probe.h reads through GuestMemory range/protection checks. Overflow,
uncommitted pointers, non-finite/out-of-range values and invalid body axes fail
closed. The probe executes on the guest-owning thread after Present, not from
Java concurrently with guest writes. It never changes guest memory or input.

On the S25 Ultra the user completed straight driving, stopped steering, sliding,
curb and wall scenarios. Captured343 samples at4Hz in the initial probe:
- One coherent player anchor120d8ef0 and race11c010d0 across the capture.
- Forward velocity -1.78…30.78, lateral -8.74…11.40; stopped samples zero motion.
- Wheel angular speeds -8.61…106.13, suspension values -.15…+.15.
- 108 straight/coherent samples give median rolling radius0.2958 from forward
  speed / front wheel angular speed (reference used0.30m).
- Estimated lateral slip angle median1.02°,90th percentile9.20°, peak49.14°;
  slide sections include~20–49° while straight sections are near zero.
- A sharp horizontal velocity change reaches~134m/s² at the probe's coarse rate;
  ordinary sections are much lower. This supports an impulse detector but is not
  proof of a direct collision-event tag.
- Race clock advances at4000 ticks/s. RPM and applied throttle move coherently.
  Candidate handbrake remained zero, so it is not used by the feedback algorithm.

Local evidence (ignored): build/verification/physics-probe-phone.txt. These are
observations in this capture, not guarantees for every car, track or game mode.

## Channels

physics_haptics.h updates from every presented frame using race-clock time:

- **Slip estimate:** body lateral velocity versus forward velocity detects a
  slide; front/rear average wheel surface speed versus vehicle speed estimates
  wheelspin/lock. A conservative straight/coasting calibration adapts rolling
  radius. Minimum speed/angle/ratio thresholds suppress ordinary rolling; parked
  steering/revving with stationary wheels gives no cue. Airborne-like vertical
  motion suppresses slip output. Actual contact flags/tire forces are not read.
  The output is a pulsed chatter with intensity following the filtered estimate.
- **Surface:** changes in suspension values give quieter texture while moving.
- **Impact estimate:** sufficiently abrupt horizontal velocity changes trigger a
  priority event, with a vertical-motion rejection and cooldown. This is an
  impulse estimate; hard landings or extreme maneuvers may still confuse it.
  Direct collision callbacks would provide stronger classification evidence.

Java receives strength, channel and impact sequence in one integer. A new impact
submits one finite Android waveform (15ms quiet,65ms strong,45ms quiet,35ms softer)
and suppresses other channels for210ms. Subsequent UI polls do not restart the
waveform. Slip/surface use the existing finite-pulse service, scaled by strength.
This is waveform timing on a phone motor, not two-motor PS2 reproduction.

Frozen race-clock data, stale samples (>150ms), invalid data, large pose jumps,
car/race changes, long frame gaps and lifecycle pause reset/silence the channels.
The app does not use steering deflection as a grip signal in this mode. Brake,
wheel and body motion are physical state estimates, not proof of actual tire
friction coefficients. Quick-race phone testing does not establish all free-roam,
replay or alternative-camera cases; validate these before making broader claims.

## Checks

Standalone host tests:

```sh
g++ -std=c++17 -Wall -Wextra -Werror tests/physics_probe.cpp -o /tmp/probe-check
/tmp/probe-check
g++ -std=c++17 -Wall -Wextra -Werror tests/physics_haptics.cpp -o /tmp/haptics-check
/tmp/haptics-check
```

Reader tests cover pointer overflow/bounds, race gating, finite/range/axis checks.
Analyzer tests cover straight/stopped silence, sliding, wheel lock/spin, suspension
texture without a false impact, impact priority/event identity, frozen data,
teleport/car changes and invalid snapshots. Phone perceptual results are recorded
in HANDOFF.md; passing these tests alone does not establish a convincing feel.
