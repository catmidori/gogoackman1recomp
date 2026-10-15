# Controls remapping

The menu tabs are File, Game, Graphics, Audio, Controls, Hotkeys and Mods.
Hotkeys is the system-shortcut tab associated with `[KeyMap]`; its 33 rows
support keyboard/gamepad capture and individual save slots. See
[HOTKEYS.md](HOTKEYS.md). Gameplay controls are independent.

## Configuration

`[Controller.Keybinds.Player1]` stores SDL gamepad/joystick inputs.
`[Keyboard.Keybinds.Player1]` stores SDL scancode names. Both sections use
`up, down, left, right, a, b, x, y, l, r, select, start`.
The SNES L/R shoulders use INI keys `l`/`r`; these are configuration field names,
not forced keyboard keys. Existing `q`/`w` values migrate without changing their
assigned inputs. Explicitly empty canonical `l`/`r` entries take precedence.
Player 2 keeps its existing compatible schema.

Both dropdowns expose all twelve individual rows in this order: Up, Down, Left,
Right, A, B, X, Y, L, R, Select, Start. L/R share the same capture, cancellation,
clearing, cross-tab conflicts and friendly gamepad labels as the other rows.

The old `[Keybinds.Player1]` contained keyboard keys: these migrate into the
Keyboard section, preserving user mappings. A canonical Keyboard section takes
precedence if both old and new sections exist. Controller defaults use the SDL
standard layout: D-Pad, A=East, B=South, X=North, Y=West, L/R=shoulders,
Select=Back, Start=Start. Standard D-Pad bindings also accept the left stick.

The distributed/generated INI contains no Player2 section. Existing mod files
with `[Keybinds.Player2]` remain readable and writable, and the runner's second
controller seat remains available. Ordinary single-player saves do not generate
Player2 bindings.

Controller binding formats:

| Format | Example | Meaning |
| --- | --- | --- |
| SDL logical button name | `a`, `dpup`, `leftshoulder` | Device-independent gamepad button; legacy `south/east/west/north` also accepted |
| Signed logical axis | `Axis lefty -` | Negative half of the left vertical stick |
| Raw joystick button | `Joystick Button 4` | Button index on an unmapped joystick |
| Signed raw axis | `Joystick Axis 0 +` | Positive half of a raw joystick axis |
| Raw hat direction | `Joystick Hat 0 4` | SDL hat direction bit; here Down |
| Disabled input | empty value (legacy `None` accepted) | No controller input for that SNES button |

## Wizard

The Controls tab has two independent collapsible branches, Gamepad Remapping
and Keyboard Remapping. Each contains Configure all buttons followed by twelve individual rows: Up, Down,
Left, Right, A, B, X, Y, L, R, Select, Start. Configure all buttons has a phrase-sized single white outline,
transparent unless focused; its full-width row uses the shared gray highlight
and completing/cancelling restores visible gray focus
to that button. Rows inherit the
shared full-width gray highlight, single border and safe padding; labels align
left in the invisible asymmetric three-column layout.

The centered black modal has a 2:1 aspect ratio and 90% background opacity. Its
two centered instructions are `Press button for <SNES_BUTTON>` and `Press Esc to cancel`,
separated by a complete blank text row. The sequence displayed is exactly
UP, DOWN, LEFT, RIGHT, A, B, X, Y, L, R, SELECT, START.
The underlying menu remains visible and the core/audio retain the overlay pause.

All pending mappings live in RAM. Esc consumes the event and closes the wizard
without closing the main menu or modifying the current mappings/INI. Closing
the application also discards an unfinished session. Completing Start writes all
12 entries through the atomic INI writer, then applies the new map. A failed
write leaves the live map unchanged. Keyboard and controller commits affect only
their respective section and preserve `[KeyMap]` and other settings.

During capture, the wrong input device, mouse actions, Q/E, LB/RB and system
hotkeys cannot navigate or activate the underlying menu. ImGui's polling-based
gamepad navigation is also suspended. Keyboard repeat is rejected; a key/button
must be released before the next step. Axes require magnitude above 16000 and
return below 8000 before another capture. The first accepted controller locks
the session to that device. Raw joystick events from recognized SDL gamepads are
ignored to prevent duplicate captures from the two SDL event streams.

`src/ackman_controls.c` owns capture and controller polling. The framework host
uses the resulting runner-order SNES mask instead of its old Player1 button and
axis mapping, preventing ghost gameplay inputs after remapping. Player2 and
explicit framework gamepad system commands remain compatible.

## Validation

The existing 14 CTests cover the new section defaults, legacy migration,
Player2 compatibility, modal centering/aspect ratio, keyboard navigation,
gamepad navigation, exact prompt sequence, wrong-device rejection, axis
neutral/release gating, byte-identical cancellation, no intermediate writes,
complete commit/reload, keyboard runner masks, swapped gamepad buttons and raw
joystick button/axis/hat mappings. The popup capture is exported as
`build-validation/menu-remapping-popup.bmp` by the UI test.

The full 19-patch stack also applies to the clean pinned framework. A 1000-frame
smoke validates gameplay, video/audio hot reload and persistence independently
of the UI fixtures; ROM and user savestates are not modified by these tests.

## Global dropdown widget

`AckmanMenuDropdown` is the common implementation for Save, Load, Gamepad
Remapping and Keyboard Remapping. It draws the original Save/Load triangle
independently of the label, with the same size, text-relative offset and color.
The label and ImGui ID remain constant while the disclosure rotates. File keeps
its centered alignment; Controls keeps its left alignment. Keyboard/gamepad
activation retains a visible gray header focus until Down moves to the first
child. UI regressions verify the actual glyph positions, filled highlight and
navigation IDs through expand/collapse, using both keyboard and a virtual pad.

## Individual bindings and conflicts

Each row displays its live mapping on the right, with friendly SDL gamepad names
(Dpad directions, face buttons, LB/L, RB/R, LT/L2, RT/R2, stick clicks and signed
analog directions). An unbound key is displayed as a blank parameter.

Activating a row starts device-specific inline capture. Its parameter becomes
`Press button (press Esc to cancel or Del to clear)`. Esc preserves the previous
value. Delete clears either the active capture or a directly focused row; the
INI key remains present with an empty value. Capturing a binding persists that
single entry atomically before applying it to the live input map. Failed writes
leave the original live mapping intact. Individual capture changes only the selected entry, including L/R. Navigation returns to the initiating row with visible gray
focus after commit, clear or cancel. Captured controller buttons/axes are drained
until released/neutral, preventing a newly mapped Back/B or direction from being
interpreted a second time by ImGui's polling-based navigation.

Duplicates remain selectable. A smaller red `Already used in [function]` warning
appears immediately after the parameter and lists matching gameplay/system
functions. Gameplay comparisons include all twelve buttons, including L/R and overlapping D-Pad/left-stick defaults. Keyboard conflicts consult the
live configurable host command map with exact key/modifier equality and
save/load slots, rather than hardcoded shortcut keys. Gameplay keyboard bindings
are individual scancodes without chord modifiers; Return therefore does not
conflict with Alt+Return, nor F2 with Ctrl+Shift+F2. A plain F2 system shortcut
still conflicts with a plain F2 gameplay binding.
Explicit native gamepad system commands are also checked. Warnings wrap inside their dedicated right column instead of clipping outside
the panel. Each conflicting function is enclosed in brackets.

The fourteen-test suite now also covers immediate single-entry persistence,
empty-value reload, failed-write safety, all friendly gamepad labels, duplicate
warnings, direct Delete, inline cancellation, restored focus after both capture
modes and keyboard/virtual-gamepad navigation. UI captures are exported to
`build-validation/menu-individual-capture.bmp` and
`build-validation/menu-individual-conflict.bmp`.

## Shared cursor and asymmetric binding columns

All common menu rows use one navigation focus ID for mouse, keyboard and pad.
Mouse activity resolves that ID from the previous frame's row rectangles before
highlights are drawn, so the old keyboard row and a hovered row cannot remain
highlighted together. Arrow/D-Pad navigation continues from the hovered item.
The Configure all buttons outline remains white and phrase-sized, while its
interactive/focused row spans the full common menu width; moving focus removes
the gray fill immediately.

When SDL hides the pointer after 2000 ms, ImGui's NoMouse flag prevents backend
position polling or queued mouse positions from changing focus. The last row
stays highlighted and keyboard/gamepad navigation remains enabled. A hidden
pointer's first click is consumed on both press and release: it only wakes the
pointer, without activating a widget or moving focus. Real motion or a subsequent
click restores mouse interaction.

Controls uses an invisible three-column table: the first fixed width fits the
longest SNES label; the second fixed width fits the complete capture instruction
and centers all current values in that same column; the third consumes the
remaining width and draws smaller red warnings aligned left. The widths are
measured again at the current uniform menu font/DPI scale. The menu font fit
also reserves space for the full instruction and conflict column at small sizes.
Regression tests inspect actual table widths, rendered focus pixels, mouse-to-
keyboard/pad transitions, timer hiding, ignored hidden position polling and the
no-action wake click, in all existing font/fullscreen test variants.

The Configure all buttons static white outline is inset vertically by one
physical pixel inside the row. This protects all four walls at fractional
font/DPI scales without changing the shared full-row highlight geometry.
Rendered-pixel regressions check every edge, and conflict regressions distinguish
Return, Alt+Return, Shift+Return, Ctrl+Return and combined modifier shortcuts.

## Twelve-row validation

All 14 CTests pass with individual keyboard and virtual-gamepad navigation
through all twelve rows. Shoulder tests verify q/w-to-l/r migration, canonical empty
value precedence, persistence, cancellation, clearing and native L/R input bits.
Captures in private/shoulder-buttons-candidate show all twelve labels, including
LB / L and RB / R in the gamepad list. The 1000-frame smoke also passes.
