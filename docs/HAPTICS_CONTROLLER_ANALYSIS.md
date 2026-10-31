# PC force feedback and Android controller output — 2026-10-31

The user reports overlong curb rumble and weak traction feedback with an Xbox controller.
This investigation separates the game's signals, our adaptation, and the motor API.

## What the original PC executable establishes

Target: `/home/ubuntu/projects/nfsu2-game/SPEED2.EXE`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
REA/Ghidra12.1.4 inspected the immutable executable, then closed its temporary project.
Retained evidence: `build/verification/ffb-analysis-2026-10-31.rea.json` (ignored,16 records).
These are decompiler observations, not original source or observed driving events.

- `0x5c8ed0` builds a56-byte DIEFFECT and a16-byte periodic payload, then calls
  SetParameters with flags0x140. Evidence:
  `ev_22d7500d6759f38c16a7e57d2c4ff61e3486418706c4b28283ba84ac6141d23a`.
- Caller `0x5c9760` requests a150000µs periodic effect with75000µs period and20000µs
  fade, and rate-limits its path using150ms. Its driving-event identity is NOT established.
  Evidence `ev_6da05726d4959f31d3680fe3378973dd4f8c117152256f678a1d218890bab59d`.
- `0x5c9fd0` selects among five periodic GUID addresses and sets an indefinite
  duration with variable period (argument capped at150 then multiplied by1000).
  Evidence `ev_a09d49f9a7ecc3b0ff432d1a18964542b15b09d4f185f2a1614459cfc06de2da`.
- `0x5c06a0` constructs condition data; `0x5c9150` supplies indefinite condition
  effects. Evidence `ev_1b9919e229361bbea446846fb493972bc4f9c1787c7526211285c6d2f148bff8`
  and `ev_f61482b71356641089447f069423523c081b08d2a47f9999897580b5fb323eb9`.

Earlier real runtime traces also show spring/damper and sine/square effects (see
ANDROID_PHONE_FEEDBACK.md and HANDOFF). This is wheel-oriented force feedback, not
an established console motor mix. No PS2 executable or event trace was inspected;
PS2 equivalence and exact curb/traction event labels remain unknown.

Added bounded runtime FFB trace: actual guest caller/type, magnitude, offset, gain,
duration, periodic period/phase, attack/fade, condition coefficient. At most120 records,
no more than one/type/second; excludes caller0 startup self-checks. Period/phase now
retained for inspection, but legacy scalar mixer still does not reproduce carrier
frequency or directional steering torque. Physics source remains selected by default.

## Motor control actually available

SM-S938U1 over Bluetooth reports Xbox Wireless Controller device26, motor=true,
amplitude=true, IDs[0,1]. Final installed game logs dual=true. This is reported API
capability; physical intensity resolution, radio latency and perceptual balance have
not been calibrated. User previously confirmed a simple controller pulse.

Android input-device single-vibrator calls address the device, despite a default
motor object. Separate strengths use VibratorManager + CombinedVibration parallel
effects. AOSP EventHub maps ID0 to evdev strong rumble and ID1 to weak rumble.
Waveforms let us request millisecond durations and amplitudes0–255; those numbers do
not guarantee256 physically distinguishable strengths or arbitrary motor frequency.
The two IDs do not expose Xbox trigger motors. No raw Bluetooth/HID writes used.

Primary references:
- https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/core/java/android/hardware/input/InputDeviceVibrator.java
- https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/core/java/android/hardware/input/InputDeviceVibratorManager.java
- https://android.googlesource.com/platform/frameworks/native/+/refs/heads/main/services/inputflinger/reader/EventHub.cpp

## Adaptation changes, not claims about original game intent

| Cue | Previous | New |
| --- | --- | --- |
| Surface release filter |180ms time constant|65ms time constant; attack/max unchanged|
| Surface pulse |155ms, refreshed80ms|70ms tapered pulse, refreshed at most80ms|
| Slip packet |90/150ms native gate plus Android sampling|Continuous severity; one output chatter envelope|
| Slip output |65ms one-shot restarted40ms|45ms on/35ms off, refreshed80ms|
| Slip strength |Base|1.5× base, clamped255; existing global strength still applies|
| Impact |15/65/45/35ms waveform,210ms priority|Timing/severity unchanged|

Dual-motor gains (percentage of shaped amplitude): surface65/20, slip65/100,
impact100/70 (strong/weak). Intended to separate subtle road bumps, finer traction
chatter and heavier impacts. No independent left/right tire-force claim. If the two
standard IDs/amplitude control are unavailable, retain scalar phone/controller output.
Binary motors encode intensity through shorter on-time, rather than ignoring event
amplitude and making small surface signals as strong as slide signals. All effects
are finite; pause, disconnect/route refresh and stop cancel the previous output.
A curb can still generate several pulses if suspension signals persist;70ms is the
pulse length, not a guaranteed total event duration. Slip/impacts remain kinematic
estimates, not measured contact forces or tagged collision events.

## Validation and remaining work

Final APK build passed. Full phone source suite passed including new pure-Java
FeedbackEnvelopeTest and curb-settle/continuous-slip regressions in physics_haptics;
force-feedback regression rechecked after trace metadata changes. Final installed
APK on :38393 selected dual output, then left running for user scenario testing.
No subjective success claim for the new envelopes yet. Compare curb hop, sustained
slide and wall, then inspect physics and original FFB trace around those scenarios.
Exact PS2 characteristics and matched motor calibration remain outside this evidence.


## User follow-up: distinguish slip from impacts

User said overall feel improved, but traction and collisions still felt alike.
Follow-up changes: slip motor balance25/100, impact100/25 (strong/weak), surface
unchanged65/20. Impacts now one65ms thump after5ms lead-in, then170ms silence.
The full240ms window is protected in Java even after the native210ms impact
packet expires, so slip cannot immediately fill that quiet tail. A new impact
ID can still interrupt with a fresh hit. Pause/route changes reset the hold.
Binary fallback has one amplitude-scaled pulse and the same total quiet window.
Envelope tests cover single-pulse tail and at least3:1 contrasting motor balance.
This is a perceptual tuning change; event-detection thresholds remain unchanged.


## Second follow-up: isolate output from detection

User still finds the two cues alike. Latest prior session PID789 logs14 impact
waveforms, with some short clusters but not continuous impact retriggering; this
cannot establish correct event classification. Leave physics thresholds unchanged.
Now slip uses only weak motor (0/100), refreshed80ms with110ms finite hold for
continuous texture. Impacts use only strong motor (100/0),30ms quiet lead-in,
85ms thump and245ms quiet tail,360ms protected window. Disabled motor amplitudes
are exactly zero, including tapered paths. Scalar fallback retains sustained vs
isolated timing; binary fallback stays amplitude-through-on-time.
Controls now offers Test traction (600ms hold on amplitude-controlled motors) and
Test impact (the actual impact envelope), both using the production output route.
Use these stationary tests before attributing ambiguity to event detection.
Build and focused envelope regressions pass. Phone :38393 disconnected/refuses
connection: this version is NOT installed or physically checked yet.
