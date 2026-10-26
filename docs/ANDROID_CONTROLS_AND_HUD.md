# Controller pedals and wide-screen HUD

## Controller pedals

The launcher defaults **Controller pedals** to **RT gas / LT brake · compatibility**.
Previously all keyboard fallback stopped as soon as the game acquired its
DirectInput joystick. The game's button bindings could then work while its
unbound combined trigger axis did nothing.

The compatibility mode keeps RT/LT mapped to the existing keyboard gas/brake
(up/down arrow) actions while driving, including when the joystick is acquired.
Steering, buttons and the raw joystick axes retain their existing path. Both
triggers can be held independently, without cancelling on the combined Z axis.
Menus do not receive these pedal keys; release/lifecycle clearing releases them.
This is digital pedal compatibility: partial pulls cross a threshold rather than
provide proportional throttle. It assumes the game's keyboard pedal bindings
remain up/down. **Game bindings · manual axis setup** disables this fallback for
players who configure their own analog trigger-axis bindings in the game.

Host regression test:
`g++ -std=c++17 -Wall -Wextra -Werror tests/gamepad_state.cpp -o /tmp/pad-check && /tmp/pad-check`
It exercises joystick acquisition, independent/simultaneous trigger holds,
menu isolation, button-only triggers, release and opt-out. Android event decoding
stays in gamepad_input.h; shared state/mapping logic is in gamepad_state.h.
The Backbone device reports GAS/BRAKE axes and L2/R2 keys; Android reads both.
A real physical trigger check must be completed by the player, not remote taps.

## HUD anchors

The base game downloaded from the website has no
`scripts/NFSUnderground2.WidescreenFix.dat`. Native widescreen geometry was active,
but zero HUD anchor entries left gauges/map/leaderboard near the 4:3 center.
The app now compiles in the 73-entry MIT-licensed ThirteenAG position table.
Existing custom tables still take precedence. No additional PC DLL/ASI is needed.
Aspect correction and anchor displacement use the actual render aspect ratio,
including the phone's 1756×810 Balanced resolution. Existing minimap top/bottom
choice now also works when the external table is absent. HUD artwork and sizes
retain their aspect; this change adjusts recognized positions rather than
stretching every UI element. Menus/unrecognized anchors remain unchanged.

Source: ThirteenAG/WidescreenFixesPack `nfsu2` release ZIP dated 2026-09-12:
https://github.com/ThirteenAG/WidescreenFixesPack/releases/download/nfsu2/NFSUnderground2.WidescreenFix.zip
Only the position table was converted to src/runtime/hud_defaults.inc; no binaries
are used. Original DAT SHA256:
2b8a08a7d99d4f3f28dd7adbaa0a49136688b948558fa5f3357723e0213b03f2.
MIT copyright/license retained in src/runtime/HUD_TABLE_LICENSE.

Host test tests/widescreen_hud.c checks missing-file fallback, 4:3/16:9/phone
anchor displacement, minimap vertical adjustment, unchanged unknown elements and
repeat initialization. Build with the sibling pcrecomp/runtime/recomp32 includes
and -lm; the preexisting hook-trace loop needs -Wno-error=misleading-indentation
under GCC's -Werror. Visual placement still requires checking actual game scenes.

Device smoke check (S25 Ultra, 2026-10-26): 73 anchors loaded and the race renders
map upper-left, leaderboard upper-right, gauges lower-right near phone edges.
Screenshot/local log under build/verification. Native callback received gas-down
from the physical controller; brake confirmation is pending from the player.

Potential future work: HUD size/safe-margin sliders, steering dead-zone tuning,
camera/FOV presets and skip-intro startup. These are candidate native additions,
not claims that Windows ASI mods run unchanged on ARM64.
