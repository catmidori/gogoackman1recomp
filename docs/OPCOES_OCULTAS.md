# Hidden options in the SNESRecomp runner

Static audit carried out on 09/10/2026 on the local checkout of the framework, revised by `framework.lock`: `e119994bcde03dec95f9ebaeeda305d72a6668b2`, with the specific patches of this project already applied. Comparison reference: `config.ini` in the root and build validated in `build-validation`.

## How to interpret this inventory

A missing file key **can already be accepted by parser**. This is different from an environment variable, an option stored by the mods system and a function that exists, but is not yet connected to the game.

The section names in the column **Menu** indicate where the option would fit into the future interface; do not rename the sections INI existing. For example, the Audio tab currently corresponds to `[Sound]`. Future key names for options without INI are not proposed as if they were already recognized.

Ratings used:

- **INI functional:** reading and operating consumer found in the runner.
- **Functional environment:** existing mechanism without key INI correspondent.
- **Conditional:** depends on backend, build, content or execution path.
- **Overridden/not integrated:** exists in the framework, but does not produce the expected result in this port without additional work.
- **Constant:** implemented behavior, without current configurable control.

This is a code audit, not a fidelity validation of each gameplay option. No changes were made `config.ini`, sources, menu, or executable; did not need to recompile or repeat CTests to generate this documentation.

## 1. Keys already recognized, absent from our config.ini

### Graphics

|Internal name|Menu/key already accepted|Type, values, and local default|Impact and situation|
|---|---|---|---|
|`g_config.linear_filtering`|Graphics; `[Graphics] LinearFiltering`|Boolean / checkbox; default `false`|It uses linear interpolation to enlarge texture rather than nearest-neighbor. Smoothes pixels; it is not an audio filter. Functional in SDL and OpenGL; a shader can specify another filter per pass. `[Video] LinearFilter` is compatibility aliases. [C1, V1, V2, S1]|
|`g_config.frame_blend`, `g_blend`|Graphics; `[Graphics] FrameBlend`|Boolean / checkbox; default `false`|Temporally blending frames, softening alternating flicker and simulating translucency. Uses separate history and treatment for presentations repeated/interpolated; consumes additional memory/work. Functional, with deactivation in case of allocation failure. `[Video] FrameBlend` It is also accepted. [C1, V1]|
|`g_config.renderer`|Graphics; `[Graphics] Renderer`|String / dropdown; empty default follows `OutputMethod`; `auto`, `opengl`, `software` and listed drivers|Select the native presenter OpenGL or the driver SDL available, for example `direct3d11`, `direct3d12`, `vulkan`, as the machine. It takes precedence over `OutputMethod` when specified. Unavailable driver returns to Auto. Do not confuse with `NewRenderer`, which chooses the implementation of the PPU. Choice applied in the creation of the presenter. **Conditional:** the HUD current is integrated to the SDL_Renderer; choose OpenGL requires validate/adapt the integration of the menu and blur. [C1, V1, A1]|
|`g_config.shader`|Graphics; `[Graphics] Shader`|String/file selector; empty/default`NULL`|Loads shader GLSL or preset supported by loader OpenGL. Can implement CRT, scanlines and other effects through an appropriate shader. SDL issues warning and does not apply shaders. No native reload command in F11. Selection is done on startup; it is not a real-time shader editor. [C1, V1, V2, S1]|

### Controls and Keybinds

|Internal name|Menu/key already accepted|Type, values, and local default|Impact and situation|
|---|---|---|---|
|`g_config.enable_gamepad[0]`, `[1]`|Controls; `[Keybinds.Player1] EnableGamepad`, `[Keybinds.Player2] EnableGamepad`; aliases `[GamepadMap] EnableGamepad1/2`|Booleans / checkboxes; both `true` default|Allows the opening/assignment of gamepads of the two slots of the host. Do not create multiplayer support in the game: `kGameHost.num_players = 1`. Change during execution would need to coordinate opening/closing of devices. [C1, I1, A1]|
|`g_config.player_src[0]`, `[1]`, `has_keyboard_controls` derivative|Controls; `[Keybinds.PlayerN] Source`; aliases `[Controller] SourceP1/SourceP2`|Integer / dropdown: `0` none, `1` keyboard, `2` gamepad; P1= standards`1`, P2=`0`|Each player's keyboard polling depends on `Source == 1`. `has_keyboard_controls` is calculated from these values and is not independent key. **Nuance:** the gamepad is also controlled by `enable_gamepad`; Choosing Source does not alone equal to shutting down all gamepad input. [C1, I1]|
|`g_config.gamepad_deadzone`|Controls; `[Keybinds.PlayerN] GamepadDeadzone`; aliases `[GamepadMap] GamepadDeadzone`|Integer / slider; axle drives SDL, `0..32767`; standard `3277`, about 10%|Sets the dead radial zone of the left analog before converting it into directions SNES. There's **a shared variable**, even if the parser accepts the key in the two subsections: the last reading wins, it is not two independent dead zones. The legacy alias migrates exactly `10000` for `3277`; the new subsections have range validation. [C1, I1]|
|`g_config.rewind_gesture`, `g_rewind_gesture_pad`, `g_rewind_gesture_raw`|Controls/Keybinds; `[Controller] RewindGesture`|String / combination editor; empty uses `Select+R3`; `none` disables|Open the rewind by combination of buttons. Accept Names SNES and `L3/R3`, separated by `+`; Invalid combination or with less than two buttons returns to default. Do not confuse with `[KeyMap] Rewind`, already exposed to keyboard. [C1, I1]|
|`joymap_first`, `joymap_ents`, `GamepadMapEnt.modifiers/cmd`|Keybinds; `[GamepadMap] Controls`, `ControlsP2` and host action names|String lists; physical buttons and combinations|Map the 12 buttons SNES of each gamepad and system actions. `ControlsP2` select the second player. Accepted names include `A/B/X/Y`, `Back`, `Start`, `Guide`, `L1/R1` or `Lb/Rb`, `L2/R2`, `L3/R3` and `DpadUp/Down/Left/Right`. Lists use commas; button modifiers use `+`. Matching prioritizes combinations with more modifiers. That. **no** restores redundant `[KeyMap] Controls`, ignored on port. [C1, I1]|

### Initialisation and indicators

|Internal name|Menu/key already accepted|Type, values, and local default|Impact and situation|
|---|---|---|---|
|`g_config.display_perf_title`|Graphics; `[General] DisplayPerfInTitle`|Boolean / checkbox; default `false`|Despite the historical name, the current consumer calls `snes_osd_set_fps_visible(1)` on startup. Sets **FPS visible on boot**Instead of a new independent indicator in the titlebar. It is the preference of the initial state that is missing in the file; `[KeyMap] ToggleFPS = f` is a shortcut, and the current checkbox controls the state in memory. [C1, V1, O1]|
|`g_config.skip_launcher`|File/General; `[General] SkipLauncher`|Boolean / checkbox; default `false`|Skip the launcher when this frontend is compiled and the boot would normally open it. `--launcher` forces the opening; `--no-launcher` forces skipping it. **Conditional:** does not enable an absent binary launcher. [C1, V1]|

## 2. Functional features accessible by environment, without key INI

### Rewind and local history

|Internal name|Current Menu/Input|Type, values, and default|Impact|
|---|---|---|---|
|`s_enabled` in `snes_rewind.c`|File/General; `SNESRECOMP_REWIND`|Boolean / conceptual checkbox; enabled by default; value starting with `0` disables|Disables capture/history, saving snapshots and thumbnails. The Shortcut `[KeyMap] Rewind` just open the UI And it doesn't replace that option. Settings read once; no setter INI Real-time activation. [R1]|
|`s_depth`|File/General; `SNESRECOMP_REWIND_DEPTH`|Integer / slider; `4..240`, standard `60` snapshots|Number of states in the ring. Increases history and memory duration in proportion to the actual snapshot size, plus thumbnails. Values are limited to the range. [R1]|
|`s_interval`|File/General; `SNESRECOMP_REWIND_INTERVAL`|Integer / slider; `1..60`, standard `6` frames|Intermission between captures. Lower: more precision and more capture cost; higher: longer history with wider jumps. With the defaults, the module estimate is approximately 6 seconds at 60 Hz; it is not a buffer configured directly in seconds. Rewind is blocked during netplay session. [R1]|

### Audio

|Internal name|Current Menu/Input|Type, values, and default|Impact|
|---|---|---|---|
|`DspShadow.enabled`, `Dsp.shadow`|Audio; `SNESRECOMP_AUDIO_SHADOW`|Boolean / conceptual checkbox; disabled when absent or exactly `0`|Enable a parallel mixer with Cubic Interpolation Catmull-Rom in float instead of interpolation Gaussian interpolation/truncation of the S-DSP. Replacement occurs **only after approval by the verifier**, with return to canonical audio in disagreement. It is not a universal selector of Gaussian/linear/cubic and does not automatically improve files PCM of the Ackman layer. Shadow creation occurs on startup of DSP. [AU1, AU2]|
|`g.armed`, `g.base`, `g.auto_base` in `msu1.c`|Audio/Mods; `SNESRECOMP_MSU1`|String / choice of mode/path; absent turns off; `1`, `on`, `auto`, `true` self-detect base; another value is path/base|Activates peripheral MSU-1 and its files data/PCM. **Conditional to content:** If you have an armed peripheral, it does not convert the ROM original in a ROM sending commands MSU-1 Our descriptor does not announce MSU-1 no launcher (`msu1_supported=0`); the layer PCM Ackman's specific is another mechanism. Mod packages can provide features MSU-1. [AU3, M1, A1]|

### Graphics and capture tools

|Internal name|Current Menu/Input|Type and default|Impact|
|---|---|---|---|
|`g_snes_ppu_dbg_layer_mask`|Advanced/diagnostic graphics; `SNESRECOMP_LAYER_MASK`|Integer / set of checkboxes; mask with autodetected base, pattern `0xff`|Isolate layers: bits 0–3=BG1–BG4, bit 4=OBJ. Conceptual example: `0x11` Keeps BG1 and sprites. Presentation filter does not modify emulated state. Initial reading is curly. [P1]|
|Initial State `snes_osd_set_fps_visible`|Graphics; `SNESRECOMP_OSD_FPS`|Numeric boolean; absent does not force activation|Connect the OSD of FPS on the boot. It is an alternative to the functional key `DisplayPerfInTitle`Not a second counter. `SNESRECOMP_FPS` in `common_rtl.c` It's diagnostic heartbeat on the log and not this OSD. [V1, O1]|
|`shot_frame`, `shot_done`; path of `WritePpm`|File/Graphics advanced; `SNESRECOMP_SCREENSHOT`, `SNESRECOMP_SCREENSHOT_FRAME`|Output String + frame integer|Diagnostic capture of compound buffer in PPM in the selected frame. It is a different mechanism from the Screenshot shortcut, whose native path requires OpenGL. It's not a permanent graphic preference. [V1]|
|`dir`, `from`, `to`, `log_path`; presentation log|File/Graphics advanced; `SNESRECOMP_SCREENSHOT_DIR`, `SNESRECOMP_SCREENSHOT_FROM/TO`, `SNESRECOMP_PRESENT_LOG`|Directory/log + integer path|Series of presentations and metadata to examine interpolation/pacing. No key INI. Do not confuse with user-ready video recorder. [V1]|
|`dump_path`, `dump_frame`, `dump_dir`, `dump_start/end/step` in `widescreen.c`|Diagnostic graphics; `SNESRECOMP_FRAME_BMP`, `SNESRECOMP_FRAME_BMP_FRAME`, `SNESRECOMP_FRAME_BMP_DIR`, `SNESRECOMP_FRAME_BMP_START/END/STEP`|Paths + integers|Dump frames by widescreen path, useful for raster inspection and regressions. Availability depends on going through the corresponding composer. [P2]|

### Advanced controls and emulation

|Internal name|Current Menu/Input|Type and default|Impact/condition|
|---|---|---|---|
|Multitap configuration in `RtlApplyMultitapEnv`|Controls; `SNES_MULTITAP`|String/dropdown: `port1`, `port2`, `both`, `off`|Set multitap accessories before the first frame. **Do not add multiplayer to Go Go Ackman**, nor creates bindings of P3–P8 in our INI. Relevant appeal to others ROMs/framework. [E2]|
|`SnesrecompExecutionMode mode`|General/advanced; `SNESRECOMP_EXECUTION_MODE`|Enum/string: `off`, `on`, `force`, `verify`, `auto`; current build `off`; aliases `lle/hle`|Optimized replacement policy: `off` Uses faithful floor; `on` allows optimizations/fallback; `force` considers fallback to fail; `verify` verifies preserving the publication of the floor; `auto` applies automatic policy. **It does not guarantee performance gain:** no optimization registration call was found on port; the framework warns when there are no registered substitutions. This policy is not a magic selector of CPU. [E1]|
|Override `floor_env`|General/advanced; `SNESRECOMP_FORCE_FLOOR`|Boolean for non-empty string that doesn't start with `0`|It takes precedence over the above policy and forces `off`, useful for reproducing differences. Solves once per process. [E1]|
|`lle_yield_bounce_enabled`, `s_scheduler_aot_policy`|General/diagnostic; `SNESRECOMP_LLE_BOUNCE`|Boolean; macro default, fallback at source `1`|Allows bounce for routines AOT in the interpreted scheduler. Explicit setter of scheduler policy takes precedence; it only takes effect on that path. Do not confuse with registered substitutions of `ExecutionMode`. [E3]|
|`s_target`, `s_interp_file_targets`|General/diagnostic; `SNESRECOMP_LLE_INTERP_TARGET`, `SNESRECOMP_LLE_INTERP_TARGET_FILE`|PC24 hexadecimal / path with targets|Delete specific bounce routines AOT for differential investigation, performing them by the interpreter. No user menu or key INI. [E3]|
|`s_t` in `bridge_bounce_flush_thresh`|Audio/diagnostic; `SNESRECOMP_LLE_APU_FLUSH_THRESH`|Non-negative integer, accepts autodetected base; default **1024 in code**|Threshold of outstanding master cycles for flush from APU before bounce; `0` forces a flush on every pass. Old comment mentions 4096, but the executed expression uses 1024. Pertinent only to the bridge LLE. [E3]|
|`v` in `interp_step_cap`|General/diagnostic; `SNESRECOMP_INTERP_STEP_CAP`|Integer; minimum `1000`, standard `2000000` steps|Limits routine interpreted without termination, avoiding indefinitely blocking the host. It is execution protection, not limit of FPS. [E3]|
|`s_touch_scale`|Audio/diagnostic; `SNESRECOMP_APU_TOUCH_CYCLES`|Positive real number; default equivalent to `256`|Scale synthetic credit of cycles per touch on APU ports on the way to recompilation that uses this estimate. Can change progress/handshakes; paths with self-pacing may not use it. It is not sound quality control. [E2]|
|Hardware waiting fast-forward|General/diagnostic; `SNESRECOMP_NO_D9FF`, `SNESRECOMP_NO_POLLGATE`, `SNESRECOMP_NO_VBLANK_FF`|Environment deactivation overrides|Turn off specialized shortcuts from the interpreter to waits/polling/VBlank. Useful in A/B precision; effect restricted to the path containing each optimization. These are not recommended options for the Graphics tab. [E3]|

## 3. Shaders: preset, not preset parameters config.ini

The loader supports parameters and multiple passes. Select `[Graphics] Shader` points to this content; the lines below belong to the preset/shader, **no** They saw keys. INI from the game automatically. Everyone demands it. OpenGL. Source: [S1].

|Internal name / preset key|Menu|Type|Impact|
|---|---|---|---|
|`GlslPass.filter`; `filter_linearN`|Graphics/Shaders|Boolean|Nearest or linear per pass; it can prevail over global filtering.|
|`GlslPass.scale_type_x/y`; `scale_typeN`, `scale_type_xN/yN`|Graphics/Shaders|Enum/string|Sets framebuffer pass size reference as source/viewport/absolute.|
|`GlslPass.scale_x/y`; `scaleN`, `scale_xN/yN`|Graphics/Shaders|Real number|Scale or dimension of the framebuffer of each pass, according to the above mode.|
|`GlslPass.wrap_mode`; `wrap_modeN`|Graphics/Shaders|Enum/string|Off-limit sampling mode: affects edges and texture repetition.|
|`GlslPass.mipmap_input`; `mipmap_inputN`|Graphics/Shaders|Boolean|Enables mipmaps for pass input texture.|
|`GlslPass.float_framebuffer`, `srgb_framebuffer`; their keys with suffix N|Graphics/Shaders|Booleans|Selects framebuffer representation/transfer, affecting accuracy and color treatment.|
|`GlslPass.frame_count_mod`; `frame_count_modN`|Graphics/Shaders|Integer|Set frame counter modulus provided to pass, useful for periodic animation.|
|`GlslParam.value`; `parameters`, IDs and `#pragma parameter`|Graphics/Shaders|Real number / slider; limits/default defined by shader|Adjusts uniforms, for example effect intensity when the shader offers this parameter. The loader consumes them, but there is no editor in the HUD current or API persistence in our INI. .|
|`GlslTexture`; `textures` and attributes of LUTs/textures external|Graphics/Shaders|List of IDs/paths and attributes|Provides auxiliaries textures to the shader. It is not equivalent to LUT the color of the core described in section 5.|

## 4. Mods: functional options with separate storage

The mods runtime is switched on (`SNESRECOMP_ENABLE_MODS=ON`) and initialized by the host. The schema is dynamic, defined by package manifests: there is no universal list of options for each mod to copy to `[Mods]`.

|Internal name|Menu|Type|Current Impact / Mechanism|
|---|---|---|---|
|`PackageSelection.version`|Mods|String/dropdown|Selects installed package version. The plan is validated against the identity of ROM and package requirements.|
|`FeatureSelection.enabled`, `has_enabled`|Mods|Boolean / checkbox|Enables package feature; initial value comes from `Feature.default_enabled` when there is no explicit selection. Effective activation occurs after resolution/commit and preparation of plugins.|
|`FeatureSelection.values`, `Option.type`, `min_value/max_value/step`, `choices`|Mods|Boolean, choice/string or integer|Values of the options of a feature, with type and track declared in the manifest. They only affect behavior when the plugin/resource Consumer uses them. They're not global core parameters.|
|`FeatureSelection.resources`|Mods|String/file selector|Selects features such as audio/data pack, with validation of identity/format when declared by the package.|
|Runtime Allowlist, `snes_mod_runtime_set_cosmetic_allow_c`|Mods/Netplay advanced|Textlist|Sets authorized cosmetic exceptions for peer comparison. It is an integration policy; to declare `presentation_only` in a manifest alone does not give a right package to ignore the comparison. Conditional to netplay and plugin registered as presentation.|

Sources: [M1]. Selections are persisted in `<exe>/mods/state.toml`, not in `config.ini`. Mods selection and unification of game configuration files are distinct contracts. A future tab should use the provider/runtime And their validations, without inventing a second competing source of state. Many changes require new commit/restart can't be treated like live toggles.

## 5. Existing options that we should not announce as ready on Ackman

|Existing internal name/input|Menu|Type|Evidence and current limit|
|---|---|---|---|
|`g_config.display_aspect`; `[Graphics] DisplayAspect`|Graphics|Enum: `4:3`/`CRT`/`0`, `8:7`/`SquarePixels`/`1`, `1:1`/`SquareFrame`/`2`; default 4:3|Parser and native presenters use the option, but `AckmanVideoComputeViewport` overwrites the viewport and fixes the interpretation CRT 4:3, with its own adaptive expansion. Just add the key does not implement new aspect modes in this port. [C1, V1, V2, A1]|
|`g_config.widescreen`; `[Graphics] Widescreen`|Graphics|Boolean; default `false`|Field used by other Launcher ports and callbacks. Ackman uses `IgnoreAspectRatio`, `AckmanVideoSetWidescreen` And his hook in size. `widescreen_supported=0` in the descriptor. Do not expose as a second independent toggle. [C1, A1]|
|`g_active`, `g_lut`; `SNESRECOMP_SCREEN`, `snes_color_lut_setup_kind`|Graphics|Dropdown: `raw`, `crt`, `composite`, `trinitron`|Transformations BGR555 are implemented in `color_lut.c`, but the search for calls did not find setup/map in the pipeline of runner/port. The library is latent infrastructure: **the environment variable alone does not prove visual effect on this executable**. CRT here is color transformation, not full shader with geometry/scanlines. [P3]|
|`PpuWsExtraOverride`; `SNESRECOMP_WS_EXTRA`|Advanced Graphics|Integer; ceiling `272`, absence returns `-1`|Helper exists, but no consumers were found in runner/port audited. Ackman limits his own adaptive margin to 64 per side. Do not offer this override as operational width control. [P1, A1]|
|`g_config.enable_snes9x_oracle`; `[General] EnableSnes9xOracle`|General/diagnostic|Boolean; parser default `true`|Initialisation protected by `ENABLE_ORACLE_BACKEND`, absent from the audited build. Requires binary prepared for oracle. It does not automatically improve the accuracy of a normal build; it serves to compare, with limitations in savestatee. [C1, V1]|
|`g_config.netplay_player_name`; `[Netplay] PlayerName`|Controls/Netplay|String, 64-byte field|Recognized configuration and identity also treated by the specific module. Netplay is turned off on build (`SNESRECOMP_ENABLE_NET=OFF`), therefore does not represent an active lobby function in this executable. [C1, N1]|
|`[Controller] GuidP1/GuidP2`, `DeadzoneP1/DeadzoneP2`; `[Controller.<guid>]`|Controls|Strings of GUID, integer and profile|Parser silently accepts fields/profiles Launcher, but the host game path assigns controls by index/availability and uses a global deadzone. No consumption of these profiles was found by the Ackman input; they would be integration of launcher/device, non-remapping already functional in HUD. [C1, I1]|
|`[Features]`|Mods|Reserved section|Recognised section, but no functional key handler in the current parser. It does not automatically contain features of mod packages. [C1, M1]|

### Netplay: effective built-only controls with network

These controls are implemented, but **are not active in the current candidate**. Source: [N1]. They should not be counted as ready local emulation options.

|Internal name / environment|Menu|Type|Impact|
|---|---|---|---|
|`SnesNetplayConfig.enabled`; `SNES_NETPLAY`|Controls/Netplay|Boolean|Enables session when network support is compiled.|
|`local_slot`, `slot_count`, `input_player`; `SNES_NET_SLOT`, `SNES_NET_SLOTS`, `SNES_NET_INPUT_PLAYER`|Controls/Netplay|Integer|Assigns peer/slots and source of local input; it is not amount of players supported by ROM. .|
|`input_delay`; `SNES_NET_DELAY`|Controls/Netplay|Integer frame count; default 2 in the module's initial config|Adds input delay to sync.|
|`rollback`, `input_prediction`; `SNES_NET_MODE`, `SNES_RB_PREDICTION`|Controls/Netplay|Enum (`rollback`/`rb` versus delay) and integer|Selects synchronization and prediction mode; it also depends on rollback support on build.|
|`session_id`, `bind_hostport`, `peer_hostport`; `SNES_NET_SESSION_ID`, `SNES_NET_BIND`, `SNES_NET_PEER`|Controls/Netplay|Integer and strings host:port|Identifies session and transport endpoints.|
|`transport`; `SNES_NET_TRANSPORT`|Controls/Netplay|Enum/string `ice` or `lan`|Chooses transport when available; ICE depends on build/additional dependencies nesses.|
|URL lobbying, version; `SNES_NET_LOBBY_URL`, `SNES_NET_GAME_VERSION`|Controls/Netplay|Strings|Endpoint and identity used by lobby client.|
|`SNES_NET_TURN_HOST/USER/PASS`, `SNES_NET_STUN_HOST`, `SNES_NET_FORCE_TURN`|Controls/Netplay|Strings and Boolean|Crossing configuration NAT/relay, only with infrastructure ICE/TURN enabled. They're network settings, not emulation sliders.|

## 6. Buffers, interpolation and boundaries that are constant, not hidden options ready

|Internal name|Potential Menu|Current type|Behavior / consequence of future parameterization|
|---|---|---|---|
|`HOST_AUDIO_PREFILL`|Advanced Audio|Fixed Macro `2136u` samples|Audio prefill before consuming the ring. Change affects latency and stability, but there is no reader INI/environment to that value. [AU4]|
|`HOST_AUDIO_HIGH_WATER`|Advanced Audio|Fixed Macro `4096u` samples|Producer backpressure to avoid overflow; wait with consumer protection stopped. It is not the `AudioSamples` already exposed. [AU4]|
|`DSP_SAMPLE_RING`|Advanced Audio|Fixed Macro `8192` stereo frames|Ring capacity PCM Do DSP. It is incorporated into structures/routines ring and snapshots; requires coordinated code change, not just a slider. [AU2]|
|`g_audio_stream_buffer_size`|Audio|`size_t`, automatic growth|Scratch buffer SDL3 it grows according to the demand of the stream. It is neither configurable elastic buffer nor optional stretching algorithm. [AU4]|
|`gaussValues`, `dsp_getSample`, ring resampler|Audio|Algorithms/table fixed|Gaussian voice interpolation and output resampling are implemented. No generic key `Interpolation`, `AudioFilter` or `TimeStretch` in parser; shadow is the opt-in enhancement found. [AU1, AU2]|
|`g_simulation_hz`, presentation clock, `SNES_HOST_NTSC_HZ`|Graphics/General|Calculated and constant frequency NTSC|The host separates simulation and presentation as refresh. Could not close temporary folder: %s `MaxFPS`, `FrameLimit`, `TurboSpeed` or selector PAL configurable in INI. `DisableFrameDelay` and VSync these controls are already present; diagnosis `RUN_FRAMES` is output condition, not limiting FPS. [V1]|

## 7. Diagnostics found, separated from user preferences

The search also found numerous instrumentation flags. Not missing options of Graphics/Audio for normal use, although they are useful for reverse engineering. Each family below preserves name, type and effect instead of suggesting a product checkbox for each trace point.

|Parameter/internal family or environment|Logic section|Type|Impact/restriction|
|---|---|---|---|
|`SNESRECOMP_RUN_FRAMES`, `HOST_PROFILE`, `HOST_PROFILE_START_FRAME`, `STATE_TRACE` via `HostGetenv`|General/diagnostic|Whole, boolean, way|Close after N frames, measure host phases and emit state trace. `HostGetenv` add prefix `SNESRECOMP_`; are not new gameplay preferences.|
|`SNESRECOMP_DEBUG_PORT`; `--paused`, `--script`, `--lua`, `--framedump`|General/diagnostic|Port, boolean, paths|Debug server, input script and dumps; Lua depends on `SNESRECOMP_ENABLE_LUA`, currently OFF. .|
|`SNESRECOMP_WRAM_TRACE_FILE`, `APURAM_TRACE_FILE`, `DSPREG_TRACE_FILE`, `DSPOUT`|Audio/diagnostic|Paths|Memory trace/DSP and audio dump, with cost of I/O.|
|`SNESRECOMP_WLOG*`, `WRITE_WATCH*`, `WRAM_WATCH*`, `REG_TRACE`, `SPC_TRACE`, `SPC_PORT_TRACE`, `RAMWRITE_TRACE`, `DMA_DEBUG`|General/diagnostic|Flags, addresses/tracks, boundaries, paths|Execution observation/writing; some readers are protected by diagnostic macros. They do not change graphic quality as filters.|
|`SNESRECOMP_CPU_TRACE_RING_ENTRIES`, `BOUNDARY_RING_ENTRIES`, `VRAM_RING_ENTRIES`, `GM14_TRACE_ENTRIES`|General/diagnostic|Integer|Capacity of forensic history, conditioned to the compiled trace code.|
|`SNESRECOMP_PHASE_MS`, `AOT_PROF*`, `INTERP_*PROFILE*`, `INTERP_OPCODE_HIST`|General/diagnostic|Flags/integers|Profiling, opcode count and timing; they are not accelerated game modes.|
|`SNES_COSIM_*` (stride, port, audit, shared APU, audio, drawing and accuracy APU)|General/diagnostic|Flags/integers|Comparison between instances/oracles, conditioned to the build of co-simulation. Do not infer that `ACCURATE_APU` works as a sound preference in normal build.|
|`SNESRECOMP_TIER2_CAPTURE`, `TIER2`, `TIER2_JOURNAL`, `TIER2_MANIFEST`, `TIER2_VERBOSE`|General/diagnostic|Flag/paths|Capture/journal Tier2, subject to the conditions of compilation and execution of the subsystem.|
|`SNESRECOMP_TRAP_*`, `CRASH_TEST`, `FREEZE_AT_FRAME`, `BOOT_WATCHDOG_SECS`, `POSTMORTEM_TRACE_N`|General/diagnostic|Flags/integers/PCs|Deliberate disruptions and failure reports. Boot watchdog requires the corresponding diagnosis in build.|
|`SNESRECOMP_OVERLAY_SELFTEST*`, `OVERLAY_DUMP`, `REWIND_DUMP`, `VOLUME_DEMO`, `KEYMAP_DUMP`|Diagnosis of UI/input|Flags/integers/paths|Overlay autoexercise/dump and volume demonstration; they are not persistent visual settings.|
|`SNESRECOMP_DSP1_ROM`, `SNESRECOMP_CX4_ROM`, `SNESRECOMP_MMC_MAP`|General/cartridge diagnostics ostic|Paths/override|Data/behavior specific coprocessors and mapping. Go Go Ackman does not come to possess DSP-1/Cx4/S-DD1 by configuring these variables.|

There are also backend variables of its own SDL, how `SDL_VIDEODRIVER`, `SDL_RENDER_DRIVER` and `SDL_AUDIO_DRIVER`; belong to the platform, not to the emulation parser. In this project, variables `ACKMAN_*` tests, trace and experimental gates are port-specific and were separated from the native inventory.

## 8. Duplications that should not become new options

- `RunAhead` already exists in `[General]`; `[Emulation] RunAhead` and `SNESRECOMP_RUNAHEAD` are alternative entries of the same resource.
- `ToggleFPS`, `Mute`, `Rewind`, `SaveStateMenu` and all listed host commands already have shortcuts in the file. `DisplayPerf`, `WindowBigger` and `WindowSmaller` are aliases of `ToggleFPS`, `ScaleUp` and `ScaleDown`.
- `Controls`/`ControlsP2` in `[KeyMap]` are ignored by the Ackman adaptation; keyboard gameplay belongs to `[Keybinds.PlayerN]`. This does not apply to gamepad functional lists in `[GamepadMap]`.
- `Mute` it is action/state of runtime and volume is already persisted in `[Sound] Volume`; no key found `[Sound] Mute` on the parser.
- `VSync`, `NewRenderer`, `NoSpriteLimits`, `WindowSize`, `WindowScale`, `Fullscreen`, `IgnoreAspectRatio`, `Autosave`, `DisableFrameDelay`, `EnableAudio`, `Volume`, `AudioFreq`, `AudioChannels` and `AudioSamples` They're already there. Variants of value such as `WindowSize = Auto` and `OutputMethod = SDL-Software/OpenGL`They're not missing keys.

## 9. Priority for future tabs

1. **Graphics:** LinearFiltering and FrameBlend; preference of FPS on boot; renderer/shader with validation of integration HUD/OpenGL.
2. **Controls/Keybinds:** EnableGamepad, Source, shared deadzone, gamepad combinations and rewind gesture.
3. **File/General:** rewind activation, depth and range, with a safe ring reconfiguration contract before offering live editing.
4. **Audio:** shadow checked as advanced option; save distinction between DSP, PCM Ackman specific and MSU-1
5. **Mods:** mount UI by the package scheme and existing provider, keeping explicit the persistence in `mods/state.toml`.

DisplayAspect, LUTs of the core, profiles/GUID control and internal buffers require integration before being presented as ready options. Netplay, oracle and Lua need their respective builds. None of these proposals have been implemented in this audit.

## Local sources and entry points

The codes in brackets above refer to these files. The evidence is local implementation, including patches, not just comments from headers.

|Code|File and relevant points|
|---|---|
|C1|`external/snesrecomp/runner/src/desktop/config.h`; `desktop/mmx_config.c`: `GetIniSection`, `HandleIniConfig`, `ParseConfigFile`, `ParseGamepadArray`, `WriteConfigFile` (approx. lines 277–665).|
|V1|`external/snesrecomp/runner/src/desktop/host_main.c`: `RendererChoice/Apply` (455–481), composition (1012–1201), `SdlRenderer_Init/EndDraw` (1894–1966), launcher (2359–2535), oracle (2762), rewind/OSD (2959–2964).|
|V2|`external/snesrecomp/runner/src/desktop/opengl.c`: creation of shader (219–220), viewport/filter (341–363).|
|S1|`external/snesrecomp/runner/src/desktop/glsl_shader.c`: preset (154–206), parameters and pass application (527–588).|
|I1|`external/snesrecomp/runner/src/desktop/host_main.c`: gesture (1360–1410), polling by Source (3169–3170), pad opening (3570–3625), dead zone (3730); `runner/src/keybinds.c/h`. .|
|O1|`external/snesrecomp/runner/src/snes_osd.c/h`: `snes_osd_set_fps_visible`, `snes_osd_fps_visible`. .|
|R1|`external/snesrecomp/runner/src/snes_rewind.c/h`: standards, configure (34–108), capture/blocking netplay and opening (170–215).|
|AU1|`external/snesrecomp/runner/src/snes/dsp_shadow.c/h`: creation by environment (11–27), cubic mixer and gate; `snes/audio_shadow.c/h`: verification/calibration.|
|AU2|`external/snesrecomp/runner/src/snes/dsp.c/h`: creation/processing of shadow (70/187), Gaussian (444–448), ring/resampling.|
|AU3|`external/snesrecomp/runner/src/snes/msu1.c/h`: `msu1_init` (184–204) and resolution of resources.|
|AU4|`external/snesrecomp/runner/src/desktop/host_main.c`: audio, macros/buffers (1733–1867), device creation (approx. 2800–2870).|
|P1|`external/snesrecomp/runner/src/snes/ppu.c/h`: layer mask (151–162), `PpuWsExtraOverride` (265–275).|
|P2|`external/snesrecomp/runner/src/widescreen.c`: frame dump readers (51–59).|
|P3|`external/snesrecomp/runner/src/snes/color_lut.c/h`: Selection/setup/map (146–192); absence of call sites on the audited pipeline.|
|E1|`external/snesrecomp/runner/src/execution_mode.c/h`: vocabulary, registration and policy resolution.|
|E2|`external/snesrecomp/runner/src/common_rtl.c`: multitap (662–683), credit APU (1645–1705).|
|E3|`external/snesrecomp/runner/src/snes/interp_bridge.c`: flush (49–66), target exclusions, bounce/step Cap (780–843), waits/VBlank (1091–1312).|
|M1|`external/snesrecomp/runner/src/mod_runtime.cpp/h`: `OptionType/Option`, `FeatureSelection/PackageSelection`, validation, persistence `load_state/save_state` and provide.|
|N1|`external/snesrecomp/runner/src/netplay/snes_netplay.c/h`, `snes_netplay_identity.c/h`, `runner/src/lobby/snes_lobby_client.c`: config/env and identity.|
|A1|`src/main.c`: `kGameHost`, callbacks/capabilities; `src/ackman_video.c`: `AckmanVideoComputeViewport`, `AckmanVideoViewportExtra`; `src/ackman_menu_imgui.cpp`: backend SDL_Renderer Do HUD. .|

Build conditions granted on `build-validation/CMakeCache.txt` and runner sources: SDL3, mods ON, netplay OFF, Moon OFF, execution default `off`, main diagnoses OFF and absence of `ENABLE_ORACLE_BACKEND`/`RECOMP_LAUNCHER` in the flags of the current target. This state should be reassessed when build configuration changes.
