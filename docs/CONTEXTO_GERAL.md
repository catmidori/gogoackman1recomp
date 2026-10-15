# Go Go Ackman — consolidated technical context

> **Recursive catalog and multi-selection in Mods:** lists filter `.snesmod` and `.msu`, persist relative paths, and read XML/BML metadata beside the MSU file. Ctrl/Shift select ranges/items and Enter/A transfers batches; mouse hover does not change selection. Lists have white frames and use the space freed by halving the metadata panel. See [MODS-MENU.md](MODS-MENU.md).

> **Mods Panel Correction:** the metadata were present in the manifest; the interface still used mock values. The panel now reads Name, Author, Version and Readme complete, with original breaks and scrolling, directly from ZIP generated. Previews are also updated after conversion.

> **Native consolidation of 11 October 2026:** the conversion of official translation uses table and manifest constants in C, ZIP/CRC Natives and atomic publication, no Python in runtime. The package is identical to the one previously validated. Reboot preserves fullscreen in creating the new window and grants focus permission before starting the child, which calls SDL_RaiseWindow. See [NATIVE-TRANSLATION.md](NATIVE-TRANSLATION.md).

> **Update of 11 October 2026 — Persistent Mods:** `[ActiveMods]` records file names per line. Boot removes missing records, validates native packages and activates English translation before the first frame. File → Reboot starts a new process with the same ROM and configuration. The 14 CTests, 1,000 frames smoke and the real Cold Boot passed. The introduction image was checked in English. See [ACTIVE-MODS.md](ACTIVE-MODS.md).

> **Update of October 11, 2026:** Hotkeys was populated with 33 rows in three columns, hybrid capture and conflicts by exact + modifiers key. `[KeyMap]` use SaveSlot1–10 and LoadSlot1–10; SaveStateMenu has been removed. Rewind migrated from F8 to 8. Configuration, focus and scrolling were validated at 14 CTests, with a smoke of 1000 frames. See [HOTKEYS.md](HOTKEYS.md).

> **Update of October 10, 2026:** Controls offers transactional wizards and individual keyboard/gamepad remapping, Delete clearing, three-column conflict warnings, and unified mouse/keyboard/gamepad focus. Hidden mouse position does not interfere; the first click only reveals it. Keybinds was renamed Hotkeys. Gameplay uses `[Controller.Keybinds.Player1]` and `[Keyboard.Keybinds.Player1]`; Player2 remains internally compatible. See [CONTROLS-REMAPPING.md](CONTROLS-REMAPPING.md).


> **Update of October 9, 2026:** the top bar was replaced by an in-game menu opened by Esc, with pause, blur and hybrid navigation. The previous implementation is isolated in `legacy_code`. See [OVERLAY-HUD.md](OVERLAY-HUD.md) for the current state and evidence of tests.

> **Update of October 7, 2026:** the configuration is now unified in `config.ini`; `rom.cfg`, `keybinds.ini` and `config.local.ini` They're not read. Shortcuts come exclusively from `[KeyMap]`, including labels ImGui; gameplay comes from `[Keybinds.Player1/Player2]`. See [CONFIGURACAO.md](CONFIGURACAO.md) for architecture, behavior and current validation (12 CTests and 1,000 frame smoke). System commands have been explicitly restored in INI, including fullscreen, FPS, turbo, mute and scale; `Controls` was removed by redundancy. The key `u` now alternates widescreen/4:3; `IgnoreAspectRatio` selects the initial mode (0/1), and the comments of INI They're in English. The old references to single files, implicit shortcuts and hash from the executable below are historical.

Knowledge transfer document, consolidated on **5 October 2026**. Local project: `R:\ChatGPT\snes_decomp`. Recipient: a new AI instance or developer with access to the current tree.

This document brings together the history of decisions, the current implementation and the limits of the available evidence. It's not a re-implementation proposal. The following work should start from the existing code, preserving the already validated gameplay, synchronization and persistence corrections.

## 1. Project status and how to interpret the history

The Windows executable integrates the SNESRecomp core, SDL3 presentation/audio, Go Go Ackman-specific horizontal expansion, experimental PCM music replacement, and Dear ImGui menus. The user reported a stable complete playthrough, including savestates during transitions. Later work refined the interface, fonts, DPI, and keyboard navigation.

The consolidated main binary is `AckmanRecomp.exe`, generated in Release from `build-validation`. SHA-256 granted in this consolidation:

```text
3a860420a647618c4c2a992749794d3f736f97dec234779ba8191635db51ee93
```

The last registered round approved **11/11 tests CTest**, smoke test **1,000 frames** and regression of **51 saves × 1,000 frames = 51,000 frames**. The final results of the regression are in `private/scene-imgui-consolidated/results.json`; the maximum variation recorded in the field `tail_growth` It was zero bytes. This is evidence of stability in the observed window, **not evidence of absence of any leakage**.

The current request is documentary; this consolidation does not run this battery again or modify the engine. The mnemonics pending described at the end remains open by user report, even with the automated tests approved.

### Order of trust for a resumption

1. Current source code, effective configuration and reproducible results.
2. This document and the specialized documents linked to the end.
3. Chronological entries of `docs/RETOMADA.md` and reports of the conversation, interpreted on the date they occurred.

History contains many messages repeated by a user script; they do not represent independent tasks. Old amounts such as “13 files”, “12 files” and “5 candidate files” describe previous moments, not a current inventory. The project root is not a Git checkout; the framework in `external/snesrecomp` It has its own history. Do not assume that there is a recoverable global diff at the root.

### A summary evolution

- Mapping of NMI, row VRAM, camera, scroll and protocol SPC.
- Separation between music and effects; streaming PCM with handshake preservation and own envelopes.
- Stabilization of frame clock and structural correction of saved execution states.
- Video expansion initially restricted, then specific policies by scene and map limits.
- Raster replay fixes, parallax BG2, dialogue windows and animated floor of the car scene.
- Diagnosis of locks in HDD: frequent synchronous logging; queue replacement in RAM and written in thread.
- Regression of 51 saves and Start flow in final score with fade and reboot.
- Initial custom interface, Win32 menu attempt and abandonment of this approach due to limitations of theming/portability.
- Consolidation in Dear ImGui with SDLRenderer3, DPI, System fonts, Embedded Open Sans, Alt and Esc.

## 2. Identity of ROM and port architecture

### ROM Reference number

|Field|Value|
|---|---|
|Game|Go Go Ackman (Japan)|
|Identifier|`go-go-ackman-jp`|
|Mapping|LoROM|
|Standard size|1,310.720 bytes, no 512 byte copy header|
|CRC32|`26fb2d11`|
|SHA-256.|`e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f`|

A ROM is provided by the user, not distributed by the project. The path used in local validations was `R:\ChatGPT\Go Go Ackman (Japan).sfc`. The addresses with the bank below are addresses of the bus CPU, not direct file offsets. Addresses RAM Do SPC They belong to another memory space.

### Layers and responsibilities

```text
src/main.c — contrato SnesDesktopHostGame e ciclo de vida
    └─ desktop host do SNESRecomp
       ├─ SDL 3: window, events, renderer, audio, and file dialogs
       ├─ Dear ImGui: bar, dropdowns, navigation, and fonts
       └─ runtime do jogo / kGameInfo
          ├─ generated C + interpreted-execution bridge
          ├─ CPU, MMIO, DMA/HDMA, PPU, APU/SPC do framework
          └─ Ackman modules: hooks, timing, video, audio, states, and flow
```

The project did not rewrite the integer logic of the game as an independent engine. The implementation of the ROM and the states of hardware continue to be the responsibility of runtime SNESRecomp, with generated code and interpreted paths. The Ackman modules specialize in this runtime.

|File/module|Main responsibility|
|---|---|
|`src/main.c`|Host settings, game callsbacks and shutdown/reopening|
|`src/ackman_hooks.c`|Interception of accesses, observation of execution and instrumentation|
|`src/ackman_timing.c`|`kGameInfo`, frame execution, deadlines, execution states and reset|
|`src/ackman_video.c`|Policies per scene, lateral projection, limits and restoration of VRAM|
|`src/ackman_audio.c`|Command classification SPC, selection PCM and synchronised exchange|
|Module PCM and `AckmanMixPcm`|Saturated mix and envelope per sample|
|`src/ackman_flow.c`|Fade and restart on the final screen|
|`src/ackman_debug.c`|Debugging and integration with persistence/control|
|`src/ackman_log.c`|Queue of logs in RAM and disk worker|
|`src/ackman_menu.c`|Commands, slots, dialogs, persistence of ROM and relaunch|
|`src/ackman_menu_imgui.cpp`|Rendering ImGui, navigation, mnemonics, theme and DPI|
|`src/ackman_font.cpp`|Discovery of sources by SO and incorporated fallback|
|`src/gen`|Code generated; it is not the only place where hooks should be kept|

O CMake redirects specific framework calls: `SDL_PollEvent` for `AckmanPollEvent`, `SDL_SetRenderVSync` for `AckmanSetRenderVSync`, `SDL_Quit` for `AckmanDebugQuit` and `RtlRunFrame` for `AckmanRunFrame`. In the common runtime, redirects writing of registers and mixing MSU for the Ackman wrappers. Relevant writings continue to reach the core; intercepting does not mean eliminating the cost or protocol of the guest.

There are historical mod catalog flags in `main.c`, including `widescreen_supported=0` and `msu1_supported=0`, in addition to old validation comment. They do not summarize the current capabilities of native modules. Do not remove features by interpreting these flags alone.

### Persistent dependencies and patches

- Framework set in `e119994bcde03dec95f9ebaeeda305d72a6668b2`.
- Dear ImGui `v1.91.9b`, commit `f5befd2d29e66809cd1110a152e375a7f1981f06`, `external/imgui`.
- Used Backends: SDL 3 and **SDLRenderer3**. Do not assume that the UI current use a backend OpenGL separate.
- C11/C++17; built Windows validated with MinGW and static runtime. Previous configuration registers SDL 3.4.10.
- Framework Patches: `clocked-beam`, `timing-crossing`, `ackman-raster`, `script-savestate`, `ackman-state`, `ackman-reset`, `ackman-menu` and `ackman-sdl-menu`.

O CMake verifies if each patch is already applied and applies the missing ones. Directly changing the framework without updating the persistent patch may lose the fix or break the reverse scan. Avoid incompatible overlapping patches. `scripts/inject_hooks.py` maintains the instrumentation of the paths generated/interpreted; editing only one output generated does not guarantee persistence.

## 3. Reverse engineering: essential map

### CPU, camera, video and selectors

|Address|Meaning established|
|---|---|
|`$80:81C0`|Routine NMI|
|`$80:825D`|Queue processing VRAM|
|`$80:840D`|Scroll update table BG1/BG2/BG3|
|`$80:AA3F`|Camera/scroll calculation|
|`$80:AAB9`, `$80:AB0D`|Upgrading of scenario columns|
|`$80:ABD0`, `$80:AC41`|Map transfer in WRAM for the queue of VRAM|
|`$80:A8E0`, table `$80:B028`|Camera region selection|
|`$80:A953`, `$80:A973`|Treatment of right and left limits|
|WRAM `$0272`|Shadow of INIDISP|
|WRAM `$0274`|Video mode shadow|
|WRAM `$0276..$0278`|Tilemaps Settings BG|
|WRAM `$027C/$027E`|Scroll horizontal/vertical BG1|
|WRAM `$0280/$0282`|Scroll horizontal/vertical BG2|
|WRAM `$0284/$0286`|Scroll horizontal/vertical BG3|
|WRAM `$0400/$0401`|Selectors of state/scene; playing along with the script|
|WRAM `$0402/$0406`|Camera X/Y|
|WRAM `$040C`|Map Stride; high byte `$040D` also participates in the region selection|
|WRAM `$0416/$041A`|Origins used by projection in circular tilemap|
|WRAM `$1124/$1126`|Scene script position and pointer|
|WRAM `$7E:4000..$7E:DFFF`|Map data used by expansion|
|Script `$9447`|Opening sequence, including an initial exception to the logo|
|Script `$BB63`|Title/menus|

Selectors are often registered as little-endian word initiated in `$0400`: bytes `00 02` they appear as `$0200`. Do not confuse this value with “phase 2” universal. The policy depends on the combination of selector, script, position and layout.

The hardware vectors used by the scheduler include reset `$FFFC`, NMI native `$FFEA`/emulation `$FFFA` and IRQ `$FFEE`/`$FFFE`. The vector address and the routine indicated are distinct concepts; mirrored banks also explain zero bank appearances.

### Audio from CPU for SPC

|Address/command|Function|
|---|---|
|`$80:8773`|API public music; positive values below `$38` use API `$13`; negatives follow API `$03`, masked with `$3F`|
|`$80:879A`|Long call for `$A2:8000`|
|`$80:87C9`|Path of effects, A transferred to Y, API `$14`|
|`$A2:84E3`|Primary transmitter SPC|
|`$A2:84FF`|Other transmitter, associated with the SFX path|
|`$A2:84B5`|Upload; should not be confused with track selection|
|`$A2:84F8`|Interception point prior to writing, with DP `$0300`|
|`$2140..$2143`|Doors CPU/APU; bit 7 of `$2140` participates in the handshake|
|ROM `$84:8BA7`|Table of music indexed by selector `$0401`|

The classification also uses the caller return in the stack, considering the PHP: bytes in S+2/S+3. Returns `$80A0/$80F2` identify musical selection; `$810E/$811D/$812C/$813B/$8159` correspond to `$3B/$3A/$39/$38/$3F`. The same byte on the transmitter is not enough to distinguish music, effect and upload.

### Memory space of the SPC

- `$0880`: order; `$0898`: table of special commands.
- `$3B → $0911`: Set volume delta **−9**; `$3A → $0915`: delta **+2**.
- `$0A36`: applies the signed delta of `$66` ed `$67`, saturating in 0..255 when the timer counter `$FD` indicates activity.
- `$0B73`: musical path uses **SPC RAM `$67`**; `$0F0E`: effects use **SPC RAM `$69`**.
- Timer 0 with target `$60` a 8 kHz takes a nominal step each **12 ms**.
- `$35`, `$38` and `$39` are inert returns on the mapped points; `$36` treats control master/echo; `$37` returns to IPL; `$3C/$3D` alter flags; `$3E` treats upload; `$3F` reset.

`$67/$69` are not global volume registers of CPU 65C816. Confused address spaces would break the isolation between music and effects.

## 4. Audio MSU-1 and preservation of protocol

### Activation and files

Routing remains experimental and explicit: `ACKMAN_MSU_EXPERIMENTAL=1` and `ACKMAN_MSU_PREFIX` set the file family `<prefixo>-<ID decimal>.pcm`. The IDs accepted musicals are `$00..$34`, that is, 0..52 in decimal. The Ackman prefix does not depend on the mod manager to preserve the generic variable MSU the framework; conflicts of prefixes should be treated as such.

Each PCM is validated: header `MSU1`, 32-bit little-endian loop point, 16-bit stereo payload with multiple size of four and loop within the amount of sample frames. File missing/invalid preserves the path SPC and interrupts PCM substitute, instead of improperly silencing the original music.

Selection uses registers MSU `$2004/$2005`, check the status of `$2000` and controls volume/playback `$2006/$2007`. The mixer sums up PCM audio SPC with saturation.

### Handover without deadlock

For a replaceable song, the wrapper prepares the PCM paused and swaps the original musical command for fade SPC `$3B`. Preserve the handshake bit 7 and update both the written command and the low byte of A, which will be used by the next writing in the driver structure. Simply suppress the writing of `$2140` It causes endless waiting risk.

`AckmanObserveFrame`, under `RtlApuLock`, check the expected driver, queue/acknowledgment and the music volume `$67` reaching zero with delta `$F7`. `AckmanAudioPoll` then releases the playback PCM One time. Effects and volume `$69` remain in the SPC.

Bit 6 means playback on the driver; ID is masked with `$3F`. Do not treat protocol bits as part of track number.

### Native envelope PCM

The audio callback owns the envelope gain; the game thread publishes commands by an atomic mailbox. The gain changes per output sample, with slopes equivalent to −9 or +2 units of volume per 12 ms. The full volume fade up to zero is approximately **340 ms**. The processing uses small blocks, up to 256 sample frames, and saturated mixture.

Upon receiving `$3A` with PCM active, the command sent to SPC is replaced by no-op `$39`, avoiding reintroducing native music during fade-in PCM. Inert commands preserve playback; special commands not yet emulated in the replacement close the PCM and follow the SPC.

This equivalence was derived from the observed driver. Do not affirm absolute synchronicity with each tick of DSP: the exchange is observed by frame and the application of the envelope occurs in the next audio block. Future changes in timer programming would require further analysis.

### Threads, states and limits

O SDL_AudioStream already has callback separated from rendering, with synchronisation of the APU. The chain goes through buffer filling, `RtlRenderAudio` and `AckmanMixPcm`. A new universal solution of elastic resampling has not been demonstrated; a decisive part of the brake was I/O debugging.

Load/reset cancel pending exchanges, stop PCM and restart the envelope. **The playback position and cursor of the file PCM are not restored by the current savestatee.** The execution status of the core and its audio queues are another layer, described below.

F5/F6 for manual advance/fade are only enabled with `ACKMAN_DEBUG_KEYS=1`, in the experimental context MSU. They're not mandatory production controls.

## 5. Video, widescreen and raster

### Principle of expansion

The expansion is presentation: it does not globally modify the guest camera, collisions or enemy logic. The base frame is 256×224, with pixel ratio 7/6 for presentation 4:3. The adaptive width considers the drawable SDL, is rounded to par width and has budget of up to **64 additional pixels per side**, resulting in up to 384×224 in the current path.

`ACKMAN_WIDE_EXPERIMENTAL` is active save value `0`; `ACKMAN_WIDE_FORCE` It's also activated by default. Forced mode relaxes the strict comparison of central tiles, but retains layout and memory guards. It does not mean accepting any combination of PPU No boundaries.

### Layout, strides and projection

Mapped layout scans shadows of mode/map, including masked mode `$39` and BG1SC `$50`, minimum camera and origins aligned to 16. The **strides observed between `$0800` and `$2C00`** cover different phases; the validator accepts a larger structural range, `$0600..$6000` in steps of `$0200`. Do not replace this rule with an incomplete list of historical strides.

Each 256×256 pixel block contains 16×16 metatile words of 16 pixels, occupying 512 bytes. For metatile coordinates `tx, ty`, the projection uses:

```text
address = $4000
         + (ty / 16) * stride
         + (tx / 16) * 512
         + (ty % 16) * 32
         + (tx % 16) * 2
```

Divisions are integer. The address must remain below `$E000`, and the column within the width derived from the stride. Global coordinates and positions in the ring buffer are calculated separately from the camera and the origins `$0416/$041A`.

The effective vertical coordinate is `$0406 − 1`; apply the same correction to the two projections eliminated a line misalignment. The wrapper injects only extra side columns, saves the words of VRAM changed and restores them at the end. The Clamp of BG3 preserves HUD Native.

### Edge mask

The valid margins are calculated once at the beginning of the presentation, from the map allocation, the last visible line and the camera limits of the ROM. The left margin is limited by the distance from the camera to the beginning; the right, at the exclusive end less camera and native width of 256 pixels.

Not enough to test `scroll <= 0`: there are different origins and boundaries per region. The inclusive right border of the original format is converted to exclusive. The final mask zeros pixels RGBA outside the valid region, covering BG, sprites and effects that could reveal garbage tiles.

This calculation should not re-search for map validity per pixel or perform wide readings on each line. The current version uses arithmetic boundaries and line fill operations. Repetition funds have their own policies not to receive undue bars.

### Scene policies that should not be simplified

|Selector / script|Scene|Policy|
|---|---|---|
|`$0000 / $9447`, position up `$0037`|Banpresto logo|Repetition background / widescreen|
|`$0000 / $9447`, later sequence|Opening|4:3 fixed|
|`$0000 / $BB63`|Title and menus|Repeat / widescreen|
|`$0000 / $A483`, stride zero|Dialogue before the first phase|4:3|
|`$0200 / $A483`|Stage 1, phase 2|Map with boundaries; black out of scenario|
|`$0A00 / $AA11`, stride `$2000`|Stage 2, phase 3, up|Map and adaptive mask|
|`$0B00 / $AA11`, stride `$1800`|Stage 2, phase 3, low|Map and adaptive mask|
|`$1000 / $AA11`, stride `$1000`|Stage two, phase five, low.|Map and adaptive mask|
|`$1100 / $AA11`|Boss of Stage 2|**4:3 mandatory**|
|`$1600 / $A8A6`, stride `$0800`|Stage 3, phase 5|Map and adaptive mask|
|`$1900 / $A718`|Stage 4 dialogue|Map with black edges|
|`$2400 / $A718`, stride `$0800`|Boss of Stage 4|Map and adaptive mask|
|`$2500 / $A56E`|History with car/floor excited|Special projection by scanline|
|`$2600 / $A56E`, stride `$1200`|Stage 5, phase 1|Map and adaptive mask|
|`$2A00 / $A22E`, stride `$2C00`|Stage 5, phase 3|Map and adaptive mask|
|`$2B00 / $A22E`, stride `$1C00`|Stage 5, phase 4|Map and adaptive mask|
|`$2D00 / $98B4`|Final score|Widescreen and special flow of Start|

History Scripts `$ABD9/$A964/$A7C4/$A62D/$A1D9` They have a policy of repetition. There is fallback for layouts without stride in modes 0/1. A generic rule “`$0401 == 2` meaning history” would already be incorrect: it would reach the stage `$0200` And then I'd go back to exposing garbage on the edges.

### Raster Replay: BG2 and dialogs

The original journal did not adequately preserve the necessary states beyond the BG1. Correction records the four pairs of scrolls and the shared latches `scrollPrev/scrollPrev2`, in addition to window recorders, screen selection and composition.

They are relevant `$210D..$2114`, `$2123..$2130`, `$2100`, `$2131` and `$420C`. Writing on the zero line or from line 225 update the base instead of creating an undue visual division.

In the rendering of lines 0.224: apply raster state, update/initialize channels HDMA required, execute HDMA only after the zero line, design the scanline extension, render the line and restore the temporary extension. IRQ should not be re-executed by the renderer. The Way HDMA the beam is disabled in this strategy to avoid duplicate execution.

That fixes the parallax. BG2 divided horizontally and the spurious clipping of the dialogs. The initial hypotheses of Mode 5/6 or a simple lack of VSync were not the final explanation of these failures.

In the car scene, the consultation extension VRAM **later** Do raster/HDMA and repeats the pattern of 16 columns of the current line, changing at most eight tiles per scanline. Restoring before the next line prevents contaminating the guest status. This scene was compared to the center of a reference 4:3; the others also use the previous baseline widescreen.

Do not extrapolate 51 fixtures to all possible states of spawn/culling. Visual expansion is not equivalent to a universal rewriting of logic that decides when objects of the game exist.

## 6. Synchronism, frame boundary and performance

The adopted model uses **262 lines × 1,364 master clocks = 357,368 clocks per frame**, sampling/boundary anchored on the line **225**. They are model master clocks, not 357,368 homogeneous instructions or cycles of CPU.

`GameRunOneFrame` syncs the clock, resolves reset when there is no PC to continue, calculates the next absolute border of beam and divides the execution into slices limited by scanline/IRQ. Treat. NMI pending and returns from the interruption by the appropriate path. There is watchdog and hardware-controlled advancement in WAI/quiescence.

The patch `clocked-beam` removes the artificial advance of 64 clocks associated with certain queries from `$4212` when the actual beam clock is active. The refresh state and the difference between hardware event and instruction end are followed by timing path.

### What has been mitigated — and what is not cycle-exact

The deadline is absolute: finishing an instruction after the border does not cumulatively shift the next cadence. No “correct” by subtracting clocks already executed by CPU. The scheduler respects the legal border of instruction and maintains the Beam's accounting.

Instruction/refresh granularity delays remain, typically 0..68 clocks in the analyzed samples, with previous intervals reaching 72. An identified case started two clocks before the border and ended after 30 instruction clocks plus 40 refresh, resulting in 68 delay. That explains the difference without making it a host freeze.

So, **do not fully declare emulation cycle-exact**. The refresh is still modeled in granularity of instruction; the fixed cadence does not represent all possible modes of hardware, such as variations of field/interlace.

Force Blank should not stop the clock waiting for the PPU I'm gonna go back to drawing. The core continues to advance under the same deadlines. Locks of old states in transition also had a structural cause of serialization, described below.

### Logging in HDD and VSync

User isolated build debug micro-stutterings in HDD mechanical: common synchronous writings in multiples CSV/logs. `ackman_log.c` started to accumulate data in RAM and drain them in separate worker, usually every **180 frames** and at the closing.

The buffers start large, grow under protection, are highlighted by the worker and recorded outside the critical region. The `fflush` of the registered streams stop forcing I/O in frame thread. Shutdown waits for drainage before closing SDL. Integration also covers stdout/stderr and redirected audio statistics.

Do not remove logs to hide the problem or reintroduce flush by frame. Abrupt closing may lose the lot still in RAM. This mechanism is also not guaranteed zero cost under lack of memory or disk with error.

The wrapper asks VSync and records the result. Headless tests do not prove absence of physical tearing. The diagnosis of I/O confirmed should be distinguished from the initial suspicions of mask or resampling cost.

## 7. Savestatees, reset and final score

### Structural correction

Ancient Saves did not carry the entire volatile state of CPU in execution. Restore memory/ onlyPPU/APU and continue in a PC how `$A2:81FD` I could take back a handshake with incompatible registers and go on endless hold.

The current format combines the core state with continuation data: PC, vblank flag pending, `RtlSaveExecutionState` and marker `ACP1`. Execution status includes registers CPU, queues of APU ports, clock anchors and state of interpretation/refresh. The evolution of the runtime state includes residue of `RtlApuFrameClock` in version 7; this is different from the minimum version 5 required by the status format of the game.

The loader checks the trailer before applying an incompatible file. Keeps rollback snapshot for failure during loading, restores under lock of APU and clean deadlines/flags Transient wrapper. The strategy of HDMA Beam remains consistent with the replay. Session generation allows host to discard flow/fade previous.

Incomplete legacy files are safely rejected; there is no reliable conversion of registers that have never been recorded. Preserve the original saves as fixtures and do not overwrite them with migration attempts.

### Custom Slots and Files

The slots of UI are 1..10; internally they can use indexes 0.9. Paths are related to the executable directory:

```text
saves/GGA1_save_01.sav
...
saves/GGA1_save_10.sav
```

Save silently overwrites. **F1 loads Slot 1; Shift+F1 saves Slot 1.** Missing Slot is ignored before calling the loader, without dialog box, visual alert or interruption. This does not require you to hide error from an existing invalid file.

`Load save from file...` uses native dialog SDL filtered to `.sav`, initially in `saves/`. `Select ROM...` filter `.sfc/.smc`, open in the executable folder in the first selection and then in the last persistent folder ROM. A ROM chosen is validated and the host closes/reopens Cleanly on the Windows path. Do not assume that non-Windows reboot has the same full implementation.

### Reset and end of game

Reboot restarts core and wrapper session states, including clocks and pending audio, to return to Banpresto boot. It is not equivalent to just putting the PC on reset vector.

In the score `$2D00/$98B4`, a start edge fires fade of approximately 60 frames, followed by the complete reset of the core/session. The replay test recorded black screen in frame 73 and reset in 74; these numbers depend on the input sequence, they are not universal constants of the function.

When UI capture input, the wrapper neutralizes guest input and treats key release, preventing a menu selection from also triggering the character or Start.

## 8. UI Current: Dear ImGui, accessibility and sources

### Functional structure

```text
File
 ├─ Select ROM...
 ├─ Save
 │   ├─ Slot 1    Shift+F1
 │   ├─ Slot 2
 │   └─ ... Slot 10
 ├─ Load
 │   ├─ Slot 1    F1
 │   ├─ Slot 2
 │   └─ ... Slot 10
 ├─ Load save from file...
 ├─ Reboot
 └─ Quit game
```

There's no more bar HMENU based on `SetMenu/CreateMenu`, nor dependence on ordinals of `uxtheme.dll` to paint menus. The Win32 attempt preserved native navigation, but did not satisfactorily resolve the responsive appearance of the classic bar. The previous custom implementation also presented hit conflicts. testing/resizing. These approaches have been replaced, should not be reactivated in parallel.

A ImGui draw with the same SDL_Renderer from the game. In window, the bar height is reserved and the display of SNES Yeah. offset/scaled so you don't get covered. The draw date of UI is presented after the game; the host also updates the UI when paused. The backends are closed before you destroy the renderer.

Commands chosen in the drawing are lined in event SDL own; load/reset the core during a presentation operation is not safe.

### Visibility and keyboard: current requirements

- In window, the bar appears by default.
- On fullscreen, it is hidden by default and the game occupies the entire area.
- **Esc is global toggle**In both ways. On fullscreen, the bar appears superimposed, without booking height; new Esc hides it.
- This later rule replaced the old request for mandatory and permanent fullscreen concealment.
- Alt isolated should just focus the bar and expose the Mnemonic File without opening the dropdown.
- F or down arrow, focusing on the bar, opens File. Inside the dropdown, arrows/Enter follow navigation ImGui; Alt+Enter is still intended for the host fullscreen.
- `ImGuiConfigFlags_NavEnableKeyboard` It's active.
- Labels with `&` are treated by `MnemMenuLabel`: remove marker and draw underlined in the correct context. Shortcuts include Select, Save, Load, File, Reboot and Quit; digits select slots, with 0 for Slot 10.
- Only the deepest popup should respond to the mnemonic; avoid simultaneously activating parent and child.

Esc's behavior is intentionally global, not the traditional closure of a single level of the menu. Do not “correct” it to the pattern ImGui contrary to the user specification.

### DPI and theme

The host query `SDL_GetWindowDisplayScale(window)` and adapts rasterized font size and dimensions of the UI. The base font size is 16 pixels multiplied by the scale factor. When Changing DPI/theme, restores the base style before `ScaleAllSizes`, avoiding cumulative multiplication. The font texture is reconstructed when necessary.

The palette uses the theme informed by SDL: `StyleColorsDark` or `StyleColorsLight`. Do not force dark into a clear system. The positioning considers window coordinates and renderer pixels; mixing these units recreates problems at 125%/150%.

### Multiplatform and fallback discovery

|Platform|Current Discovery|
|---|---|
|Windows|`SystemParametersInfoW` with `NONCLIENTMETRICSW/lfMenuFont`; installed font resolution and Windows Fonts directory|
|Linux|Optional Fontconfig in CMake; `FcFontMatch` for `sans-serif`, outline/scalable|
|macOS|CoreText: `CTFontCreateUIFontForLanguage`, attribute `kCTFontURLAttribute`, conversion of URL to path UTF-8|
|Fallback|Open Sans local and, if necessary, bytes embedded in the executable|

No macOS, CoreText/CoreFoundation are linked conditionally and returned objects are released. If there isn't URL usable, use fallback; do not depend on a fixed Apple file name.

The user authorized file is **`assets/fonts/OpenSans-Regular.ttf`**, replacing the initial proposal of variable source. It has 130,832 bytes and license **SIL Open Font License 1.1**, kept with the assets. O CMake generates its embedded representation, linked to the module ImGui. Do not redistribute Segoe UI/SF Pro or other proprietary fonts found in SO; just load them from the local facility.

Fallback is not the default pixel art font of ImGui. The absence of a font file in the installation directory should not overturn the program, as there is the built-in copy. Test variables: `ACKMAN_TEST_FONT_FALLBACK=1` ignores the source of SO; `ACKMAN_TEST_FONT_NO_FILES=1` Force the path without external files.

Branches Linux/macOS were implemented conditionally, but the validation performed in this environment is Windows. Compilation and inspection on Windows do not prove the native experience on these systems.

## 9. Build, tests and evidence for the next instance

### Local reference commands

Run at the root, adapting paths only when needed:

```powershell
& .\.tools\cmake-3.31.6-windows-x86_64\bin\cmake.exe --build build-validation -j 8
& .\.tools\cmake-3.31.6-windows-x86_64\bin\ctest.exe --test-dir build-validation --output-on-failure
```

The directory is already configured for Release. For reconfiguration, see CMakeCache and the current options before recreating an environment or downloading another version of the framework.

The correct script is **`scripts/smoke.py`**, no `smoke.pyscripts`. Example of the organization used to avoid changing the main configuration:

```powershell
& C:/msys64/mingw64/bin/python.exe scripts/smoke.py `
  --exe private/menu-smoke/AckmanRecomp.exe `
  --rom 'R:/ChatGPT/Go Go Ackman (Japan).sfc' `
  --frames 1000 `
  --config private/menu-smoke/fast.ini `
  --script private/menu-smoke/replay.txt `
  --timeout 60
```

This example presupposes the candidate executable updated in the isolated folder. Do not inadvertently reuse an old binary and assign its results to the new code. The smoke can copy configuration to the executable, so it should not be randomly pointed to the everyday usage executable.

For scene regression, `scripts/validate_scenes.py` uses the manifest of `private/scene-audit`, saves in `build-ninja/saves`, baseline and separate output folder. Consult `--help` before altering the invocation. The original files must remain intact; the runner works with copies in slots from isolated directories.

### Current coverage

The 11 recorded tests are:

```text
raster_scroll_windows       buffered_logging
rom_and_trace_tools         audio_routing
debug_keys                  video_margins
final_score_reset           menu_persistence
menu_sdl_fullscreen         menu_open_sans_fallback
menu_embedded_open_sans
```

Scene regression combines execution of 1,000 frames per save, state comparison RAM/VRAM/OAM/SPC, image samples, region center/masks and observation of private memory of the process. The scene of the car floor has a specific native central reference; it does not require identity of the lateral pixels just where an intentional correction was made.

Sources of evidence:

- `build-validation/Testing/Temporary/LastTest.log`: round CTest current, 11 tests approved.
- `private/scene-imgui-consolidated/results.json`: final regression of 51 saves.
- `private/scene-fontconfig-candidate/results.json`: previous candidate stage.
- `private/pre-coretext-release/AckmanRecomp.exe`: backup prior to last consolidation.
- `docs/RETOMADA.md`: chronological narrative, commands and intermediate decisions.

One `LastTestsFailed.log` old can remain after a later round approved. Check timestamps and `LastTest.log`, without declaring regression only by the existence of the old file.

Tests do not replace ASan/LSan, exhaustive comparison with hardware or a reference emulator, hearing inspection or physical screening. The usual configuration uses VSync, audio 48 kHz stereo and buffer of 512 samples; headless measurements do not reproduce all gameplay conditions.

## 10. Known pendencies and validation limits

### Pending UI explicitly left for later

**There is a residual mnemonic/underlined bug reported by the user.** A UI is functional, but should not be considered 100% as visually. The recent history includes Alt focus corrections, down arrow and underlined drawing; the approval of the tests does not complete the manual report. There is not in this consolidation a final playback sufficiently precise to assign cause or option affected. The next session must capture the key sequence, mode window/fullscreen and DPI before changing `MnemMenuLabel` or navigation.

### Limits that must remain explicit

- Stable Timing in the adopted model is not universal execution cycle-exact.
- Savestatee of the core is complete for validated resume; music cursor PCM It's not restored.
- 51 reported full saves and gameplay are strong coverage, but not all possible scenes and combinations of culling.
- Absence of memory growth in the sample is not formal proof of absence of leaks.
- Fontconfig/CoreText require future native validation; consolidated build is Windows.

## 11. Resumption script and related documents

Rewind VHS (12/10/2026): replaced the modal selection with hold-to-rewind using exclusively `[KeyMap] Rewind` (default 8). `[General] RewindDepth = 240` Stays out of the UI; `RewindInterval = 4` is also exclusive to the INI (Accepts) 2/4/6), with fallback 4 and default history of 16 seconds. No thumbnails/filmstrip of rewind. Full snapshots have 315.991 bytes and keep raster journal and fade context in RAM; replay does not advance CPU/APU, restores the logical state again after drawing and discards the abandoned future. Stream remains paused during hold and receives deep flush in release. Hybrid shortcuts, Alt/focus, run-ahead, persistence and 3 × 1,000 validated frames. View `docs/HOLD-REWIND.md` and evidence in `private/hold-rewind-candidate/`. Playable Stack now contains 24 patches; includes shell separator correction from multiple mod catalog on Windows.

Importer IPS adaptive (11/10/2026): MD5 English `0dd930e3738524cc4f8a3d83326616a9`; PT- It's okay.BR E2P `7cc729dde829946791d18a2b27e48da6`. The native converter applies the English table embedded in RAM before the overlay PT- It's okay.BR and publish `GoGoAckman_PTBR.snesmod` Self-sufficient. IPS unknowns use `ackman_ips.c`, with limited parsing, source guards and generic package identified by MD5; there is no Python dependency on the executable. Full metadata E2P are fixed in `ackman_translation_ptbr.h`; the panel maintains all line breaks and scroll. Log in `docs/Compatible translations.txt`; private evidence in `private/ptbr-translation-candidate/`. 14 CTests and 1,000 frames in PT-BR/fallback approved; full gameplay PT- It's okay.BR It's not covered by these tests yet.

Audio Update (11/10/2026): loading saves in file uses a transaction `AckmanHostStateLoadBegin/End` ed `ackman_timing.c`, with pause and stream lock before restoration, buffer cleaning SDL/host and reset the output ring and resampler after reconciliation. Preserves the guest musical state of DSP and the pause rules of menu/focus. Playable Patch `ackman-audio-load.patch` (21 patches in total); 14 CTests and 1,000 frames in each of the two approved cutscene saves. Evidence in `private/audio-load-candidate/RESULTADOS.md`.

For new features of UI, start with `ackman_menu.c`, `ackman_menu_imgui.cpp`, `ackman_font.cpp`, menu tests and integration patch SDL. Preserve F1/Shift+F1, paths related to executable, empty slot silence, fixed security escape and configurable secondary menu and separation between command of UI and core mutation.

Do not reopen audio, video or clock fixes without evidence that the requested change affects them. Do not replace the 51 saves, their baselines or previous reports to take a “pass” test. Create a new result folder and identify the tested binary. Just promote another `AckmanRecomp.exe` after the appropriate checks to the scope.

Additional readings, all related to this folder:

- [RETOMADA.md](RETOMADA.md): detailed history and consolidation milestones.
- [WIDESCREEN-SCENES.md](WIDESCREEN-SCENES.md): policies and fixtures of scene.
- [UI-SAVESTATES.md](UI-SAVESTATES.md): interface and persistence contracts.
- [MODS_MSU_WIDE.md](MODS_MSU_WIDE.md): operation of experimental resources.
- [DEBUG-GAMEPLAY.md](DEBUG-GAMEPLAY.md): evidence collection during gameplay.
- [VALIDACAO.md](VALIDACAO.md): methodology and validation limits.
- [RESULTADOS.md](RESULTADOS.md): registered results of the project.

This file consolidates the transfer point. In future changes, update the current state and evidence without erasing the distinction between measured facts, user requirements and unverified hypotheses.


Consolidated menu and navigation: `[KeyMap] Menu` accepts keyboard, combinations and gamepad; alphabetic item in Hotkeys tab. Escape remains cancellation of the wizards. ↑ in the active tab goes to the last item, ↓ in the last one returns to the tab; works with keyboard, D-Pad and analog, including daughter lists with scrolling. Patch `ackman-menu-loop.patch` (24 patches). Validation recorded in `private/menu-loop-candidate/RESULTADOS.md`.


Vertical navigation correction (12/10/2026): the focused label does not determine the open content. ↑ in the first item returns to the open tab; ↑ on any label, including an unselected neighbor, goes to the last item of the visible tab. Tests SDL/ImGui expanded the two streams to the seven tabs with keyboard and virtual D-Pad. Evidence in `private/menu-focus-candidate/RESULTADOS.md`.


Native Rumble and Security Menu (12/10/2026): Escape is fixed/hidden; Menu continues as secondary shortcut, fallback None, preserving existing bindings. ControllerVibration in General has fallback 1 and checkbox in the Game tab. New ackman_rumble.c turn on SDL_RumbleJoystick collisions, explosions, scripts A34D, car/castle/ending and impact of Tenshi (700 ms). Gates of pause/load/rewind They cancel vibration, and speculative frames don't produce effects. No changes in save format or video clipping. Patch ackman-rumble.patch completes 25 patches in 20 framework files. 14 CTests approved; 10 × 1,200 frames of the saves, OFF/run-ahead 2 and 8 combat replays validated. Virtual control SDL; lacks only tactile appreciation in physical hardware by the user. View HAPTIC-RUMBLE.md and private/rumble-candidate/RESULTADOS.md.


Tactile refining (12/10/2026): Flyer linked to collisions confirmed in 878DE3/878C2D; Load uses 0B3A/0414 And it only generates strong hits in the ready-to-reach release. A34D pulse during geometric contact on D5, 11 frames before EF in stage 1 save; stage 5 receives single catch-up after load lock. Castle: A22E the kinematics continues A1D9, with ignition pulse, flight and end in black endpoint; do not confuse the pointer A22E retained in the 2900 selector gameplay with kinematics. Final Flash 9BE4/1F It gets 900 ms strong. No clipping, configuration or save format changes. 14/14 CTests; eight saves × 2,400 frames and six regressions × 1,100 frames, including run-ahead 2, OFF and partial charge.

Additional rumble refinement (12/10/2026): flying kick/cars use effective reduction of actors' lives in 7F:C064, in commits 85:9905/86:9940 instead of contact/SFX. Continuous load requires ready latch + animation 0448=13/14, with dialogue block 0491 bit 1; intensity reduced to 6000/7000. Phase 0500 mine: setup 86:A141, class 061C, busy car and proximity; robust pulse of 300 ms. Initial explosion of the castle: exactly 400 ms. The single pulse of 900 ms of the final flash was replaced by pulses of 150 ms in the KON channel reals SPC marked B0 (SFX 30), restricted to the white phase 9BE4 >= 1F. Communication DSP -> main loop uses an atomic state set to prevent late callbacks after pause/load and excludes run-ahead. Evidence: private/rumble-sync-candidate, 14 CTests and 12 replays of 1300 frames including miss, incomplete load, dialogue, car, mine and real audio on the driver SDL Dummy. Virtual control SDL; pending physical tactile evaluation.

Further calibration of mine/SPC (12/10/2026): the new mine save shows that C006=061C/1A1C is map address, not object class. ID now uses status 30/70 the near object, SFX 1D and negative Y thrust commit 87:AB06 consuming 04D5&2000, with short generation and validity. Mina gets it. 9000/A000 for 110 ms, equal to player damage; first impact on host frame 5 of this new save. The castle's initial explosion no longer uses 400 ms: KON Do SFX 38 (tag SPC B8) starts the effect and the end of the command in the driver closes it. In the replay goes from frame 61 to 145 (~1.4 s), without coded limit. Poll under lock after real frames preserves the end with run-ahead. Later continuous flight and acoustic pulses of the final flash were preserved. Evidence in private/rumble-spc-duration-candidate: 14/14 CTests And seven replays of 1300 approved frames. No physical tactile test.

Defeat of the bosses (12/10/2026): hook de HP zero and reconstruction of the seven actors/rooms/scripts verified activate continuous rumble 8000/8000. Sequence 86:A452/A4B8 has 16 explosions with cadence of 10 frames; keeps the vibration between shots and, after complete/remove the actor, even the SPC clean 9D tag from SFX 1D. Sound tables are relocated between seats: validate driver options and their stores in 28..2F, not fixed table address. Native classes 64/69/6B/66/5E/6F/75 are combined to room and script. Source of normal damage, mine, castle and final flash preserved. Evidence: private/boss-rumble-candidate, 14/14 CTests and 14 replays of 1500 frames (seven heads + regressions), including run-ahead 2 and vibration OFF. Virtual control SDL; no physical tactile evaluation.

Mods/peripherals (12/10/2026): titles Available/Active are off the lists, with the upper border displaced by GetTextLineHeightWithSpacing and preserved metadata panel. ackman_devices.c receives events SDL added/removed and opens/closes devices in a worker without holding the mutex from the queue during IO. Ready Event publishes handsles for slot assignment; removal removes pointers from ImGui before the closing. Backend ImGui uses manual cache gamepads without discovery/opening in NewFrame. Rumble keeps IDs by events, removing GetJoysticks from the shots. The unique boot enumeration is outside the frames. New ackman-device-events.patch closes the stack of 26 patches. Evidence in private/devices-mods-candidate: 14/14 CTests, virtual test with 200 ms opening in the worker, disconnection during opening, fixture isolated from other devices, smoke 1000 frames and image menu-mods.png Check. No hotplug latency measurement USB/Bluetooth physical; SDL/driver They still control their own locks and pumping events. Details in HAPTIC-RUMBLE.md and private/rumble-refine-candidate/RESULTADOS.md.

Shaders and presentation (2026-10-12): Shaders is the eighth tab after Mods, with exclusive radio-checkbox selection, logical state retained when effects/backend are disabled, and an undimmed white warning. Graphics adds IntegerScaling; EnableShaders and SelectedShader (basename) persist and missing selections are cleared. Framework GLSL uses the SDL renderer OpenGL context and an intermediate target with GL state restoration and vertical-orientation compensation. SDL_RenderReadPixels before Present works across all three outputs; this stage wrote BMP screenshots. Fixed Tab/Turbo, R/Rewind, and F12/Screenshot shortcuts coexist with configurable bindings; holds combine by OR and clear on focus loss. Integer scaling uses a whole-number native-resolution multiplier; below 1x it clips centrally, retaining CRT PAR and safe PPU limits. Patch stack: 27, including ackman-shaders-capture.patch. All 14 CTests passed (63.53 s), including real shader RGB/orientation assertions, disabled-effect behavior, exclusive radios, retained white warning, whole viewport, and immutable/hybrid shortcuts. SDL software/dummy and native Windows 1000-frame replays produced seven captures. Evidence: private/shaders-candidate/RESULTADOS.md.

PNG/shaders recursive (12/10/2026): capturees now PNG RGBA opaque (alpha 255), sub filter and zlib 6 in worker SDL with limited queue and drainage in the shutdown. ScreenshotResolution WindowSize (default) / InternalResolution select physical backbuffer or extended native logical frame without WindowScale. Selector below Integer scaling, included in width global/autofit. EnableShaders and SelectedShader They persist immediately and upon closing. Recursive catalog.glsl/.glslp saves relative paths; without symlinks/reparse, limits 32 levels/4096 files, clean selection missing. Folders use dropdown Save/Load, children retreat exactly two spaces per level including the clearance of the arrow; active yellow ancestors in OpenGL, gray unavailable checkboxes and preserved white warning. Folder navigation works with effects OFF. Static Zlib via find_package, ZLIB_ROOT site C:/msys64/mingw64; 28 patches with ackman-png-shader-tree.patch. 14/14 CTests (64,94 s), PNG decoded with alpha/dimensions verified, actual nested directoriesed preset, INI immediate and two 1000 frame replays. Seven PNGs, > 95% observed compression versus RGBA raw. Evidence: private/png-shader-tree-candidate/RESULTADOS.md and shader-tree.png.

Boot/raster/menu (12/10/2026): OutputMethod It's boot authority, no override Renderer. Key InternalResolution replace WindowScale; old compatibility with priority of the new key. Raster Entire 1–10 in the host buffer PPU, multiplied native captures and update of frozen frame without RtlRunFrame. MenuContent with dedicated scroll, static tabs, Home in active tab, IDs unique by shaders node and yellow subject to attenuation. Process supervisor with 120 presentations/3 seconds health window and 30 seconds watchdog; selector ROM It doesn't expire. Timed history of up to five INIs, binary comparison, temporary attempts and atomic promotion after success; preserves files from others, travels historical and tries factory once. Session shader failed returns RAM visual, preserves saved preference, checkpoint and cold boot with resume even if Autosave OFF. Separate process captures also locks and access violation on driver. 29 patches, ackman-safe-boot-raster.patch. 14/14 CTests (66.32 s), smoke 1000 frames and PNGs 2x/3x/10x verified; recovery by second history, factory, watchdog with suspended child, row five and cold boot after live failure. Original Preset Royale caused 0xC0000005 on this driver; supervisor recovered SDL and completed 350 frames. Evidence: private/boot-raster-candidate/RESULTADOS.md.

Cycle of driver/cold boot (13/10/2026): static defaults/INI/factory OpenGL. Fixed asset surrenderer per process; OutputMethod in the menu is preference pending, without recreation SDL/OpenGL in session. Active driver snapshot when opening; returning to the same preference does nothing. Difference when closing off EnableShaders for SDL/Software, atomic checkpoint, persistence and cold boot with --ackman-graphics-restart argument. Resume before first frame ignores Autosave OFF, without changing that option. Core/audio pause covers the pending exit. Forced checkpoint before entering the shader; recovery does not overwrite it with RAM initial. Supervisor corrects increment that skipped the current profile: first emergency tries new preference, only after history. Propaga summarizes in all emergencies, recognizes status of session with shader and does not repeat input scripts in recoveries. Later journey is limited by profiles. 30 patches; new ackman-graphics-coldboot.patch. 14/14 CTests (64.95 s), smoke 1000 frames with change cancelled, three transitions per cold boot verified, and two injected failures proved current->>historical, two native loads with Autosave OFF and checkpoint with preserved hash. Original Royale completed 450 frames without reproducing the crash on this candidate, without changes. Evidence: private/graphics-coldboot-candidate/RESULTADOS.md.


Consolidated State and delayed loading (13/10/2026): deep snapshot of OutputMethod selected, ActiveMods, EnableShaders and SelectedShader each opening of the menu. Renderer/shader assets attached to boot; widgets only RAM. Close without differences (including reversals) does not restart. Any difference: sanitize shaders only in changing driver to SDL/Software, forced checkpoint, atomic transaction of driver/shaders/mods, cold boot and resumes mandatory before the first frame, even with Autosave OFF. Failed to checkpoint/write maintains menu/core paused and not published INI Partial. Quit persists normally. Mods alert snapshot is also renewed by opening. Private flag --ackman-graphics-restart covers all these reboot reasons. 31 patches; ackman-unified-state.patch enables options/host keys scripts while the menu pauses the core, without consuming gameplay/RAM pokes/saves. 14/14 CTests (64.21 s), 1000 frames final smoke with the four reverse changes and no reboot. Isolated changes of shader/mods and joint software were natively restored with Autosave OFF. Invalid Shader is only compiled on the new boot. Fallback queue limited to five, oldest discarded, file of others preserved; rollback set of shader/mods and two resumes before gameplay proven by native trace. Evidence: private/unified-state-candidate/RESULTADOS.md.


Inactive shader filter (13/10/2026): at the closing of the menu, exclusive change of SelectedShader with EnableShaders OFF both snapshot and output does not create checkpoint, does not publish settings at this time and does not restart. Preferencing preserved in RAM and saved at normal closure. Driver/mods/flag changed or shader selection ON follow the consolidated cold boot. Exception does not suppress actual renderer failure recovery. 14/14 CTests (64.24 s) and native smoke of 1000 frames, an instance without checkpoint/reboot, selected option persisted in shutdown. INI from the preserved root. Evidence: private/inactive-shader-candidate/RESULTADOS.md.


Clipping/IDs interface (13/10/2026): Restort needed warning removed from Mods. UI actual on ackman_menu_imgui.cpp; MenuContent it receives explicit available height. Rows dropdown full width now respect InnerClipRect vertical, without changing the width of the highlights. Shaders use prefix/index/ nested path scopes. Additional cause of the warning of IDs: BeginChild had already submitted to native scrollbar and the ScrollbarEx manual submitted it again with the same ID. Now there is only native interaction and an additional design of the indicator, without overlapping widgets. Fixed channel remains invisible in the short tabs. 14/14 CTests (63.41 s), 40-shader test, basenames/folders repeated, drag with ID single, submission count = 1 and DebugDrawIdConflicts = 0. Captures of middle/end and Mods inspected: strict clipping without obsolete warning. INI from the preserved root. Evidence: private/shader-scroll-candidate/RESULTADOS.md.


Embedded vector assets (2026-10-13): CMake and scripts/embed_buttons.py convert 130 controller/keyboard SVGs to static const unsigned char arrays. NanoSVG is linked statically from external/nanosvg at a pinned commit with its license retained; runtime needs neither disk SVGs nor an image DLL. ackman_menu_icons.cpp/ackman_svg.c rasterize in RAM into SDL_Texture/ImTextureID, preserving aspect, current DPI/height, and cached variants released after rendering/shutdown. Controls and Hotkeys share icons keyed by raw binding/device; Ctrl/Alt/Shift/Gui combinations draw icons separated by +; empty bindings draw nothing. Capture and conflicts remain textual. Q/LB and E/RB legends are centered outside the tab labels, symmetric, and included in autofit. Joystick bindings without an asset keep truthful text; unsupported keyboard keys use ANY. All 14 CTests passed (65.12 s), checking every SVG, the dictionary, and geometry across resize/DPI/fonts. Native 1000-frame smoke without loose assets opened Controls/Hotkeys without reboot. Keyboard/gamepad/slot captures were inspected; original config.ini preserved. Evidence: private/button-icons-candidate/RESULTADOS.md.

Symmetry/tinting (2026-10-13): the SVG cache measures nonzero alpha bounds. Q/LB/E/RB legends use ink UVs, equal integer heights, and aspect-preserving widths. Keyboard glyphs are slightly larger (target 1.35 font heights versus 1.2 previously), with safe margins and lateral centering; / is centered by font ink. Controls/Hotkeys tint icons and + to RGBA 255,64,64,255 on conflicts through ImGui commands, without changing shared textures or input logic. All 14 CTests passed (64.94 s), checking resize/DPI geometry and red command tint. Both tab captures were inspected; root INI preserved. Evidence: private/icon-symmetry-candidate/RESULTADOS.md.

Contextual conflicts (2026-10-13): AckmanConflict identifies the friendly name and Hotkey/Keyboard/Gamepad origin. Bidirectional typed queries serve both tabs; string APIs are wrappers. Exact comparison and existing physical-overlap rules are retained. Inline alerts show Already used in + [hotkey], or keyboard.svg/controller_generic.svg before [SNES command]. SameLine and safe wrapping operate in the third column. ImageWithBg crops/tints to 255,64,64,255 at the smaller font height; LastItemData/layout/scale are restored to preserve focus/geometry. CMake invalidates the SVG catalog even for newly added files with old timestamps. The catalog now has 132 assets, including both device icons. All 14 CTests passed (66.07 s), checking origin/capacity, texture tint, height, and input. Keyboard/gamepad/hotkey captures inspected; INI preserved. Evidence: private/device-conflicts-candidate/RESULTADOS.md.

Gamepad restoration (2026-10-13): runner P1 depended on SDL seat 0 while the menu recognized all published devices. A seat-1 gamepad reproduced working menu input but absent gameplay input. AckmanControlsReadDevices combines the cache for single-player P1; explicit P2 configuration retains seat assignments. Post-menu draining uses the cache without SDL_GetGamepads in the loop. SVGs are presentation only; bindings/states are unchanged. Patch 32 was reproduced byte-for-byte through CMake. All 14 CTests passed (69.83 s); a 1000-frame smoke with an SDL virtual gamepad on the second seat confirmed D-pad, analog, and jump before/after icon rendering. Keyboard checks copied a virtual snapshot into the actual mapper, without physical Windows input injection. Original config.ini preserved. Evidence: private/gamepad-input-candidate/RESULTADOS.md.

SNES modularization (2026-10-13): recomp_menu_core, alias SNES::MenuCore, exposes a C API with C++17 ImGui implementation. Geometry, widgets, SVGs, fonts, cursor, remapping/hotkeys, asynchronous device cache, mod/shader models, checkpoints, cold boot, and watchdog/five-backup FIFO moved to src/recomp_menu_*. ackman_menu.c keeps the local profile, INI bindings, and native MD5 EN/PTBR converter; RecompMenuHost injects ROM/INI/video/rumble services. Old headers are aliases without duplicate implementations. The SNES matrix and eight tabs are centralized. Reusable CMake module: cmake/RecompMenuCore.cmake. All 14 CTests passed (69.45 s), as did two 1000-frame smokes, driver/mod/shader simulations, native recovery with Autosave OFF, and FIFO checks. A separate CMake consumer without Ackman sources compiled/ran; the library has no external Ackman/GGA1 symbols. Original INI preserved. Guide: docs/RECOMP_MENU_CORE.md. Evidence: private/menu-core-candidate/RESULTADOS.md.

Flatpak (2026-10-13): org.snesrecomp.AckmanRecomp.yaml uses Freedesktop runtime/SDK 26.08, SDL3 3.4.10, static zlib 1.3.2, pinned hashes, and framework.lock. CMake installs desktop entry/icon. ACKMAN_FLATPAK separates read-only /app/share assets from XDG_CONFIG_HOME configuration/saves/mods/fallback through recomp_menu_paths. Supervisor, child, and cold boot retain the persistent working directory. ROM picker and screenshots use that root. Native selection uses the portal and SDL application ID. Original icon.png 360x308 remains unchanged; the installed 360x360 copy has transparent padding for Flatpak validation. BUILD_TESTING defaults ON; packaging sets OFF. All 14 CTests passed (68.76 s), including XDG/missing/truncation checks, plus a 1000-frame smoke. YAML, real CMake staging, and shell syntax passed. At this stage Linux/WSL was unavailable; the ELF/bundle was not yet built/run. Automation: scripts/build_flatpak.sh. Guide: docs/FLATPAK.md. Evidence: private/flatpak-candidate.

CRLF/LF Flatpak (2026-10-13): normalize_patches.py converts .patch/.diff and their text targets to LF before the CMake patch stack. At this stage it also ran in build_flatpak.sh, and CMake repeated it inside the sandbox before reverse/check/apply. Git uses autocrlf=false + eol=lf per invocation without global changes; .gitattributes sets LF. All 32 patches applied cleanly to an LF clone with mixed CRLF/LF patch fixtures. Reverse/reapply is idempotent; 21 final files are canonically identical. Rebuild and 14 CTests passed (70.39 s). Config/ROM/saves preserved. No Linux host was available then; repeat ELF/Flatpak build on Linux. Evidence: private/patch-lf-candidate/RESULTADOS.md and clean-stack.log.

Flatpak catalog (2026-10-13): ACKMAN_FLATPAK=ON passes NONE to snesrecomp_target_mod_catalog; sandbox sources need no mods/preloaded directory. Runtime mods remain enabled in persistent XDG storage. The conventional branch is preserved. Fixtures using the real function/conditional configured and compiled without mods/; Windows rebuild and 14 CTests passed (69.76 s). Flatpak execution stopped because flatpak/flatpak-builder were absent on the Windows host; ELF remained pending. Patch/evidence: private/flatpak-catalog-candidate/RESULTADOS.md.

Flatpak/Ninja staging (2026-10-13): normalization was removed from CMake and the host script. The manifest runs normalize_patches.py and cmake -P cmake/AckmanPatches.cmake in a shell source after extraction/checkout and before configuration. Flatpak does not reverse/reapply patches during configure; runner.cmake timestamps remain stable. The conventional applicator was extracted without changing the 32-patch order. Clean checkout and repeated configuration/build confirmed 21 identical files, stable timestamps, and Ninja with no work. Manifest/shell/exported patch validated; rebuild and 14 CTests passed (71.27 s). ELF remained pending on Linux. Evidence: private/flatpak-staging-candidate/RESULTADOS.md.

GNU host report (2026-10-13): ackman-host-report-gnu.patch moves guarded _GNU_SOURCE to the absolute top of host_report.c; link.h already belongs to the Linux block. Forced ackman_log_redirect.h includes stdio.h before source parsing, so CMake defines _GNU_SOURCE=1 for this Linux translation unit. Staging applies 33 patches to 22 targets; clean checkout/repeated Ninja preserve timestamps. Windows build and 14 CTests passed (70.13 s). Flatpak execution stopped for missing flatpak/flatpak-builder; ELF remained pending. Evidence/patch: private/host-report-gnu-candidate/RESULTADOS.md.

Linux export completed (2026-10-13): the script cleans only temporary AckmanRecomp builds and uses force-clean + disable-cache + assumeyes. Ubuntu/WSL became accessible; SDK/Platform 26.08 were installed. /mnt/r rejected ccache permissions, so sources were copied to a Linux filesystem for building. zlib/SDL3/AckmanRecomp build and export passed without a Ninja loop. dist/AckmanRecomp-x86_64.flatpak was 25,973,296 bytes; SHA256 1e145df23ce11fef0ee0d252b6381d734aa19ebbcb030ecfafff96f8e44ce11f. ELF x86_64, static zlib, and OSTree bundle import verified. All 14 CTests passed (73.69 s), plus 11 tooling tests. Linux gameplay was not run. Evidence: private/flatpak-clean-candidate/RESULTADOS.md.

Assets/HiDPI/timestamps (2026-10-13): fonts/ and buttons/ are installed under /app/share/org.snesrecomp.AckmanRecomp; fonts/SVGs remain embedded. The loader prioritizes the installed font and calls AddFontDefault on construction failure. SDL overlay geometry uses DisplayFramebufferScale and restores the previous scale afterward. Future timestamps (~2 hours) were confirmed and corrected during staging before CMake. disable-cache remains; builder 1.4.8 has no clear-cache option, so fallback purges local cache metadata and temporary builds. All 14 CTests passed (65.28 s), including HiDPI 2x pixel/path checks. Ubuntu/WSL bundle export/import passed; CachyOS visual validation was unavailable. Updated dist/AckmanRecomp-x86_64.flatpak SHA256: 5c18f6f9e5218982b0d5ec0ce70c8e02784e25173182c68b022d42cefc502618. Patch/evidence: private/flatpak-assets-candidate/RESULTADOS.md.

Platform menu/feedback (2026-10-14): SDL fullscreen synchronization, real-window-flag getter, Alt reset/reconciliation, and physical software backbuffer handling are restricted to __linux__ && RECOMP_MENU_FLATPAK. Windows behavior is preserved. Enter/A globally advance selectors like Right. Host ABI version 2 adds optional RumbleActivationFeedback; the Ackman adapter sends medium intensity 0x8000/0x8000 for 300 ms only on UI OFF→ON activation, retaining the pulse while paused. Virtual gamepad/selector tests, all 14 CTests (67.27 s), Windows build, and Ubuntu/WSL Flatpak export/import passed. CachyOS practical validation was unavailable. Bundle SHA256: f2bec50178234ff102728cc058b82fd4a13997dc0d2de725ff5ba312105a81fd. Evidence: private/menu-platform-candidate/RESULTADOS.md.

Fullscreen debounce/focus (2026-10-14): the core exposes a checkbox-specific setter carrying gamepad origin. Linux/Flatpak rejects repeats for 300 ms after SDL_SyncWindow, without affecting keyboard/mouse/Windows. A probe of the actual function confirms 299/300 ms boundaries and the Windows branch without cooldown. Windows hides virtual mouse position/hover when leaving fullscreen; resize anchors global coordinates and synthetic motion does not move NavId. New physical motion/click unlocks focus. All 14 CTests passed (71.14 s), including focus regression; the executable was promoted and the WSL bundle exported/imported. Bundle SHA256: 72517b6615b932ceb054007ddf45b01acc2825b6c0dd4e6c0031a97b4b777239. No physical test on inaccessible CachyOS. Evidence: private/menu-debounce-candidate/RESULTADOS.md.

Windows fullscreen hover guard (2026-10-14): the transition locks mouse focus
at dispatch for menu, keyboard, and gamepad shortcuts, in both directions.
Resize/move events refresh the global anchor. Unlock requires a nonzero SDL
motion delta and changed global pointer position; static clicks stay blocked.
Queued mouse-only events from the SDL backend's per-frame polling are removed
before ImGui NewFrame, preserving keyboard/gamepad events and NavId. All 14
CTests passed (68.38 s), including native Windows SDL and deterministic ghost-
hover/movement cases. Linux/Flatpak behavior remains unchanged. Evidence:
private/fullscreen-hover-candidate/RESULTADOS.md.

OSD and official name (2026-10-14): FPS/Turbo/toast and volume panel geometry
now follows the actual internal raster scale, preserving 1x-relative size and
margins through 10x. Patch 34, ackman-osd-scale.patch, links runner composition
to AckmanVideoBlitOsd; unscaled oracle frames remain at 1x. The Ackman adapter,
SDL window/app metadata, README, and Linux desktop entry use Go Go Ackman
Recompiled without coupling the generic menu core to the game's name. All 14
CTests passed (70.27 s), including actual OSD-image pixel bounds for scales 1..10.
Ubuntu/WSL Flatpak export/import and installed desktop/static-zlib checks passed;
CachyOS visual testing was not performed. Evidence:
private/osd-branding-candidate/RESULTADOS.md.

Official icon refresh (2026-10-14): root icon.png now exactly matches the supplied
icon_noborder.png (360x360). Windows regenerated ICO/RC and embedded resources;
Flatpak always generates a 360x360 RGBA canvas, preserving this square input
without extra margins and centering rectangular artwork with transparency.
All 14 CTests passed (68.97 s). Ubuntu/WSL bundle export/import and pixel-exact
installed-icon checks passed; no CachyOS visual test was performed. Evidence:
private/icon-refresh-candidate/RESULTADOS.md.
