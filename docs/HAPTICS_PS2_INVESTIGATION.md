# PS2 reference and next haptics architecture

2026-10-26: user reports the revised DirectInput-to-phone mapping still feels the
same and asks whether PS2 rumble can be studied/adapted. No APK change in this
investigation. Do not describe pattern tuning as validated event discrimination.

## Established from source

PCSX2 revision f0523f8acb1d3a765ee73b74cffec63093569199:
pcsx2/SIO/Pad/PadDualshock2.cpp labels large motor low-frequency and small motor
high-frequency. In motor decoding (around214–257), large motor is a byte value;
small motor uses its low bit, and the two are passed to
InputManager::SetPadVibrationIntensity. This establishes the controller protocol,
not the NFSU2 PS2 game's event-selection or traction-feedback logic.
Source: https://github.com/PCSX2/pcsx2/blob/f0523f8acb1d3a765ee73b74cffec63093569199/pcsx2/SIO/Pad/PadDualshock2.cpp

Our current native app executes a lifted PC executable. Its haptic input is
DirectInput wheel forces. Condition coefficient × steering is a steering-load
approximation, not tire-slip telemetry. Force amplitude alone doesn't identify
walls, other cars, curbs, or grip loss; a phone's single output also merges those
signals. The user's perceptual result overrides previous optimistic expectations.

An exact PS2 NFSU2 recreation requires that game's PS2 executable or a recorded
runtime motor-command trace for controlled scenarios. Neither was supplied or
inspected. No PS2 game image was downloaded. Open emulator source alone cannot
establish what that particular game sent or whether it supplied grip cues.

## Useful PC reference, not yet validated against our runtime

DevCrashDay/speed2vr revision bac567eba8c0d30985e3d94be11e8a8d1cec4f74 includes
NFSU2-SimHub/src/{telemetry,ffb}.cpp. Its source reads vehicle world velocity,
body axes, wheel angular speeds, suspension compression and controls. It estimates
wheel slip from wheel surface speed vs longitudinal velocity, plus lateral
velocity; its custom FFB estimates front slip angle using velocity/yaw and steering,
and impacts from acceleration changes. These are estimates, not a proven direct
per-wheel tire-force/slip field from this native build.
Sources:
https://github.com/DevCrashDay/speed2vr/blob/bac567eba8c0d30985e3d94be11e8a8d1cec4f74/NFSU2-SimHub/src/telemetry.cpp
https://github.com/DevCrashDay/speed2vr/blob/bac567eba8c0d30985e3d94be11e8a8d1cec4f74/NFSU2-SimHub/src/ffb.cpp

Candidate addresses from that source: view832E50, race coordinator8900D8;
view list +44 -> node -> anchor; anchor+50 position,+70 velocity,+90/+A0/+B0
body rows,+330 four wheel speeds,+380 four suspension values. Our generated C
contains832E50 references. This does NOT validate the pointer chain, data layout,
units, player identity or other addresses in the running native executable.
Do not copy offsets into output-producing code without validation.

## Next implementation boundary

1. Add a read-only checked telemetry probe for the exact supported build;
   validate player selection, finite/ranged values, units, and coherent updates
   in straight driving, stationary steering, cornering, skidding, curb runs and
   controlled car/wall impacts. Avoid changing input or game memory.
2. Prefer real collision/impulse callbacks when found; acceleration spikes alone
   can confuse curbs, collisions and discontinuities. Slip estimates must account
   for steering/yaw, reversal, low speeds, airborne wheels and race transitions.
3. Keep separate surface, grip-loss and impact channels until the final phone
   synthesis. Lower road texture during an impact; give grip-loss a recognizable
   pattern tied to validated slip estimates, not just steering deflection.
4. Compare repeatable scenarios and get physical user confirmation before claiming
   fidelity or PS2 equivalence. A PS2 trace can guide timing/channel mapping if
   supplied later; it is optional for a telemetry-driven native implementation.

REA skill route was reviewed. This investigation used complete open-source
PCSX2/mod files and local generated C references, so no native binary analysis
session/provider was started. Binary/runtime reverse engineering will need the
appropriate evidence workflow when validating unestablished fields.
