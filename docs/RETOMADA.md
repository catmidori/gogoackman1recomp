# Reverse Engineering Resumption

The local build uses `scripts/smoke.py` (no) `smoke.pyscripts`). The root folder does not provide a Git diff that allows you to assign exactly 13 files to a previous session. The present sources were inspected and recompiled.

## Audio

Tracking `ACKMAN_AUDIO_TRACE=<arquivo.csv>` records A, Y, DP, SP And three raw stack bytes. There is instrumentation in the variants AOT generated and in the interpreter. `scripts/inject_hooks.py` replicates the instrumentation in the generation. Stack bytes only represent 24-bit return on public JSL entry; in internal transmitters the return is 16 bits.

Confirmed flows in the ROM and in traces:

- `$80:8773`: positive values lower than `$38` select API `$13`; negative values use API `$03`, with masked parameter by `$3f`.
- `$80:879A`: JSL `$A2:8000`. API `$13` comes to `$A2:84E3` by `$80F2`; API `$03` Enough for `$80A0` (return addresses stacked).
- `$80:87C9`: Passes A to Y and calls API `$14`, coming to `$A2:84FF`. In the gameplay parameters were observed `$02`, `$18`, `$19`, `$1b`.
- `$A2:84B5` also calls the first transmitter, during upload. It is excluded from the deviation MSU by checking the return on the stack.

Detour remains opt-in with `ACKMAN_MSU_EXPERIMENTAL=1` and `ACKMAN_MSU_PREFIX=<prefixo absoluto>`. Files are `<prefixo>-<ID decimal>.pcm`. The prefix itself prevents the mods manager from cleaning the generic variable `SNESRECOMP_MSU1` when no package is selected. The mixer MSU existing feeds the stream SDL3 with the SPC. Missing file, invalid or opening failure keeps the original command. Replacement uses `$3b` in the original protocol, preserving the handshake bit and the accumulator that will be saved in DP+$76. `$3b` it was confirmed as fade, not immediate mute; the PCM now it is loaded in pause and waits for the end of this fade before starting. The full semantics of control/loop It still needs validation. No position restoration PCM in save states; loading a state cancels a pending replacement and interrupts the PCM current.

### Correction of the controls on resumption

Returns `$810E/$811D/$812C/$813B/$8159` have been confirmed in the ROM: carry, respectively, `$3B/$3A/$39/$38/$3F` before you call `$84E3`. Previous filter accepted only `$80A0/$80F2`, leaving these paths untreated MSU. Now every return requires its corresponding operand. Controls not yet implemented interrupt PCM and are forwarded without change to the SPC; this does not implement fades. Provenly inert commands `$35/$38/$39` preserves playback PCM.

`tests/test_audio.c` executes the actual C router with a mocked MSU device. Check the two handshake bit values, accumulator, controls, incorrect operand, upload, SFX, DP incorrect and fallback by missing file. Both CTest suites passed. The combo smoke MSU/widescreen of 3401 frames passed, with cadence of 357368 clocks and latency of CPU of 0..68. Tracking is in `private/timing-controls.csv`. The test PCM is silent.

### Command Table of SPC

`scripts/disasm_spc.py` allows linearly to inspect dumps of 64 KiB, using the opcode notes of the set core. Operands are raw bytes. In `scene3400.spc-ram`, `$0880` consumes pending command, removes bit 7, mask by `$3f`, compares with `$35` and jumps through the table in `$0898`. Bit 6 goes for a playback flag in `$17`; does not belong to the ID PCM. The router now uses IDs `$00..$34` and separates the eleven special commands:

|Command|Destination SPC|Effect observed in the code|
| --- | --- | --- |
|`$35`|`$08EB`|RET|
|`$36`|`$08EC`|Adjust Volumes master/echo DSP|
|`$37`|`$08D6`|Restart protocol IPL|
|`$38/$39`|`$0919`|RET|
|`$3A`|`$0915`|Fade Delta `$66 = +2`|
|`$3B`|`$0911`|Fade Delta `$66 = -9`|
|`$3C/$3D`|`$0923/$091A`|Change flags `$65/$0810`|
|`$3E`|`$092C`|Go to upload|
|`$3F`|`$0935`|Restarts playback status|

In `$0A36`, after reading not zero timer `$FD`, the signed delta of `$66` is added to `$67`, with saturation in 0/255. This proves the gradual fade of `$3B`; the experimental deviation should not be described as immediate silencing. Table and handlers match in initial dump `driver120.spc-ram`; eight bytes of musical pointer operands change in range `$085D..$0A52`. After this correction, build and CTest passed; the smoke MSU of 1000 frames had exit = 0, with substitution of the ID 7 and fallback of IDs 14, 10 and 38 decimals. `private/timing-spc-dispatch.csv` kept cadence 357368 and latency 0..68.

### Wait for the fade before PCM

The musical volume routine in `$0B73` use `$67`; the SFX path on `$0F0E` update separately `$69`. The replacement sends `$3B` by the normal protocol and loads the PCM track without starting playback. The frame observer reads the APU under lock, check the signature of the handler `$0911`, empty port queue, expected acknowledgment, delta `$66 = $F7` and volume `$67 = 0`. Only then does it start PCM, once. Later inert commands update the expected acknowledgment without canceling the track. Reset and change track cancel pending orders. The tests cover these transitions.

The 1,000 frame smoke confirmed `PCM started after SPC music fade` for synthetic track 7. The resolution of this wait is from a frame; it is not a sample-exact exchange, nor does it necessarily eliminate the echo tail or audio already lined up in the device SDL. PCM fades are not yet emulated.

## Video

`$80:AA3F` produces scroll from camera and origins `$0416/$041A`. `$80:AAB9/$80:AB0D` update columns; `$80:ABD0/$80:AC41` They copy words from `$7E:4000` for the queue of VRAM. The map uses 256x256 pixel blocks, 16x16 words per block, with 16 pixel tiles. In the captured phase, the position on the map is camera minus 256 on each axis and the stride is `$2800` Bytes. An offline comparison found equality in 272/272 Visible words.

`ACKMAN_WIDE_EXPERIMENTAL=1` enables filling BG1 in the validated layout (mode `$39`, BG1SC `$50`, stride `$2800`). Up to 64 pixels per side are calculated by the proportion of the window. With each frame, all visible words need to match the map before allowing expansion. Filling is temporary for rendering; VRAM is restored after drawing. BG3 is limited to the native area. Camera, collisions and simulation queue are not dislocated. The previous clamp state of the layers is also preserved and restored, preventing the restriction of BG3 Stay after temporary filling. Different layouts retreat to the native area. Culling of objects on the sides, other phases, HDMA and transitions are not yet approved.

## Timing

Reading AOT of `$4212` artificially advanced the beam in 64 clocks, even with the scheduler already using absolute clocks. The patch `patches/clocked-beam.patch`, applied and verified in the configuration of CMake, turn off this advance only when `SNESRECOMP_CLOCKED_BEAM` It's set. The commit upstream remains fixed by `framework.lock`.

`ACKMAN_TIMING_TRACE=<arquivo.csv>` separates the vblanc event from the instruction border. `python scripts/check_timing.py <arquivo.csv>` requires cadence of 262*1364 = 357368 clocks per event, without subtracting time from CPU to force H=0. The 120 frame test after patch passed, with latency of 0..66 master clocks. This corrects the observed duplication, but does not make the entire core cycle-exact: it uses fixed fields and approximate refresh accounting, besides executing instructions/blocks to legal borders.

## Local evidence

- `private/audio-long.csv`: Tracking to gameplay.
- `private/scene3400.png`: capture the playable phase.
- `private/scene3400.spc-ram`: RAM SPC for further analysis.
- `private/timing-boundaries.csv`: before the correction of `$4212`.
- `private/timing-clocked.csv`: 120 frames after correction.
- CTest: includes regression that rejects shortened cadence in 64 clocks.
- `private/wide3400.png`: 384x224, with an identical 256x224 central cut byte byte to `scene3400.ppm` (zero bytes different).
- `private/wide-restored.ppm`: smoke of 3401 frames after the restoration of the clamp, exit=0. Full image identical to `wide3400.ppm`; central cut again identical to `scene3400.ppm`, with zero different bytes.
- `private/timing-wide.csv`: 3401 frames, exact cadence of 357368 clocks in this model, CPU 0..72 clocks, smoke exit=0.
- Smoke MSU 1000 frames with PCM synthetic silent: a replacement of the ID Decimal 7, fallback of IDs 14, 10 and 38 without file, exit=0. That checks out. routing/opening and continuity, not sound quality.

Build, smoke and these comparisons do not replace a comparison with hardware or a reference emulator or an auditory validation of the actual tracks.


## Update: envelope PCM and border diagn?stic (2026-09-20)

`src/ackman_pcm.c` only involves the mixer MSU before the sum of SPC no stream SDL3. The $3B command keeps the cursor PCM running and reducing gain linearly at zero; $3A increases gain and sends the RET $39 to SPC to not reactivate the original track. Reset and new playback restore unitary gain. An atomic mailbox transfers the commands to the consumer of audio; there is no allocation by callback. The sum is saturated in int16.

The envelope interpolates the steps -9/+2 using the observed timer $60 (12 ms): approximately 340 ms for complete fade-out. This is a approximation by sample, does not synchronize exactly with the DSP: the beginning is applied in the next block of audio, the timer is not dynamically monitored and the driver may have delays when consuming $FD. It still requires comparison between hearing and real music; the PCM In smoke is quiet.

The tests of audio exercise attenuate monotonic, preserves the contribution SPC, arrive at silence, fade-in and satura. The Persistent Instrument `patches/timing-crossing.patch` adds to CSV tho PC and the costs of the instruction that crosses the border. In the execution of 1000 frames, the frame 728 started $A2:81EB two clocks before the edge, spent 30 instruction clocks plus 40 refresh clocks: 68 delay. The cadencia remains 357368. There was no run cycle-exact: the core still snake refresh accumulated and ends entire instructions. Subtracting the counter delay would rule out execution time.

`ACKMAN_VIDEO_TRACE=<arquivo.csv>` log mode, bases BG, stride, camera, origins and acceptance of completion at each frame drawn. There are no other modules of phases already implemented in ackman_video.c. The filling and global clipping NOT were released; lack of coverage of the other phases and bosses with savestates or entry routes. Smoke of 1000 frames does not prove that all phases or edges of objects are correct.

Validate the final: CMake and 2/2 suites CTest passed; smoke of 1000 frames ended with exit=0. timing-crossing-final.csv Confirms 357368 and delay 0..68. video-layouts.csv cont?m 1000 frames, none with accepted map: this stretch is still on the initial screens (stride zero). Therefore this execution does not revalidate widescreen gameplay. The user reported that there are still no savestates or scripts of the other phases.

## Gameplay debug hooks

See DEBUG-GAMEPLAY.md and scripts/play-debug.ps1. This debug built now enables forced viewport extension by default (ACKMAN_WIDE_FORCE=0 restors validation). F5 cycles valid PCM files, F6 fades PCM; both consume host savestatee key bindings. Console logs identify layout and scroll-axis changes, including all three BG scroll shadows. Stage IDs Remain unverified: raw 0400/0401 selectors are explicitly established, not presented as known stage IDs.

## Corrections from 02/10/2026 (concluded in code)

- Logging: Queue RAM for CSV/log/stdout/stderr and audio-buffer.txt, drained by thread SDL every 180 frames; shutdown joins the thread before SDL_Quit. Explicit closing of a file also drains your queue. Abrupt output may lose the last batch; no flush per frame in the producer.
- Aspect: $0400/$0401=00/00 also needs the script pointer $1126. $9447 uses 4:3; $BB63 uses widescreen wrapping. Selector 00/02 He uses wrapping in the story scenes, without injecting strips of the map into the scene.
- Raster: baseline and replay of BG1..BG4 and windows $2123..$2130; shared scroll latch preserved. HDMA does not consume entry in line zero; initial mask and changes of $420C are applied in replay. Patch ackman-raster.patch. raster_scroll_windows test checks BG2 before/after Split line, windows, subscreen, vblank baseline and events HDMA. The visual elimination of the defect in stages/dialogues still needs comparative gameplay.
- New savestates include RtlSaveExecutionState: CpuState real, APU ports, clock anchors, interpreter status and refresh. The absence of RtlApuFrameClock in the execution block (resident v7). Disk loading checks signature ACP1 before changing the game; old saves without registers are refused. The two files sent have been preserved, but cannot serve as trusted Phase 4 replay. Load failure of a new block restores a safety snapshot.
- CMake applies persistent patches of raster, savestatee and savestatee command of the test script. It does not depend on editing generated code.

Validation: build CMake, six tests CTest and 1,000 frame smoke with recording in initial Force Blank, load, new save/load and rejection of legacy slot. In the initial replay, 89/89 frames coincided in PC, clock and delay with the execution prior to the load. No timeout SPC or crash; observed delay maximum 66 master clocks, still instruction border (no cycle-exact). Logs: private/smoke.log, private/final-timing.csv, private/final-video.csv. Slot 11 was used only by the test, without modifying the provided saves.


## Closure of scenes and Release — 03/10/2026

The current rules and evidence are in `WIDESCREEN-SCENES.md`. `ackman_flow.c` intercepts Start only in the final score: fade of 60 frames, restarting of CPU, RtlReset and GameSessionReset. `ackman-reset.patch` protects the core reset without driver SPC high level (the game uses SPC emulated). Asynchronous logging remains in Release; audio keys are now opt-in.


## Menu and saves names

View `UI-SAVESTATES.md`. Slots started using GGA1_save_01.sav up to GGA1_save_10.sav; custom saves remain accessible from the menu.

## Dear ImGui — 04/10/2026

Migrated Menu to Dear ImGui v1.91.9b, commit f5befd2d29e66809cd1110a152e375a7f1981f06, backends SDL3/SDL_Renderer3. Persistence in ackman_menu.c and C++ interface in ackman_menu_imgui.cpp. Fonts, style and viewport accompany DPI. Themes follow SDL_GetSystemTheme. Fullscreen hides the bar; commands run via queue SDL After the drawing.

Release approved: 9/9 CTests; 1000 frames smoke with save/load; 51 saves / 51000 frames in private/scene-imgui-final/results.json. DPI simulated: 100%, 125%, 150% and return to 100%; dark and light themes. Dropdown capture: build-validation/menu-imgui-slots.bmp. updated main executable, SHA256: 83A16597745713061765BBB548FC548EB5D861321971313785953E657B423F9A Previous backup: private/pre-imgui-release/AckmanRecomp.exe.

## Fonts, Alt, and Esc - 04/10/2026

System fonts are queried at src/ackman_font.cppwithout redistribution. Authorized Fallback: assets/fonts/OpenSans-Regular.ttf + OFL.txt. Windows uses NONCLIENTMETRICSW and the font registry; Linux/macOS They have conditional local candidates. The font and rasterized again when changing DPI. Alt only focuses/underlines File; F or Down opens. Esc switches the bar without repeating by key-repeat, including as fullscreen overlay (viewport remains integer). 10 CTests and smoke of 1000 approved frames, including forced fallback. Native validation available only on Windows; Linux/macOS They need testing in their respective environments. Details in docs/UI-SAVESTATES.md. Final regression with Open Sans pitched: 51/51 Saves, 51000 approved frames, results in private/scene-imgui-fonts-final/results.json. Main execution updated after approval; backup in private/pre-font-refinement.

## Candidate Fontconfig/Open Sans - 04/10/2026

Linux: Optional Fontconfig via find_package; FcFontMatch Sans-serif scalar. Library failure or absence goes directly to Open Sans Regular. The source is also incorporated into the binary to tolerate missing assets. No proprietary font is redistributed. Windows/macOS They're preserved. Fixed Down's Open File to navigate instead of reopening dropdown. Alt and Esc covered by the tests; 11/11 CTests and smoke 1000 frames approved. Executable candidate in build-validation; main not replaced. Native Windows validation. Fontconfig/Linux and macOS they still require testing in their respective operating systems.

## Consolidation ImGui / CoreText

Build Release consolidated in root after 11/11 CTests, smoke of 1000 frames with embedded font and return to the end of 51 saves / 51000 frames. Final results: private/scene-imgui-consolidated/results.json. The previous round in private/scene-fontconfig-candidate It's over, too. Approved memory variation measurement; it is not a leak sanitizer.

Submenus play & and display underlined when opening by keyboard, including slots 1..10. Mnemonic keys only activate the current menu. NavEnableKeyboard remains active. Alt/Down/Esc and preserved savestates. macOS use CTFontCreateUIFontForLanguage + kCTFontURLAttribute; no fixed paths. CoreText/CoreFoundation bound conditionally in CMake. Native macOS execution still pending due to unavailability of this environment. Previous executable backup: private/pre-coretext-release/AckmanRecomp.exe.
