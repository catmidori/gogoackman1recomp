# recomp_menu_core

A menu library for SNESRecomp desktop hosts. Its CMake target is `recomp_menu_core`, with the alias `SNES::MenuCore`. The public API is C; the visual implementation uses C++17, Dear ImGui, and SDL3.

## Separation of responsibilities

| Layer | Files | Responsibility |
| --- | --- | --- |
| Engine | `src/recomp_menu_core.c/.h` | Relative directories, dialogs, mod/shader models, entry snapshots, consolidated exit decisions, delegated persistence, and cold boot |
| Visual engine | `recomp_menu_ui.cpp`, `recomp_menu_widgets.cpp` | 4:3 window, blur, tabs, clipping, columns, padding, unified cursor, Home, vertical wrapping, dropdowns, selectors, and conflict alerts |
| Assets and fonts | `recomp_menu_icons.cpp`, `recomp_menu_svg.c`, `recomp_menu_font.cpp` | Embedded SVGs, rasterization/cache by scale, tinting, navigation legends, and native/fallback fonts |
| SNES input | `recomp_menu_controls.c`, `recomp_menu_hotkeys.c`, `recomp_menu_devices.c`, `recomp_menu_cursor.c` | Capture/remapping, exact conflict comparison, asynchronous device publication, single-player routing, and 2000 ms inactivity tracking |
| Recovery | `recomp_menu_checkpoint.c`, `recomp_menu_boot.c` | Atomic checkpoints, mandatory restoration, a separate supervisor process, attempts with the new profile, and up to five backups in `fallback/` |
| Shared data | `recomp_menu_snes.h`, `recomp_menu_types.h`, `recomp_menu_metadata.c` | Eight tabs, the 12-button matrix, and SNESMOD/MSU XML/BML manifest reading |
| Ackman adapter | `src/ackman_menu.c` | Executable/save identity, local INI bindings, callbacks, and native conversion of MD5-registered translations |

The old `ackman_menu.h`, `ackman_boot.h`, `ackman_controls.h`, and related headers are compatibility facades with aliases to the engine. They contain no second visual or watchdog implementation. The library does not include Ackman headers or call game symbols directly.

## Host contract

`RecompMenuHost`, declared in `recomp_menu_host.h`, contains the project's names/strings and callbacks for:

- INI reading and atomic publication of configuration and active mods.
- ROM validation and host action dispatch.
- Translation import and converter progress/errors.
- Game-specific widescreen and haptic feedback hooks.
- Immediate audio pause before native dialogs.

`RecompMenuBindHost()` copies the table and rejects invalid versions, sizes, or mandatory callbacks, preserving the previous profile. Strings and functions must remain valid throughout execution. Register the profile before boot and do not replace it while the interface or workers are active. In Ackman, `AckmanMenuRegister()` performs this registration before `RecompMenuBootEnter()`.

The local names `AckmanRecomp.exe`, `GGA1_autosave.sav`, and `GGA1_save_XX.sav` belong to the adapter. The checkpoint receives its directory and filename explicitly through `RecompMenuAutosaveConfigure(directory, filename)`. On Windows, cold boot uses the current process's actual path and preserves focus/fullscreen flags.

The engine uses the standard SNESRecomp runner contract (`Config/g_config`, keybinds, OSD, run-ahead, and Rtl snapshot APIs), including this project's extensions. Another host must supply a compatible contract and its own callbacks; the library does not replace the emulation core.

## CMake integration

Include `cmake/RecompMenuCore.cmake` after SDL3, Python3, and the runner's `snesrecomp_target_sdl` helper are available. Enable C and C++17 in the consuming project.

```cmake
include("path/to/menu/cmake/RecompMenuCore.cmake")
target_link_libraries(OtherSnesRecomp PRIVATE SNES::MenuCore)
```

The module resolves sources, NanoSVG, Dear ImGui, and assets relative to its own directory. SVGs and the fallback font are compiled into the binary; Python only participates in build-time asset generation. Registered translations continue to be converted natively in C by the Ackman adapter.

## Preserved behavior

Opening the menu captures the driver, active mods, shader enable state, and selected shader. Closing without changes, or changing only a selection while shaders remain globally disabled, does not restart the game. Other valid changes pass through driver sanitization, a mandatory checkpoint, a consolidated commit, and cold boot with restoration even when Autosave is OFF. Checkpoint/commit failures keep the menu paused.

The supervisor tries the new profile before consulting history. Recovery promotes the safe profile atomically, limits backups to five, and preserves unrelated files. Inputs remain raw SDL values: friendly names and SVGs are presentation only.

Consolidation evidence: `private/menu-core-candidate/RESULTADOS.md`.
