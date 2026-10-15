# Menu and savestates

> Historical report of the original menu bar, before the in-game overlay migration. Current behavior is documented in OVERLAY-HUD.md and the README.

The bar uses Dear ImGui v1.91.9b with the official SDL3/SDL_Renderer3 backends. The system font (or Open Sans Regular) uses 16 x SDL_GetWindowDisplayScale pixels. The base style is restored before ScaleAllSizes, avoiding cumulative scaling. Reserved bar height follows font size and padding. Fullscreen returns the whole viewport to the game; Esc can display a floating bar. StyleColorsDark/Light follows SDL_GetSystemTheme. Dialogs remain native; the window supports high pixel density.

```
File
  Select ROM...
  Save
    Slot 1                 (Shift+F1)
    Slot 2 ... Slot 10
  Load
    Slot 1                 (F1)
    Slot 2 ... Slot 10
  Load save from file...
  Reboot
  Quit game
```

Select ROM uses SDL_ShowOpenFileDialog with the sfc;smc filter. With no history, the initial directory is the executable's folder; with rom.cfg, it is the previous ROM's directory. The compatible Go Go Ackman revision is checked before ending the session. A valid selection closes audio/logs/window and launches another session of the same executable with that ROM. Cancellation preserves the game.

The ten slots, F1/Shift+F1 shortcuts, test scripts, and savestate browser share the filename function: saves/GGA1_save_01.sav through saves/GGA1_save_10.sav, under the executable directory. Saving overwrites. Loading a missing slot neither calls the core nor displays an alert/error log. The shortcut uses Slot 1 independently of old host associations.

Load save from file opens in saves/, filters sav, and accepts custom names including accented characters. Old files are not renamed; use this option to open them. Save integrity checks remain in place.

Reboot shares the score-screen reset path: clear CPU, restart core/session, and realign the host clock. Quit game uses SDL_QUIT, allowing RAM logs to drain and audio/files to close normally.

Implementation: src/ackman_menu.c; event integration in ackman_debug.c; reset in ackman_flow.c; relaunch after cleanup in main.c. patches/ackman-menu.patch preserves framework integration; patches/ackman-sdl-menu.patch integrates viewport, rendering, and paused updates. Dialog callbacks only enqueue a copied UTF-8 path. All core/window actions occur on the main thread.

## ImGui interface

Alt only focuses File and underlines F. F or Down opens the dropdown. Esc toggles the bar, including fullscreen; key repeats do not toggle again. Arrows/Enter navigate. Alt+Enter toggles fullscreen. F1/Shift+F1 remain bound to Slot 1. Menu focus blocks game input. Selected commands enter the SDL queue and execute after drawing.

Persistence: src/ackman_menu.c. Interface: src/ackman_menu_imgui.cpp, with a C API for the core. ImGui textures/backends close before the SDL renderer.

## Dependency

external/imgui: v1.91.9b, commit f5befd2d29e66809cd1110a152e375a7f1981f06. CMake links four core files and SDL3/SDL_Renderer3 backends. License: external/imgui/LICENSE.txt. Its pixel font is not used. For a fresh copy:

```sh
git clone --depth 1 --branch v1.91.9b https://github.com/ocornut/imgui.git external/imgui
```

OutputMethod=SDL shares the graphics device between game and menu.

## Tests

11 CTests cover persistence, mouse, Alt/arrows, fullscreen, light/dark themes, DPI 100% → 125% → 150% → 100%, shortcuts, slots, and core behavior. DPI is simulated without changing user monitors. Smoke: 1000 frames with save/load. Regression: 51 saves, 51000 frames, core state, images, masks, and cadence. Private-byte measurements bound observed memory changes but do not replace a leak sanitizer.

## Fonts

Windows: NONCLIENTMETRICSW/SystemParametersInfoW identifies lfMenuFont; the registry resolves the installed file under Windows/Fonts. No system font is copied into the package. Linux uses Fontconfig (FcFontMatch for sans-serif) when its development library is available in CMake. macOS calls CTFontCreateUIFontForLanguage(kCTFontUIFontMenuItem), extracts kCTFontURLAttribute, and resolves the path through CoreFoundation. An unreadable URL falls back to Open Sans; there are no hardcoded Apple paths.

If discovery/reading fails, use assets/fonts/OpenSans-Regular.ttf, authorized by the user, with assets/fonts/OFL.txt (SIL OFL 1.1). CMake copies only these two assets beside the executable. Missing external files use an embedded copy of the same Open Sans rather than terminating the game or using the pixel font. CMake generates bytes from the original asset without embedding proprietary fonts.

ACKMAN_TEST_FONT_FALLBACK=1 tests Open Sans even when system fonts exist. Native testing occurred on Windows; Linux/macOS branches were reviewed but require native validation.

The Release build was validated at build-validation/AckmanRecomp.exe and copied to the root only after tests. ACKMAN_TEST_FONT_NO_FILES=1 tests the embedded font without reading external fonts. Fontconfig is optional; Linux immediately uses Open Sans without it. Alt only focuses/underlines; F/Down opens. Once open, Down navigates options instead of reopening File on each press. Esc toggles in both window/fullscreen without held-key repeats.

## Submenu mnemonics

Alt + F/Down immediately shows underlines. Markers are &Select ROM, S&ave, &Load, L&oad save from file, &Reboot, and &Quit game. Slots use 1–9 and 0 for Slot 10. Letters/digits activate only the deepest open level, without triggering parent menus. NavEnableKeyboard remains active. Presentation stays in ackman_menu_imgui.cpp; commands/persistence stay in ackman_menu.c.

CoreText/CoreFoundation are linked only on Apple builds. The API was reviewed against official documentation; runtime validation requires native macOS testing. Tests in this environment were on Windows.
