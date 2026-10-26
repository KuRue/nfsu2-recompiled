# Phone vibration from game force feedback

The newer default is [vehicle-physics feedback](ANDROID_PHYSICS_HAPTICS.md).
This document describes the selectable previous Game force effects mapping.

The launcher has **Phone vibration**: Off, Light (25%), Medium (50%, default),
and Strong (100%). **Test phone vibration** sends a separate brief180ms pulse.
It always targets Android's default handset vibrator, even with a controller
connected. The connected controller is not vibrated by this implementation.
Changes take effect on the next game launch.

Native DirectInput now exposes emulated force-feedback capabilities and
constant/ramp/periodic/condition effect GUIDs. Guest effect objects have bounded
parameter parsing, reference ownership, Start/Stop/SetParameters, duration,
iterations, gain, start delay, attack/fade envelope and status. Device gain,
actuator enable/disable, pause and StopAll are handled. At most128 effect objects
may exist. Unknown/custom waveforms and effect-file import are unsupported.

Phone vibration is a scalar approximation: constant/ramp/periodic magnitudes
become motor strength. Wave frequency and force direction are not reproduced.
Spring/damper/friction resistance becomes a light steering-weight cue during
cornering, using the physical steering axis and the game's condition coefficients.
Changes in resistance produce a brief pulse. This is an approximation of steering
resistance, not measurement of tire slip or a guarantee that the game emits a
separate cue for every car/wall collision. A steady straight wheel produces no
condition rumble. Effects are mixed
using the maximum strength instead of summing into saturation. This does not
invent engine or collision events; it needs the game to send force effects.
Brief non-condition force peaks are retained for85ms (bounded32-cue queue), so
an impact shorter than the40ms UI polling interval can still be felt, even if
the effect object is subsequently released.
Stronger peaks (native amplitude>=160/255 before the user's strength scaling)
use a250ms priority pattern: a short quiet lead-in, two75ms taps separated by a
60ms gap. This temporarily overrides other road/cornering output so a hit can
be distinguished from ongoing sidewalk rumble. Repeated force updates within
an existing pattern strengthen it without continuously restarting the quiet
lead-in. This is a force-strength heuristic, not a separate wall/car event tag;
a sufficiently strong curb force can also receive the impact pattern. Device StopAll, gain mute, actuator
mute and lifecycle pause silence pending cues. An individual effect Stop/Unload
ends its sustained output but allows its already-triggered short cue to finish.
Condition changes use coefficients/saturation across all reported axes; malformed
or truncated condition payloads are rejected.

A neutral virtual joystick endpoint is offered while phone feedback is enabled,
so the feature can be used with touch controls as well as a physical pad.
Touch overlay visibility still depends on physical controller connection.

The UI polls native force level every40ms while foregrounded, submitting finite
65ms pulses through VibrationEffect with USAGE_GAME. Strength uses amplitude
control when available; otherwise pulse duration scales with the chosen strength.
No endless Android waveform, worker thread or service is created. The phone
motor and active guest effects stop on Activity pause; effects also stop on
joystick Unacquire/disconnection/device release and explicit game commands.
Menu input mode produces zero output. Android/system vibration policy can mute
USAGE_GAME; a successful API submission alone does not prove a physical sensation.
No-motor devices disable force capability. Vibrate is a normal manifest permission.

References:
- https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417952(v=vs.85)
- https://developer.android.com/reference/android/os/VibrationEffect
- ABI constants checked against the DirectInput header shipped with mingw-w64.

Validation: tests/force_feedback.cpp covers gain, duration, repeated/infinite
playback, start delay, pause/actuator muting, per-device stop, signed extremes,
condition suppression and envelopes. Startup guest parameter checks exercise
real guest pointers, partial updates, envelopes and malformed payload bounds.
Device test status/evidence is tracked in HANDOFF.md; physical phone testing must
not be inferred from emulator vibration-service records.
