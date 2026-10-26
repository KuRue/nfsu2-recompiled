# Phone vibration from game force feedback

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
Spring/damper/friction resistance is accepted as a condition effect but produces
no vibration because the phone cannot exert wheel resistance. Effects are mixed
using the maximum strength instead of summing into saturation. This does not
invent engine or collision events; it needs the game to send force effects.

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
