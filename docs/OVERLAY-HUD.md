# Graphic menu in-game — October 9, 2026

Esc opens/closes the Dear ImGui overlay with the SDLRenderer3 backend in windowed or fullscreen mode. The centered black panel occupies 90% of the logical viewport height, maintains 4:3, and uses alpha 0.90. The old bar no longer reserves game height. In very narrow windows, the required ratio is retained without reducing height to fit.

The tabs at this stage are File, Graphics, Audio, Controls, Keybinds, and Mods. File/Graphics have content; others are empty. Q/E and LB/RB switch tabs; mouse, arrows/Enter, and D-pad use native ImGui navigation. Opening selects File, collapses Save/Load, and resets focus/popups/scroll. Fullscreen accepts the resolved `[KeyMap]` shortcut even with the overlay open. Other navigation events remain with the interface; held input does not leak immediately to gameplay when closing.

The real loop stays in `external/snesrecomp/runner/src/desktop/host_main.c`, called by `src/main.c`. The patch `patches/ackman-overlay.patch`, applied by the CMake, interrupts the execution before the emulation/runahead routines, pauses the audio output and presents the frozen texture while processing events. Do not change the player's explicit pause. The clock is reset during the menu, avoiding accumulation of temporal debt. `AckmanRunFrame` It also has a break guard.

The capture occurs after the composition of the game and before the interface. The filter is a Gaussian separable in half resolution (sigma equivalent to 5 pixels output, support approximately 15–16 pixels), followed by linear interpolation. It is a visual approximation of the requested blur, not a proprietary implementation of Photoshop. The texture is reused while the menu is open and rebuilt at the next opening or changing output size. There's no frame readback.

The cross-platform `ackman_font.cpp` module discovers native fonts, with disk/embedded Open Sans as fallback. DPI changes rebuild the atlas and scale style; panel height also affects font scale. Dimensions are recalculated every presentation.

## Visual finish and autofit

Inactive flaps and components have transparent filling. The tabs have discrete white contours and a white line under the bar; hover and selection use the same translucent gray of the items. Focus contours are also white, no blue. The set of the six tabs is centered on the width of the panel. File buttons and selectors are centralized, including Save/Load And the slots, without the side indentation of the old trees. Each slot line centralizes the name and shortcut set.

Before starting frame ImGui, the menu measures the six strings in the loaded font. The calculation reserve padding, spacing, rounding margin and scrollbar. If necessary, reduce `FontGlobalScale` proportionally for all tabs and all widgets. The native atlas is reused during resizing and recreated in changes of DPI; there is no reading/rebuilding TTF Every frame. Fitting the bar does not shorten labels or display scroll buttons: the calculated scale accommodates the six full words.

Additional validation: sizes 320x240, 480x360, 640x480 and 960x720 combined with DPI 100/125/200%, checking the actual width of each tab against the full text, combined width and shared scale of widgets. Also past the 12. CTests, including sources in disk/embedded and hybrid navigation. File Capture in `private/menu-style-candidate/menu-file-style.bmp`; reports and smoke of 1,000 frames in the same directory. Previous binar in `private/pre-menu-style/`.

## Alt shortcuts exclusiveness

`ackman_menu.c` intercept Alt before sending events to ImGui or the host. The state includes `SDL_GetModState()`, event modifiers and individual Alt borders left/right. During the modifier, only system commands solved by `[KeyMap]` You can pass. Text, buttons mouse/gamepad and keys without system command are consumed; the queue and input state of the ImGui they are also clean, including after backend gamepad polling.

The chord keys are isolated until their own key-up, even if Alt is released before. Thus, Enter maintained after Alt+Return does not become Start or confirm an item. The key-up saves the original modifiers to correctly finish continuous actions like Turbo. Loss of focus clears the transient lock.

The physical consultation of Keybinds in the host is a second input path, distinct from events. The persistent patch `ackman-alt-input.patch` neutralizes the state of control and human input before the gestures of overlays and the execution of core, including runahead and Player 2. Reproduction scripts remain independent. `AckmanRunFrame` keeps the existing additional protection. `src/main.c` continues delegating the loop to the shared host; there is no second loop SDL In that file.

Validation: 12/12 CTests And a 1,000-frame smoke. Tests include Alt+Return with open menu without activation, remapping to Alt+F3, old chord lock, Alt released before Enter, Alt left/right, start neutralization and real centralization of tabs in all resolution/ combinationsDPI previous ones. The chords are events SDL for testing and `SDL_SetModState`; the fullscreen Windows test remains separate. The new capture and reports are in `private/alt-input-candidate/`. Clean application and reverse verification of the new patch passed; the CMake checked the installed patches. Previous executable is in `private/pre-alt-input/AckmanRecomp.exe`.

File Save Select ROM..., Save/Load (slots 1–10), Save to file..., Load save from file..., Reboot and Quit game. Shortcut labels come from the active map, without implicit keys. The commands are queued for execution by the main loop. Names `saves/GGA1_save_XX.sav`silent overwrite and silent missing slot load continue in the module `ackman_menu.c`. Selecting an action closes overlay.

`legacy_code` contains only copies prior to the transition from `ackman_menu.c`, `ackman_menu.h` and `ackman_menu_imgui.cpp`, with original names. They are historical references and do not enter the build.

Windows Release Validation: 12/12 CTests And smoke separated from 1,000 frames. Menu tests exercise keyboard events, mouse, virtual gamepad SDL, flap change/reopening, save per click on slot 10, geometry, blur contrast, DPI 100/125/200% resize, fullscreen Windows and fallback fonts. The execution guard is checked by 100 calls without advance of the core. The smoke is headless; the fullscreen test uses the Windows driver. Perceptive evaluation was not performed with a physical gamepad or complete regression of 51 saves.

Evidence: `private/overlay-candidate/LastTest.log`, `smoke.log`, `smoke.csv` and `validation.json`. The new patch has gone through reverse verification and isolated reapplication; reconfiguration CMake checked the installed patches. The previous binary is in `private/pre-overlay/AckmanRecomp.exe`.

## Dropdowns, tab line and Save to file...

In the transition from hidden to visible by Esc, the menu clears focus and popups and marks a reboot of the two sections. In the first File drawing, record Save and Load as closed in the storage of ImGui and returns the scroll to the top. Only changing tabs during the same opening preserves expansion. The two separators between File groups were removed. The line under the tabs uses a white strip of a logical pixel at the same height as before, covering the full width of the panel, including its padding, in the draw list of foreground.

Save to file... is immediately after Load and before Load save from file.... Open it. `SDL_ShowSaveFileDialog` with single filter `sav` and home directory `<executable directory>/saves/`, created when necessary. Cancellation or dialog error does not write anything. Callback duplicates the path and publishes an event; only processing this event in the main loop calls `RtlSaveSnapshot`. Names without extension receive `.sav`; other extensions are silently refused. The user can choose another destination in the dialogue. Existing numbered saves continue to use their original names. The exit of the dialogue follows the existing overlay closing policy when selecting an action.

Validation of the candidate: 12/12 CTests And a 1,000-frame smoke. Tests exercise expansion of both dropdowns per mouse followed by Esc/reopening, new button, filter/directory dialogue, cancel/error, exclusion of competing dialogs, callback without early writing, path UTF-8, overwritten, no extension and invalid extension. The line is checked by pixels at both ends. As APIs dialogue are replaced by wrappers in automated tests; the appearance/interaction of the native box was not manually evaluated in this round. Evidence in `private/save-file-candidate/`, including File capture, logs and `validation.json`. Previous Executable on `private/pre-save-file/AckmanRecomp.exe`.

## Initial focus and Graphics / Show FPS

The first active item receives `SetItemDefaultFocus()` after being drawn, along with explicit focus of keyboard for exchanges in a window already visible. The navigation cursor is made visible. Tab change is also detected when it comes from the mouse, in addition to commands Q/E and LB/RB. File highlights Select ROM...on opening; Graphics highlights your checkbox. The focus is not continuously reapplied while the user browses the same tab. An arrow down after the initial highlight of Select ROM..., it's usually on to Save.

Graphics uses a table of two columns without borders, filling or padding of horizontal cell. Its coordinates range from 5% to 95% of the panel width, calculated at each frame. The Show text FPS is at the left border of the first column; the checkbox is aligned to the right border of the second column. The geometry follows the viewport size and the scale of font/DPI already calculated by overlay.

The checkbox is an invisible button with enabled navigation, opaque white contour and transparent center when turned off, filled with opaque white when turned on. Your focus cursor also uses opaque white. Mouse, Enter and button A/South the gamepad activates the same option.

The state is the OSD real framework, by `snes_osd_fps_visible()` and `snes_osd_set_fps_visible()`. `[KeyMap] ToggleFPS` remains a shortcut string (default `f`), not a Boolean value in INI. This shortcut is now processed in the menu module, including while overlay is open, with repeat suppression and key-up. Shortcut and checkbox read/change the same display variable; there is no parallel state or persisted new preference. Startup of OSD continues to respect the existing host boot options.

Validation: 12/12 CTests And a 1,000-frame smoke. Menu Target Connects Module OSD Real, no mock of his state. Tests check initial focus on File and Graphics via keyboard/mouse/gamepad virtual, toggle by Enter/A/mouse, shortcut remapped to F9 unanswered from the old `f`, repeat/key-up, real margins of 5%/95%, absence of borders in the table and checkbox pixels checked/unchecked. The geometry was verified in 320x240, 480x360, 640x480 and 960x720, with DPI 100/125/200%. Reports and captures in `private/graphics-fps-candidate/`. Previous Executable on `private/pre-graphics-fps/AckmanRecomp.exe`.

## Focus filling, global and executable checkboxes GUI

The File buttons apply the gray fill to the item initially focused on the opening frame itself, in addition to the outline. The slots of Save/Load use the same fill when navigated by keyboard or control.

`ackman_menu_widgets.cpp` centralizes checkboxes: the entire line receives hover/focus gray, with text on the left and box on the right. The box has a single white outline of a logical pixel and white fill when marked. The navigation cursor involves the line, without drawing another outline on the square. Graphics maintains its two-column guides and 5%/95% margins.

As APIs public `ImGui::Checkbox` and `CheckboxFlags` use this component globally, including 64-bit flags and mixed values. O CMake renames only the original implementations in translation unit `imgui_widgets.cpp`, without altering the sold-out sources. New options using these APIs inherit the same drawing and interaction by mouse, keyboard and gamepad.

On Windows, `WIN32_EXECUTABLE TRUE` selects the subsystem GUI. `WinMain` forwards the arguments of CRT for `main` existing; file logs and smoke arguments remain available. The executable does not create a console.

Build generates `windows-icon/icon.ico` and `icon.rc` in the build directory from `icon.png`. Pillow preserves the proportion of art and its transparency in a square canvas, with sizes of 16 to 256 pixels. Resource compiler incorporates the nine sizes to the executable without requiring PNG/ICO outside in runtime. Python of tests may be different from Python with Pillow: configure `-DACKMAN_ICON_PYTHON=<caminho-do-python-com-Pillow>` when necessary.

Validation: 13/13 CTests And a 1,000-frame smoke. The tests confer by pixels the initial filling and the line of Graphics, simple contour, toggles of the APIs checkbox publics, mixed flags and hybrid navigation. The new test `windows_gui_icon` checks the subsystem PE and compares the built-in resources with each image of ICO generated. Evidence in `private/ux-icon-candidate/`; previous executable in `private/pre-ux-icon/AckmanRecomp.exe`.

## Global rule of single border and internal margin of items

File, slots Save/Load and checkboxes share the highlighting renderer in `ackman_menu_widgets.cpp`. The contour uses four tracks of a logical pixel entirely within the line. The expanded cursor of ImGui and the frame edge are not drawn in these components, avoiding double contours. Hover, initial focus and navigation use the same gray fill and the same border. New sub-items should use `AckmanMenuButton` or APIs Checkbox globals.

The internal horizontal margin is the width of a space of the current source, rounded up and with a minimum of two logical pixels. Texts and parameters are within this margin; long texts also respect your clipping. The box to the right is receded and its horizontal position is aligned to pixels, keeping all four white walls within the line and outside the table boundary. The external margins of Graphics continue at 5%/95% of the panel.

Validation: 13/13 CTests And a 1,000-frame smoke. Tests check the internal single edge of File, breathe to left/right and checkbox white right wall in all resolutions/DPI of the previous matrix. Captures were also inspected visually. Evidence in `private/menu-spacing-candidate/`; previous executable in `private/pre-menu-spacing/AckmanRecomp.exe`.

## Uniform width of highlights and right contour

All sub-items use the limits of the useful File content area. Graphics capture `GetContentRegionAvail().x` before opening the table of two columns and passes this entire width to the interactive line. This replaces the anterior external margin of 5%/95%; the internal margin based on the source space is preserved. The highlight covers text and checkbox from end to end, with the same horizontal format as the buttons and File slots. The cells remain only alignment guides.

The common renderer roundes the right limit into the useful area in an entire pixel, both for buttons and checkboxes. The simple border is entirely inside the rectangle, avoiding cutting into fractional coordinates of DPI. The checkbox line uses a clip that crosses both cells, keeping visible its right contour and highlighting contour.

Validation: 13/13 CTests And a 1,000-frame smoke. Tests compare File and Graphics limits to the same content area in resolutions/DPI from the matrix, they check for pixels both the lateral contours of the enhancement and the right wall of the checkbox. Capture of Graphics visually inspected. Evidence in `private/menu-uniform-candidate/`; previous executable in `private/pre-menu-uniform/AckmanRecomp.exe`.

## Global vertical spacing of checkboxes

The global component keeps the interactive height of the line on `GetFrameHeight()` and only reduces the square. Reserve above and below a margin derived from `FramePadding.y`, rounded up, with a minimum of two logical pixels. The side is rounded to integer pixels and the vertical position centered on the line (maximum difference of half pixel due to alignment with rasterization). Simple contour, white filling and horizontal margin remain the same. That rule goes for `AckmanMenuCheckbox`, `ImGui::Checkbox` and all types of `CheckboxFlags`, no additional configuration per tab.

Validation: 13/13 CTests, inspected capture and smoke of 1,000 frames. Pixels above and below the box show the gray background, and the four walls remain white. The matrix of resolutions/DPI and the tests APIs public check centralization and vertical spacinging. Evidence in `private/checkbox-height-candidate/`; previous executable in `private/pre-checkbox-height/AckmanRecomp.exe`.

## Cursor for inactivity in the game and overlay

`main.c` restarts the cursor policy when starting. The monitor SDL3 Shared on `ackman_cursor.c` use `SDL_GetTicks()` (64 bits), 2,000 ms. `AckmanPollEvent`, the native loop wrapper, checks the deadline even when there are no events or the core is paused. Move and any mouse button (down/up) call `SDL_ShowCursor()` and restart the deadline before the menu consummates the event. Keyboard, gamepad and scroll do not prolong visibility. The timer starts in the first pass after video startup and is restarted to a new window.

The backend ImGui use `ImGuiConfigFlags_NoMouseCursorChange`: he can't call anymore `SDL_ShowCursor()` Every frame and undo the concealment. Navigation by keyboard/gamepad continues to function while the system cursor is hidden.

Validation: 14/14 CTests And a 1,000-frame smoke. The deterministic test confirms the borders of 1.999/2.000 ms, restart of the deadline in each interaction, absence of redundant calls to hide and events that do not count as mouse activity. Integration tests await two real seconds with menu closed/open, consult `SDL_CursorVisible()` and verify that multiple frames ImGui they don't show the cursor again. Move, right button down/up and immediate return are also exercised with the native Windows driver. The gameplay smoke is headless; there was no manual physical mouse evaluation. Evidence in `private/cursor-idle-candidate/`; previous executable in `private/pre-cursor-idle/AckmanRecomp.exe`.

## Functional Aspect display and horizontal selector

Graphics contains Show FPS and Display Aspect. Shared selector draws `< 4:3 CRT >` or `< 8:7 Square Pixels >`, aligned to the right, with label to the left. The entire line maintains the useful width, single border and padding of a space. Arrows left/right the keyboard and D-Pad are reserved to the selector when focused, alternating cyclically without shifting the focus. Mouse activates characters `<`/`>`; click on the line only allows you to focus on it. The tab continues with the invisible table of two columns. LinearFiltering and FrameBlend were not added to HUD or config.ini in this implementation.

`AckmanVideoComputeViewport` use `g_config.display_aspect` instead of CRT fixed. `AckmanVideoFrameSize` refer to the same pixel aspect before calculating the amount of additional columns. CRT use PAR 7:6; Square Pixels uses PAR 1:1. In original mode, both have the 256x224 field centralized with bars; in adaptive mode, the expansion passes from the new central width. The safety limits and masks continue to apply: maximum 64 columns per side on the Ackman. In very wide windows, the limit can leave sidebars, especially at 8:7. The map is not increased beyond the validated limits.

The preference is in `[Graphics] DisplayAspect = 4:3` or `8:7`, with 4:3 standard also in the generated configuration. The parser of the framework already recognized these values. `AckmanMenuSetDisplayAspect` changes the presentation status, separated from the emulated snapshot, and marks the preference as modified. By closing the host, `AckmanMenuShutdown` update only this key in INI selected, preserving comments and bindings and avoiding inserting extra options via a global write. Close overlay with Esc keeps the choice in memory; viewport uses the new aspect in the game presentation, without reboot. The blurry background of HUD remains the frame captured in the opening, because the core is frozen during the menu.

Validation: 14/14 CTests and smoke of 1,000 frames in each aspect. Tests include 500 geometry alternations, different adaptive budgets for CRT/Square Pixels, original mode, keyboard left/right, click on the two arrows, virtual D-Pad, preserved focus, absence of truncation in the resolution matrix/DPI, border pixels and persistence followed by rereading of the INI. Evidence and catch in `private/display-aspect-candidate/`; previous executable in `private/pre-display-aspect/AckmanRecomp.exe`.

## Global horizontal selector box

All selectors use `AckmanMenuSelectorWidth()`: the largest text width among all recorded options, plus the same spaces and arrow glyphs. The catalog is registered before drawing any tab, including hidden tabs; currently `8:7 Square Pixels` determines the internal width. New selectors must record all options with `AckmanMenuRegisterSelectorOptions` on menu startup. Strings are copied; the catalog is restarted along with the context. The measurement uses the current source and scale, following changes in window/DPI.

The arrows are drawn separately at the fixed ends of the box aligned to the right. The current value is centered in the internal range, without changing the arrow positions, the click area or the simple border highlight of the line. The persistence of DisplayAspect continues using the same path as config.ini.

Validation: 14/14 CTests sequentially executed. Additional test renders Aspect and Mode selectors (On/Off), inspects the actual coordinates of the glyphs in the draw list and confirms the same positions before/after to exchange values. Captures and smoke of 1,000 frames in `private/global-selector-candidate/`.

## Square Coverage Correction Pixels

The previous limitation of 64 columns per side was reviewed for 112 safe pixels. The width and rounded up, according to the PAR active, and the viewport covers the window without stretching the image. In the standard window, Square Pixels goes from 384 to 448 internal pixels, removing the bars imposed by the old ceiling. Scene policies and map limits continue to mask margins without valid scenery. The rule for ultra-large windows uses vertical cutting on the safe limit of 480. View WIDESCREEN-SCENES.md for the justification of tilemap and evidence candidates.
# Permanent scrollbar channel

The main Ackman HUD uses `ImGuiWindowFlags_AlwaysVerticalScrollbar`, so its usable width remains constant across every tab. The reserved right channel is empty when content fits. Overflow uses Dear ImGui's scrollbar indicator and interaction in the same channel; Hotkeys forwards this to its scrolling child to track the stationary tab bar. Tab titles are centered in the usable area. Regression checks compare content edges across remappinged Q/E and LB/RB cycles, and inspect rendered pixels for a visible Hotkeys scrollbar thumb and an empty Game channel.

