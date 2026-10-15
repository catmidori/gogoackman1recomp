# Game, Graphics and Audio menu settings

The tab order is File, Game, Graphics, Audio, Controls, Keybinds, Mods.
All tabs share one font fit, one five-percent horizontal content inset, the
same full-width single-border gray rows, and the same globally measured
carousel box, measured across all registered aspect, audio, scale and size values.
Q/E and LB/RB cycle all seven tabs; arrows/D-pad, Enter and mouse use the
existing shared widgets.

| Tab / item | INI key | Default / valid values | Live behavior |
|---|---|---|---|
| Game / Pause when inactive | General.PauseWhenInactive | 1; boolean | SDL window focus loss suspends the whole host simulation and audio device; focus gain releases this pause reason |
| Game / Autosave | General.Autosave | 0; boolean | Loads executable-relative saves/GGA1_autosave.sav on boot when enabled; a missing file silently starts normal boot. Saves/overwrites that dedicated file on exit when enabled. Manual slots are independent; enabling during play does not load a different state |
| Game / Run ahead | General.RunAhead | 0; 0..4 | Calls snes_runahead_set_frames; real core maximum is SNES_RUNAHEAD_MAX=4, with an additional menu ceiling of 10 if that bound ever grows |
| Graphics / Display Aspect | Graphics.DisplayAspect | 4:3 or 8:7 | Existing adaptive viewport and PPU border budget |
| Graphics / Full screen | Graphics.Fullscreen | 0; boolean | SDL_SetWindowFullscreen on the existing window, also shared by the configurable fullscreen hotkey |
| Graphics / VSync | Graphics.VSync | 1; boolean | SDL_SetRenderVSync on the live presenter; the runner retains its DisableFrameDelay/decoupled-presentation rules |
| Graphics / Output method | Graphics.OutputMethod | SDL, OpenGL, SDL-Software | Recreate the SDL3 presenter between completed UI frames; preserve the last game pixels and the visible HUD |
| Graphics / Remove sprite limit | Graphics.NoSpriteLimits | 0; boolean | Existing PreparePpuFrame updates kPpuRenderFlags_NoSpriteLimits on the next raster pass |
| Graphics / Widescreen | Graphics.Widescreen | 1; boolean | Adaptive Ackman extension; disabled mode pillarboxes the selected native aspect |
| Graphics / Windowed mode size | Graphics.WindowSize | 1344x672; 448x224 multiplied by 1..9 | Changes the window independently of internal scale; sizes selected in fullscreen apply when returning to windowed mode |
| Graphics / Internal resolution | Graphics.WindowScale | 1; 1..10 | Nearest-neighbour intermediate render target; guest PPU/raster buffers remain native |
| Audio / Enable audio | Sound.EnableAudio | 1; boolean | Safely closes/reopens the SDL audio stream between frames |
| Audio / Audio volume | Sound.Volume | 100; 0..100 in steps of 10 | Updates the existing mixer; retains the separate Mute shortcut |
| Audio / Sample rate | Sound.AudioFreq | 48000; 32000, 44100, 48000 | Reopens the stream and updates core resampling; labels are 32 kHz (original), 44,1 kHz, 48 kHz |

`Widescreen` is the canonical key. `IgnoreAspectRatio` remains a read-only legacy
alias when Widescreen is absent; Widescreen takes precedence in either ordering.
WindowScale now controls the intermediate image, not the OS window. The exact
requested scale labels (including 720p/1080p/1440p/4K) are menu names; native
224-line SNES output multiplied by 3 is 672 lines, not a new 720-line PPU mode.
Missing/unreadable/unsupported ROMs open the native selector in executable-relative
`rom/` before core initialization. A valid selected path persists in General.Rom.

The displayed Software value is written using the framework's canonical
`SDL-Software`; the parser also accepts `Software`. OpenGL uses SDL3's real
OpenGL render driver, so it shares the ImGui and blur path with SDL and Software.
A failed driver creation restores the prior output method and frozen frame.
Software drivers may reject VSync; the requested preference remains persisted
and the host reports the driver's result. Ordinary frame pacing remains active.

Focus suspension is independent of the manual Pause shortcut and HUD pause.
Regaining focus never releases either of those other pause reasons. The clock
is reset while suspended, preventing a simulation catch-up burst. Run-ahead's
real frame enters AckmanRunFrame so final-score fades/reboot still execute once
per real tick; speculative frames use the core directly and are rolled back.

Menu-owned values are committed on shutdown through AckmanConfigUpdate's atomic,
section-preserving writer. It does not add filter or frame-blending preferences.
When changing output, an existing legacy Renderer override is reset to auto so
it cannot undo OutputMethod on the next boot.

Implementation in the pinned runner is recorded in
`patches/ackman-menu-settings.patch`. CMake unwinds that known tail patch before
checking older overlapping menu hunks and reapplies it afterward. The entire
original 14-patch stack was also applied successfully to untouched pinned source
files. The current 17-patch stack additionally includes autosave, dialog pause
and `ackman-av-settings.patch`; it was checked against clean pinned sources.

Validation evidence: `private/game-menu-candidate/`. The 14 CTests exercise the
seven tabs, keyboard/mouse/virtual gamepad navigation, all new controls,
small-window/DPI font fitting, exact row/checkbox raster borders, globally
aligned carousel arrows and shutdown/reload persistence. Runtime scripts cover
all three native Windows drivers, unsupported-driver rollback with dummy SDL,
fullscreen, VSync, run-ahead snapshots, autosave, and focus/audio pause. Focus
loss and gain have the same emulated frame number over the measured pause.
The final-score save is also replayed with RunAhead=4 through fade and reboot.
These are automated SDL tests and captured UI images, not a manual controller
session. The standard 1000-frame smoke uses the root configuration in an
isolated executable directory.

Final candidate: 14/14 CTests passed; native Windows and final-score RunAhead=4 replays each completed 1000 frames. SDL VSync on/off requests succeeded on the native driver. UI captures are in private/game-menu-candidate/menu-overlay.png and menu-graphics-unchecked.png. Previous executable: private/pre-game-menu/AckmanRecomp.exe.
