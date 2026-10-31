# Controls and vibration

## Physical controller

Connect the controller in Android before or during play. The app accepts external
USB/Bluetooth gamepads and filters out internal sensors that can advertise joystick
axes. Standard Android gamepad mappings are expected; unusual firmware/modes can differ.

| While driving | Xbox-style control |
| --- | --- |
| Steer | Left stick / D-pad left-right |
| Accelerate | RT |
| Brake / reverse | LT |
| Handbrake | A |
| Nitrous | B |
| Change view | X |
| Accept free-roam action / enter highlighted race or garage | Y |
| Shift up / down | RB / LB |
| Pause | Start / Menu |
| World map | Select / View |
| Look back | R3 |

**Controls → Controller pedals** defaults to **Triggers**. The fallback keeps
RT/LT usable when the PC game's DirectInput bindings expect buttons. **Game bindings**
disables that pedal fallback; it does not provide a full remapping editor.

## Game menus

Use D-pad/left stick and A to navigate/confirm, B to go back. For PC controls
outside normal focus order, including **Quit Game**:

- Move the **right stick** to position the mouse pointer.
- Press **R3** to left-click; hold it while moving to drag where supported.

This fallback is automatically active in non-driving game screens. During driving,
R3 remains look-back. Launcher and Android dialogs use Android focus navigation,
not the game's mouse-pointer layer. Full controller-only coverage of every PC
screen has not been validated; report the screen and controller model if one fails.

## Touch controls

Touch driving/menu controls use the silver/lime theme. The driving dock can collapse
behind the top-left expander. Use the in-game control/layout tools for position,
size and saved layouts; classic, Xbox and PlayStation styles remain available.
Tilt steering and its sensitivity/inversion controls are separate from physical
controller input. With a physical pad active, touch controls can hide; touching the
screen reveals them again.

## Vibration

**Controls → Vibration** selects Off, Light, Medium or Strong. Physics feedback is
the default; Legacy retains the scalar adaptation of the PC game's wheel forces.
Vibration changes are read at game launch.

Output prefers a connected controller that reports rumble support, with phone
fallback when none is available. On compatible pads exposing two amplitude-controlled
channels, traction favors the weak motor and impacts the strong motor. Curbs use
short, quieter surface pulses. The newest mapping uses sustained traction texture
and an isolated impact knock with a quiet interval; physical tuning is still ongoing.

Use **Test traction** and **Test impact** in Controls to compare the output without
driving. Tests target the currently available controller/phone at a fixed reference strength. Compare them before
changing settings globally: they help distinguish motor/driver limitations from
misclassified game events.

Android/controller/connection support varies. A reported motor does not establish
calibrated intensity, trigger-motor access or arbitrary frequency control. Slip and
impact cues are kinematic estimates, not tagged collisions or measured tire forces.
The implementation is not claimed to reproduce the PS2 game's exact rumble.

[Graphics](GRAPHICS.md) · [Troubleshooting](TROUBLESHOOTING.md) · [Index](README.md)
