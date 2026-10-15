# Unified configuration — October 7, 2026

> Historical implementation report. Later migrations are documented in the linked guides and the current README; the original examples and validation counts below describe their respective development stages.

The production file is `config.ini`, beside the executable. At this stage the root file used six sections: `General`, `Graphics`, `Sound`, `KeyMap`, `Keybinds.Player1`, and `Keybinds.Player2`.

`rom.cfg`, `keybinds.ini`, and `config.local.ini` are not read. There is no include layer or automatic overlay. `--config <arquivo>` selects one alternative file, also used by the ROM picker and control editor. Explicit ROM arguments and diagnostic environment variables remain available to validation runners; they are not additional configuration files.

## ROM and persistence

`[General] Rom` stores a quoted ROM path. Backslashes are literal and need not be doubled. The picker validates the ROM and updates that same key before restarting. Without an explicit argument, the configured ROM takes precedence over an incidental ROM beside the executable.

If configuration is absent, the host generates defaults with `Rom = ""`; this machine's personal path is not embedded in the executable. Subsequent selection fills the key. Write failures are reported.

The shared C reader accepts a UTF-8 BOM, CRLF, case-insensitive section/key names, quoted values, and `#`/`;` comments. Quotes protect those characters in paths. Writing preserves unchanged sections/comments and publishes a temporary file only after a complete write. ROM, core preferences, and controls use this mechanism.

## System shortcuts

Only `[KeyMap]` registers host keyboard commands. There are no implicit save/load shortcuts. Historical example:

```ini
[KeyMap]
Pause = p
Save = Ctrl+Shift+F2
Load = F2
```

Here F1/Shift+F1 stop saving/loading. `None` or an empty value disables an action; missing keys do not inherit old invisible shortcuts. Accepted modifiers are `Shift+`, `Ctrl+`, `Alt+`, and `Gui+`. Caps Lock/Num Lock do not prevent recognition. Defaults include `Fullscreen = Alt+Return` and the dictionary below. Esc opens/closes the paused ImGui overlay; Q/E and LB/RB switch tabs. See [OVERLAY-HUD.md](OVERLAY-HUD.md). While Alt is held, only INI system shortcuts pass: Enter reaches neither gameplay Keybinds nor ImGui confirmation. Chord keys remain quarantined until their own key-up.

The audit covered the original dictionary/defaults in `desktop/mmx_config.c` at the pinned HEAD, SDL event pumps and `HandleCommand` in `desktop/host_main.c`, the OpenGL presenter, and overlay navigation. `src/main.c` supplies `AckmanDefaultConfig`; dynamic resolution remains in the runner's shared parser rather than a duplicate parser in `main`.

| Key in `[KeyMap]` | Default | Function at this development stage |
| --- | --- | --- |
| Fullscreen | Alt+Return | Toggle fullscreen |
| ToggleFPS | f | Toggle the OSD FPS counter |
| Turbo | Tab | Accelerate while held |
| Mute | m | Toggle mute while preserving `Sound.Volume` |
| ScaleUp | o | Increase window scale |
| ScaleDown | i | Decrease window scale |
| Reset | Ctrl+r | Full reset through the Ackman menu reset path |
| Pause | p | Toggle pause |
| PauseDimmed | Shift+p | Pause with frame dimming on Windows |
| Save | Shift+F1 | Save slot 1 |
| Load | F1 | Load slot 1 |
| VolumeUp | Keypad + | Increase volume by five points |
| VolumeDown | Keypad - | Decrease volume by five points |
| ToggleRenderer | r | Toggle PPU renderer; widescreen retains the optimized renderer |
| SaveStateMenu | F7 | Open the runner savestatee browser |
| Rewind | F8 | Open the rewind panel |
| Screenshot | None | OpenGL capture; unavailable in SDLRenderer3 at this stage |
| ToggleWidescreen | u | Toggle adaptive widescreen/native 4:3 framing |

`ToggleFPS`, `ScaleUp`, and `ScaleDown` alias the original `DisplayPerf`, `WindowBigger`, and `WindowSmaller` commands; the old names remain accepted. Use one name per action. `Mute` was added because this runner revision had no such command. It applies to the session without disabling the APU/device. Adjusting volume while muted preserves silence and changes the level restored when unmuting. Repeated key-down events do not repeat toggles.

This revision has no F11 shader reload command. Original F11/F12 defaults opened save/rewind browsers; this stage exposed them through F7/F8 under the existing `state_menu_hotkeys` contract. No reload action was invented. Arrows/Enter/Esc/digits, ImGui mnemonics, and Ctrl+wheel/Shift+double-click remain contextual rather than global `[KeyMap]` commands. Diagnostic F5/F6 require `ACKMAN_DEBUG_KEYS=1`.

Comma-separated lists still support later slots. The menu queries the core's resolved map for each slot's shortcut. Repeats and key-up do not repeat save/load. Selected interface commands remain queued during drawing.

## Gameplay controls

`[Keybinds.Player1]` and `[Keybinds.Player2]` contained twelve individually named buttons, with SDL scancode names and `None` for an unbound button. The editor writes these sections to the selected file while preserving other preferences. Each player has a separate map.

The historical `[KeyMap] Controls` line was removed from the shipped configuration and template: the parser ignored it, so it never supplied gameplay input. Player sections are authoritative. Old `Controls`/`ControlsP2` entries remain accepted without registering parallel bindings.

Player 1 defaults to keyboard; Player 2 is disabled. Per-player `Source` selects 0=disabled, 1=keyboard, 2=gamepad. Identical maps do not automatically enable both players. The game is single-player. Persisted advanced launcher settings remain in the same INI and six sections.

## Implementation and validation

- `src/ackman_config.c/.h`: defaults, shared reader/writer.
- `src/ackman_menu.c`: save/load through resolved mappings without intercepting F1.
- `src/ackman_menu_imgui.cpp`: dynamically queried shortcut labels.
- `patches/ackman-config.patch`: host parser, ROM cache, controls, player source, and removal of local overlays.
- `patches/ackman-system-shortcuts.patch`: aliases, mute, safe reset, and repeat suppression.
- `CMakeLists.txt`: module, patches, and `unified_configuration` test.

Windows Release in `build-validation`: **12/12 CTests passed**, a **1,000-frame** smoke with `tests/enter-game.txt`, and a **120-frame** startup without a ROM argument using an explicit configuration and deliberately conflicting legacy files. Clean patch application/reverse checks passed. The 51-save regression was not repeated.

Evidence: `private/config-unified-candidate/validation.json`, `smoke.log`, `smoke.csv`, `selected-config.log`, and `build-validation/Testing/Temporary/LastTest.log`. Previous binary/config files were preserved in `private/pre-unified-config/`. The root executable was promoted only afterward.

Promoted executable SHA-256:

```text
dec0b282d026fb166f7ea3e1012faf2b0fff0d03635daad98c3841096ee08311
```

## Validation after restoring shortcuts

Windows Release: **12/12 CTests** and **1,000-frame smoke** passed. Tests check active defaults, remapping the six requested commands, absence of old shortcuts after reload, historical aliases, removal of `Controls`, and the remapped fullscreen shortcut passing through a focused menu. Clean application/reverse checks passed. Headless execution does not demonstrate audio perception or physical fullscreen; the 51-save regression was not repeated.

Evidence: `private/system-shortcuts-candidate/validation.json`, `LastTest.log`, `smoke.log`, and `smoke.csv`. Previous binary: `private/pre-system-shortcuts/AckmanRecomp.exe`.

Promoted executable SHA-256:

```text
7af1947f3effef2e3419acc5d2c7a8ce1015dec59921fadb551902a7b09ed5ef
```

## Functional widescreen switching

`ToggleWidescreen = u` selects the Ackman presentation policy without changing camera, collisions, or guest logic. It is remappable through `[KeyMap]`. At this stage `[Graphics] IgnoreAspectRatio = 0` started native mode and `1` adaptive mode; defaults used `1` to retain widescreen. This selects expansion rather than stretching; pixel aspect remains 7/6.

Native mode has zero PPU extension, a 256x224 frame, and a centered 4:3 viewport. Existing SDL/OpenGL clears fill exterior areas black. At 1920x1080 the viewport is 1440x1080, leaving 240-pixel sidebars. Displays narrower than 4:3 produce letterboxing without distortion.

Adaptive mode restores up to 64 pixels per side under the same scene policies, masks, and VRAM restoration. A shortcut changes the next presentation; a paused host redraws immediately. No reset, state reload, or renderer/audio recreation occurs. The host resizes textures when frame dimensions change. This host preference is not part of guest savestatees. INI edits are read at boot; the shortcut changes memory state.

`after_config` and `compute_viewport` callbacks in `src/main.c` link initialization and viewport calculation to video. `AckmanVideoFrameSize` resolves pixel budgets; `AckmanVideoComputeViewport` preserves aspect in both modes. `ackman-system-shortcuts.patch` passed reconfiguration and reverse checks of previous patches. Root `config.ini` comments are technical English.

Validation: **12/12 CTests**, including 500 geometry/VRAM-preserving switches, and a separate **1,000-frame smoke**. Integration ran **6,000 frames** across two fixtures (stage 1 and car floor), comparing adaptive, native, and alternating modes. **400 SDL-pump toggles** used a shortcut remapped to F9. RAM/VRAM/OAM/SPC and central pixels remained identical; captures confirmed 256/384 widths. These headless checks do not measure perceived smoothness or physical tearing. The complete 51-save regression was not repeated.

```powershell
& C:/msys64/mingw64/bin/python.exe scripts/validate_aspect_toggle.py `
  --exe build-validation/AckmanRecomp.exe `
  --rom 'R:/ChatGPT/Go Go Ackman (Japan).sfc' `
  --out private/nova-validacao-aspecto
```

The runner gained `hostkey <nome SDL>` for unmodified taps through normal events. Original fixtures are untouched; tests use isolated copies. Evidence: `private/widescreen-toggle-candidate/final-integration/results.json`, `smoke.log`, `smoke.csv`, and `LastTest.log`. Previous binary: `private/pre-widescreen-toggle/`.

Promoted executable SHA-256:

```text
c82648b58366ff3ee04793ebbf987830858f567750aa3c0f2aff10b499565094
```
