# NFSU2 native UI theme audit and plan

2026-10-29. Evaluated native HEAD d3ee5a4, its launcher/control/dialog/benchmark source,
and saved phone screenshots from the preceding run: phone_home_after_tablet.png,
dialog_polish.png and dock_editor.png. These are not fresh device verification.
Scope: app-owned launcher, menus, dialogs, overlays and controls. No app changes in this pass.

## Findings

- Launcher: lime and dark green are established, but large rounded cards and a green gradient
  PLAY button look like a settings dashboard. Generic bold/default body fonts dominate.
- Dialogs: NeonDialog covers many colors, but framework title typography still bypasses the
  intended style. Framework lists, buttons and custom input forms need one owned component.
  There are 21 AlertDialog.Builder call sites across Java sources.
- Controls: editor accent is orange (#FF9A32), idle utility controls use blue-gray/white,
  pedals use red/green, and the dock adds a lime outline. These need consistent roles rather
  than a blanket recolor. Preserve Xbox/PlayStation face-button identification colors.
- Benchmark: condensed headings and lime accents are useful, but blue panel backgrounds,
  monospace timing and tiny labels need integration into the shared hierarchy.
- Implementation: colors are duplicated in Java and XML. Resources named neon_violet and
  neon_cyan actually hold lime colors. Launcher uses the platform DeviceDefault theme;
  there is no shared app theme or bundled TTF/OTF font.
- Other surfaces to cover: Play dropdown, option pickers, graphics/controls/game/saves/support
  submenus, tilt and frame-limit forms, named-layout dialogs, update/crash/storage flows,
  benchmark results, driver probe, startup/failure overlay, editor toolbar and text entry.

## Proposed visual direction

An interpretation of Underground 2's nighttime garage/menu character, not a claim that the
suggested colors or font are extracted originals. Use dark charcoal/blue-black surfaces,
white text, lime selection strips, angular/clipped panel details, restrained amber warnings
and condensed italic display headings. Avoid heavy glow, continuous animation and faux chrome
around ordinary settings. Keep body copy upright and readable. Bundle a redistribution-cleared
font after comparing it with actual game-menu references; do not label a generic Android
condensed font as the original. Use local font files so OEM font substitutions cannot alter it.

Provisional tokens for visual review:
background #090C10; panel #141A21; raised #202A32; lime #B5EA38;
text #EEF1E6; muted #A5B0AF; rule #34414A; warning #FFC15A; error #F07878.
Use semantic roles for focus, selection, pressed, disabled, destructive and success.
Controller focus must remain distinguishable from a saved selected option.

## Implementation sequence

1. Reference and foundations: compare actual game menu/HUD captures; approve a small launcher,
   dialog and control-editor mockup; choose and bundle the font with its license; define semantic
   XML resources, text appearances, dimensions, drawable shapes and a Java/Canvas theme adapter.
   Keep one source of truth for colors and fonts, including Canvas controls.
2. Launcher and menus: preserve current navigation/settings, replace oversized rounded-card
   treatment with compact angular rows and a strong PLAY/RESUME action, consistent section
   headers, value alignment and back navigation. Use real pressed/disabled/focus drawables.
3. Dialogs and inputs: add one app-owned dialog header/body/action component. Migrate the 21
   builder sites, pickers, input errors, checkboxes, radio choices, sliders and progress states.
   Set dimensions by available width/insets instead of one 68% width rule on every device.
4. Controls and overlays: unify dock, editor toolbar, utility icons, steering/pedals, pressed
   feedback, benchmark/live result and startup/error panels. Keep saved layouts, hitboxes,
   touch mappings, pedal distinctions and controller glyph colors intact. Retain low-opacity
   racing controls, unobstructed HUD and clear edit-mode selection/drag targets.
5. Completeness and verification: inventory every app-owned surface and every interaction state;
   check phone/tablet widths, landscape insets/cutouts, large text, long values, keyboard entry,
   controller-only navigation, focus restoration, TalkBack labels and non-color state cues.
   Check GPU/CPU cost with overlay visible/hidden; avoid expensive blur or continuous redraw.

Each stage is a reviewable commit. First implementation should be foundations + launcher,
then dialogs, then controls/overlays; no single giant restyling commit.

## Acceptance checklist

- No app-owned screen falls back to mismatched framework title/button/list styling.
- All text/Canvas views consume the same font roles and semantic palette.
- Normal, focused, selected, pressed, disabled and error states are visibly distinct.
- Menus are usable with touch and Backbone/controller focus; targets remain at least 48dp.
- Body text is readable; verify contrast for actual alpha-composited controls over bright/dark
  race scenes. Large fonts do not clip labels, values or dialog actions.
- Existing settings, control positions/docking and mappings survive the visual migration.
- Verify phone + tablet screenshots; source/build checks pass; compare overlay performance.

Android-owned permission screens, system file pickers and notification chrome cannot be fully
reskinned by the app. Theme the app's explanation and return flows around them.
