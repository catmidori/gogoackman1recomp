# Hotkeys menu and individual save slots

The Hotkeys tab contains 33 directly selectable rows, with no dropdowns or
configuration buttons. The first 13 rows are alphabetized; ten Save rows and
ten Load rows follow. The list scrolls independently, keeping the tabs visible.

## Configuration dictionary

All entries belong to `[KeyMap]` in the executable-relative `config.ini`.

| Menu label | INI key | Default |
| --- | --- | --- |
| Audio volume down | VolumeDown | Keypad - |
| Audio volume up | VolumeUp | Keypad + |
| Decrease screen size | ScaleDown | i |
| Fullscreen | Fullscreen | Alt+Return |
| Increase screen size | ScaleUp | o |
| Mute | Mute | m |
| Pause | Pause | p |
| Reset | Reset | Ctrl+r |
| Rewind | Rewind | 8 |
| Screenshot | Screenshot | y |
| Show FPS | ToggleFPS | f |
| Turbo | Turbo | Tab |
| Widescreen | ToggleWidescreen | u |
| Save slot 1–10 | SaveSlot1–SaveSlot10 | Shift+F1–Shift+F10 |
| Load slot 1–10 | LoadSlot1–LoadSlot10 | F1–F10 |

`SaveStateMenu` has no listener and is removed during migration. Legacy `Save`
and `Load` arrays are converted to individual slot entries, preserving supplied
values and filling missing slots with defaults. An explicitly empty slot stays
unbound. The current project configuration required just one relocation:
**Rewind: F8 → 8**.

During this schema upgrade, another system command using plain F1–F10 or
Shift+F1–Shift+F10 is relocated to the corresponding digit, retaining Shift.
F10 maps to 0. Ctrl/Alt combinations remain distinct. Once all canonical slot
keys exist, subsequent boots preserve intentional remappings and conflicts.
`PauseDimmed` and `ToggleRenderer` remain compatible INI-only commands; their
bindings are included in conflict checks.

## Capture, conflicts and live execution

Activate a row with the mouse, Enter or gamepad navigation. Its middle column
shows `Press button (press Esc to cancel or Del to clear)`.

- An ordinary keyboard key commits immediately as a single key.
- Alt, Ctrl or Shift pressed first starts a chord; following held modifiers and
  the first ordinary key are serialized, for example `Ctrl+Shift+F2`.
- Releasing the modifier without another key assigns that modifier alone.
- A gamepad button or directed axis commits immediately as one input, using
  SDL names such as `Gamepad a` or `Gamepad Axis lefttrigger +`.
- Esc cancels without writing. Del clears, keeping the INI key with an empty
  value. Del also works directly on a focused row.

The update uses the existing atomic INI writer, then reloads the native keyboard
map and the wrapper's gamepad dictionary. The initiating row regains visible
navigation focus. Captured keys/buttons are quarantined until release so they
cannot immediately activate the new shortcut or another menu item. Losing
window focus cancels capture. Axis activation uses separate press/release
thresholds; Turbo releases correctly even if a chord modifier is released first.

Conflict comparison requires the same device, key/button/axis direction and
exact modifier set. Modifier order and left/right variants are normalized;
Return and Alt+Return are different bindings. All active system entries and all
twelve Player 1 gameplay bindings participate. Warnings use friendly function
names in the smaller red `Already used in [function]` format. Conflicts warn
without rejecting the user's selection.

Controls and Hotkeys query the same exact tuple comparator and friendly function
dictionary in both directions. Controls lists every matching Hotkeys command,
including multiple commands assigned to the same input, rather than only the
first native lookup result. Both tabs also share `AckmanGamepadLabel` for display:
serialized `Gamepad b` displays as `B`, shoulder/trigger buttons use `LB / L`,
`RB / R`, `LT / L2`, `RT / R2`, and axis directions use the same analog labels.
Only presentation changes; persisted SDL binding strings remain intact.

## Shared geometry and implementation

The tab reuses the existing three-column binding row: left label column expanded
by 10%, unchanged middle capture column, and right warning column reduced by 10%.
Highlights retain the global full-row gray fill, single white outline and space
padding. Drawing is clipped vertically to the scrolling child area.

- `src/ackman_hotkeys.c`: shared descriptors, capture, exact conflicts and gamepad
  dispatch to native host commands.
- `src/ackman_menu.c` / `src/ackman_menu_imgui.cpp`: event routing, focus, scrolling
  and visual rows; File slot labels use the same current bindings.
- `src/ackman_config.c`: defaults, atomic deletion/clearing and migration.
- `patches/ackman-hotkeys.patch`: native individual slot keys, obsolete listener
  removal and a command bridge. CMake reproduces all 19 framework patches.

## Validation — 11 October 2026

Release CMake configure/build succeeded. All 14 CTests passed, including real
SDL/ImGui keyboard and virtual-gamepad capture, modifier-first chords, exact
conflicts, punctuation keys, cancellation, clearing, restored focus, column
dimensions and navigation/scrolling through Load slot 10. The suite also checks
fullscreen and both font fallback paths.

Cross-tab regression coverage verifies the complete friendly gamepad table in
both tabs, matching keyboard/gamepad axes in both directions, all duplicate
functions, friendly names (including `Widescreen`, `Show FPS`, `Audio volume up`
and `Save slot 10`), exact modifier exclusions, and immediate warning removal
after either binding is cleared.

The 1000-frame headless smoke passed with live presentation/audio settings.
Additional runtime validation boots a private copy of the user's configuration,
creates all ten strict-named `GGA1_save_XX.sav` files and sends F1–F10 to the real
host after migration. User saves are untouched. Interface captures are available
in `private/hotkeys-candidate/menu-hotkeys-top.png` and
`private/hotkeys-candidate/menu-hotkeys-slots.png`; these use a legacy test fixture
with intentional conflicts, rather than the project's default slot bindings.
