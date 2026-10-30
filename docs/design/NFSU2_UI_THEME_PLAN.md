# NFSU2 native UI theme audit and plan

2026-10-29. Evaluated native HEAD d3ee5a4, its launcher/control/dialog/benchmark source,
and saved phone screenshots from the preceding run: phone_home_after_tablet.png,
dialog_polish.png and dock_editor.png. These are not fresh device verification.
Scope: app-owned launcher, menus, dialogs, overlays and controls. No app changes in this pass.

## Current palette and divider rule (latest user refinement, 2026-10-29)

The all-dark revision was farther from the desired look. Return toward silver-gray menu
sheets, but darker than the initial pale version, over the dark neutral backdrop. Use dark
readable text on these medium-gray sheets, with rounded black frames and lime titles integrated into their top bands.
Game-file checks get a distinct dark inset, light-gray labels and colored status glyphs so
these do not blend into their parent sheet. Preserve outlined actions and separate focus states.
Static header dividers remain plain: dots belong only on actual interactive slider thumbs.
Option values have no separate background pills: whole rows are slightly darker than the gray
sheet, with lime value text and a small dark drop shadow. Play/Resume uses a green pulse and
moving shimmer inspired by the title-screen prompt.
This latest refinement supersedes the preceding all-dark panel/light-body-text direction.

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

## Revised direction: reproduce the actual menu and HUD vocabulary

This revision supersedes the earlier generic charcoal/lime/angular mockup. The game references
show rounded selection frames and translucent silver panels; clipped corners, saturated lime
fills, all-caps block headings and solid dark settings cards should not define the theme.

Inspected references:
- Local game captures: /tmp/opencode/main_menu.png and /tmp/opencode/options_menu3.png.
  The top carousel/title/footer are game UI. The large circular touch buttons are Android
  additions and must NOT be used as evidence of the original game's visual design.
- Original Display settings screenshot in TechTudo's PC requirements guide:
  https://www.techtudo.com.br/noticias/2015/12/confira-os-requisitos-para-jogar-need-speed-underground-2-no-pc.ghtml
- Race HUD reference (IGDB screenshot): https://images.igdb.com/igdb/image/upload/t_1080p/sc10v13.png
- Main-menu cross-check: https://github.com/ThirteenAG/WidescreenFixesPack/issues/1081

Observed element -> native app adaptation:

| Game element | Observed treatment | App use |
|---|---|---|
| Title/header | Black translucent band; narrow oblique yellow-green title, often title case | Launcher section header, dialog title, editor header |
| Main navigation | Horizontal pale translucent icon strip, circular arrows, selected rounded lime luminous frame, caption below | Top-level launcher categories with real touch/controller focus |
| Settings sheet | Silver-gray translucent body over garage scene; dark text; dark selected row with lime text/value | Graphics, controls, game and support option lists |
| Divider/position track | Thin gray rounded rail with a small circular marker | Header separators, category position indication, sliders |
| Footer actions | Slim white/gray rounded outline, dark translucent interior, condensed italic label | Back, Continue/Play, Apply, Cancel and dialog actions |
| Menu typography | Narrow oblique/italic, lighter than a heavy all-caps display face | Section labels and actions; keep long explanations readable |
| HUD standings | White bold italic labels/numbers, dark shadow, minimal black translucent backing | Benchmark values, status/telemetry overlays |
| HUD instruments | White ticks and needle, gray ring, dark transparent face; blue meter and red high-range band | Optional control/gauge accents and analog input feedback |
| Minimap | Restrained gray circular rim, translucent interior, sparse colored directional markers | Steering control visual proportions; no repeated decorative gauges |

Color values are still provisional: reference pixels are alpha-composited over a scene and
must not be treated as original raw asset colors. Use soft yellow-green for menu headings and
selection glow, silver/neutral gray panels, white/gray outlines, dark text on pale menu bodies,
and white HUD numerals. Preserve the HUD's blue/red semantic accents. Do not tint everything
green or use a neon-green primary fill for every action.

Typography needs two distinct matched roles: narrow oblique menu text and heavier italic HUD
numbers. The logo is a separate logotype, not a normal UI font. Exact font identity has not
been established; compare candidate letterforms (a/e/g/R, numbers, slant, width and spacing)
against references before claiming a match. No proprietary font/image extraction is performed
by this evaluation. Do not substitute generic bold italic Arial and call it authentic.

## Revised implementation sequence

1. Make a reference sheet with separate menu, settings, footer and HUD examples. Match one
   title, selected icon tile, settings row, slider, footer action and telemetry block in a small
   prototype before restyling complete screens. Determine font source/license and asset policy.
2. Build shared menu components: title band, translucent neutral settings sheet, rounded
   selection glow frame, circular navigation arrow, rail/marker and outlined footer action.
   Define separate menu/HUD font roles and semantic states in resources plus a Canvas adapter.
3. Rework launcher navigation around the game's icon/category strip and content sheet. Preserve
   Play/Resume discoverability, current settings/navigation and mobile-sized targets. Use a
   locally available approved static garage reference/background as a prototype first; choose
   the shipped backdrop explicitly rather than adding a costly live 3D launcher scene.
4. Apply the same menu components to all dialogs, pickers, forms, graphics/controls/game/saves/
   support screens and editor actions. Own dialog title/body/actions to avoid OEM mismatches.
5. Adapt racing controls with HUD-derived neutral translucent faces, measured rim/tick weight,
   white icons and restrained active-state accents. Keep controller identification colors,
   pedal differentiation, saved layouts and hitboxes. Utility dock/editor uses menu vocabulary;
   live benchmark uses HUD typography and telemetry vocabulary.
6. Compare side by side with actual game screenshots at matched scale. Verify phone/tablet,
   text size/contrast, controller navigation/focus and bright/dark race backgrounds. Use static
   or cached selection glow and draw only on changes; no continuous blur or expensive animation.

Each stage is a reviewable commit. This pass changes the plan only, not the app.

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

## First implementation pass — 2026-10-29

Shared MenuTheme + semantic menu_colors resources now drive launcher sheets, headers,
rail/marker separators, rows, actions and value pickers. Silver sheets use dark text; headers
use black bands with soft lime captions. Selected values use lime framing; D-pad focus uses
white framing; pressed and disabled states are separate. Actions have translucent dark faces
and outlines, with no gradient fill. Rows/actions have 48dp minimum targets. Picker width is
bounded by available width and a 520dp cap; its sheet is less transparent for readability.

Bundled unmodified Barlow Condensed Regular and Medium Italic from
https://github.com/google/fonts/tree/main/ofl/barlowcondensed as a visual approximation,
NOT an identified original game font. Original filenames: BarlowCondensed-Regular.ttf and
BarlowCondensed-MediumItalic.ttf. The OFL license is included in the APK at
assets/licenses/BarlowCondensed-OFL.txt. Menu actions/headings use oblique; help copy uses upright.

Verified on SM-S938U1: launcher, Visual settings, performance preset picker and selected Vulkan
system-driver picker. D-pad navigation switches focus frames and activates picker Back;
no game options were changed. Debug APK build and phone source checks pass. Font license and
both font files verified in APK. Contrast checked against the darkest launcher sheet composite;
small status/body colors were darkened. Full physical Backbone traversal, large-font/tablet
screens and overlay performance remain to verify.

This is foundations + launcher/picker styling only. The original navigation structure remains;
icon-category navigation, backdrop/logotype, all framework dialogs, racing controls/editor,
benchmark overlays and HUD-specific typography are subsequent passes. No gameplay code changed.


## Integrated panel frames and submenu titles — 2026-10-29

Panels now have rounded 4dp black outer frames, with their title in the continuous black top
band. Shared heading layout keeps the title flush with the frame and pads the gray content
separately. This applies to launcher panels, contextual information and value pickers.
The right-hand header is driven by the same group-title mapping as the contextual pane:
Options at the root; Visual settings, Display detail, Controls, Game, Saves or Support inside
the corresponding submenu. Refreshing a value does not reset the title to Options.
Build passed and installed; home, Visual settings, Controls, picker layout and Back navigation
checked on the S25 Ultra. No game settings were changed.

## Separate translucent sheets and pinned navigation — 2026-10-29

Latest user refinements implemented:
- Back is above, not inside, the scrolling list in submenus and value pickers; default outline
  is lime. Root Options hides Back. Submenu navigation resets the list to the top.
- Current values are lime on small translucent dark fields, separating values from row captions.
- Left contextual panel and home Game Files panel both start 8dp beneath the same logo/header.
- Subtitle is exactly Android Evolved Recomp. The global apply-on-play note is removed.
- The old header rail is replaced by a small black gap and distinct content-sheet corners.
- MenuPanel owns a rounded translucent black title/frame and a separate gray body with all four
  corners rounded. Its frame painter leaves a matching hole behind the body: gray is not painted
  over an opaque black rectangle, so transparency and the corner shape remain correct.
- Main/popup gray alpha is 184/208 of 255; outer black frame alpha is 204. Existing framework
  dialog background also changed from nearly opaque green-black to translucent neutral dark.

Verified on phone: home and submenu geometry, pinned Back while scrolling Visual settings,
value highlights, transparent minimap picker, top-Back preset picker, dismissal and returning
home. No options were changed. Final debug build and phone source checks pass. Main body/secondary
contrast over the plain dark backdrop measures 4.84/4.71:1. Future image backdrops must be checked
again because transparent surfaces composite differently over different scenes.


## Play/Resume glow and full-row backgrounds — 2026-10-29

GlowActionButton gives the existing Play/Resume action a 2.8-second green halo pulse, moving
surface highlight and text shimmer. Both labels use the same view. It schedules only its own
redraw at 34ms intervals, cancels callbacks when paused/hidden/disabled/detached or unfocused,
and remains static when Android disables animations. Cached gradients/paint avoid per-frame
shader allocation; halo uses layered strokes rather than a software/full-screen blur.

Removed separate value-background pills. Default rows use a subtle black overlay across the
whole row; captions are darker for contrast and lime values have a small dark drop shadow.
Selected/focused/pressed states remain separate, including existing controller focus frames.

Debug build/source checks passed and installed on SM-S938U1. Home and submenu appearance
checked. A roughly 3.2s gfxinfo window rendered 95 frames, no janky frames, 5ms CPU 95th and
3ms GPU 95th percentiles. With Play hidden in Visual settings, a separate 3.2s idle window
rendered zero frames. These are brief launcher checks, not sustained game/battery measurements.
No game was launched and Resume text/system-animation-disable were not separately exercised.
Preview recording: /tmp/nfsu2-play-shimmer.mp4; still: /tmp/nfsu2-glow-home.png.

### 2026-10-29 — Compact labels and persistent launch action

Use one/two-word row titles and value labels; put necessary explanations in confirmation/help
content, never in the value itself. Keep real filenames, hostnames and build identifiers intact.
Play/Resume remains visible in settings. Visual settings/Display detail use a persistent aggregate
Estimated demand panel, updated from current configuration. Do not replace it with whichever
option was last touched. Bars are relative CPU/GPU demand estimates, not live utilization.

### 2026-10-29 — Fixed launch footer and finer demand bars

Play/Resume is anchored below the left scrolling pane, at the bottom of the launcher columns.
CPU/GPU use ten lime segments each; both share the same active color. Segment count follows
relative demand score while Low/Medium/High/Very high labels retain their tier thresholds.
Real rendered preview feasibility/next steps: VISUAL_PREVIEW.md (not yet implemented).

### 2026-10-30 — Preview removed; in-game Graphics panel

The launcher preview, its isolated service and the native showcase runtime are
removed. Display detail is no longer a launcher group, and the game's native
Display page is removed from both Options carousels after every lift. A themed
in-game Graphics panel is hosted by GameActivity (MenuPanel frame/sheet, lime
oblique headings, silver rows) and is reachable from the expanded racing dock
and the pause/frontend icon row. It reuses the launcher's silver/black/lime
language and applies the same persisted GameOptions values live.
