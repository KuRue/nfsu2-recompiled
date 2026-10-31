# Troubleshooting

## Play is unavailable or the game fails during startup

Check the launcher Game Files panel first. Verify file access, `/sdcard/nfsu2/`
without an extra folder level, the supported executable and nonempty base folders.
Choose a language whose file exists in `LANGUAGES/`. Optional HUD/HD pack warnings
are separate from required game data.

On a build machine, the read-only preflight is:

```sh
python3 scripts/check_android_game.py /path/to/your/game
```

It checks executable identity and directory presence, not every asset's integrity
or gameplay compatibility. See [Installation](INSTALLATION.md).

## Vulkan/device error, corrupted textures or black output

Start with the system driver and **Graphics setup → Test driver**. Vulkan 1.1 support
alone is not a full compatibility guarantee. If a custom driver caused the issue,
return to System. Try CPU texture decode for compressed-texture problems, then
restart the game. Do not replace the supported executable or erase saves to fix a
GPU capability failure. Include the probe result and GPU/driver in a report.

## Low FPS, hitches or heat

Enable the FPS counter. Compare the same scene/settings after loading settles;
loading, new peers and shader compilation can cause spikes. Reduce resolution,
world/car detail, reflection work, or nearby cars as appropriate. A lower FPS cap
can help sustained load. Clear shaders only when troubleshooting a suspected cache
issue, since rebuilding them can temporarily increase stutter.

There is no universal target FPS for every phone. CPU/GPU demand sliders estimate
settings cost; they are not sensors. [Graphics guide](GRAPHICS.md).

## Controller cannot select a control

In game menus, right stick moves the pointer and R3 clicks, including mouse-only
controls such as Quit Game. D-pad/left stick and A/B remain the normal navigation.
For Android launcher/dialogs use Android focus navigation. If the cursor fails,
report controller model, USB/Bluetooth mode and the exact screen; broad physical
coverage is still pending. RT/LT issues: check **Controls → Controller pedals**.

## Vibration feels wrong or stays on the phone

Connect the controller and use **Controls → Test traction** and **Test impact**.
A capable controller is preferred; otherwise the phone is the fallback. Some
Android drivers/connections do not expose rumble even when the hardware supports it.
Use these stationary tests to separate output behavior from driving-event detection.

The newest dual-motor mapping is still being tuned. Slip/impact estimates can
misclassify extreme maneuvers or landings. Describe whether the problem happens in
the isolated tests, actual driving, or both; include vibration strength and source.
No exact PS2 or trigger-motor behavior is promised.

## Resume starts fresh

Resume is a retained live process, not a durable snapshot. Android can kill it.
Reload your regular career save after process death. Do not rely on Resume as a
substitute for saving. Released APKs keep experimental save-state support off.

## No sound after changing headphones/Bluetooth

The audio stream is designed to reopen on route changes. If it does not recover,
record the output device and switching sequence, pause/resume once, and report the
result. Save progress before restarting the game. Do not clear app data as a first step.

## Network or quitting problems

For multiplayer, identify Local LAN, nfsu2.xyz or the separate Online server; test
switch reachability where applicable. A transport connection does not validate a
whole race/lobby path. [Multiplayer](MULTIPLAYER.md).

Quit Game should return to the launcher and close the completed game process. If
an error screen appears instead, capture its exact message. If force-closing is
necessary, unsaved progress may be lost; avoid uninstalling/clearing data.

## Report an issue

Use **Support → Report issue** or
[the fork issue tracker](https://github.com/KuRue/nfsu2-recompiled/issues).
Include app version, phone/Android/GPU, driver, steps, scene/network mode, controller
and connection, relevant graphics/haptics settings, and whether a fresh launch
changes the result. Attach the diagnostic log and a screenshot when useful; inspect
logs for personal paths/account details before sharing. Never post passwords or
private signing keys. Do not attach game binaries, music, converted packs or saves
that you do not want made public.

[Index](README.md) · [Current validation](STATUS.md)
