# Integrations requiring Ackman reverse engineering

> Historical integration contract. This records the requirements and limitations of the initial scaffold; later implementation reports describe subsequent work.

This document does not claim the features below already work in this game. The host/hardware exists, but Ackman addresses and structures still need identification at this stage.

## Native widescreen and Adaptive View

`src/port_extensions.c` implements width calculation, framebuffer limits, and real `PpuSetExtraSpaceCentered`/`PpuSetExtraSideSpace` calls. Pixel aspect is 7:6: 256×224 becomes 4:3; approximately 342×224 becomes 16:9. Resizing does not change simulation frequency. Logical height remains 224; logical resolution is distinct from physical window output.

`AckmanWorldMargins` starts disconnected, leaving 256×224 with letterbox/pillarbox. There is no fabricated fill, mirroring, or stretching disguised as widescreen. To enable it:

1. Use Mesen 2 to locate camera X, map width, and scene state in WRAM. Watch writes to `$210D–$2114`, VRAM DMA, and OAM tables.
2. Link these PCs to C variants in `program_manifest.json`. Configure M/X/DB/DP and function bounds before replacing routines.
3. Separate simulation camera from presentation camera. Stream tilemaps ahead to the requested bounds and track valid columns.
4. Extend only visual sprite submission. OAM uses wrapped nine-bit coordinates; world-coordinate signs require specific hints. Window resizing must not change spawning, collisions, AI, or RNG.
5. Return only margins with available backgrounds/sprites. Clamp each side independently near map edges.
6. Identify the actual HUD layer. Preserve a central 256×224 logical canvas or use PPU splits if HUD is truly BG3. Do not call `PpuSetWidescreenHudSplit` with guessed coordinates.
7. Register the callback in `InitializePort`, then enable `widescreen_supported`. Titles, menus, fades, and bosses need separate policies and may return zero margins.

Criterion: identical inputs/frame counts produce identical gameplay states at 4:3 and 16:9; central pixels match except for declared visual changes.

## MSU-1 and FMV

The build includes `runner/src/snes/msu1.c`. An explicit prefix can be supplied:

```powershell
$env:SNESRECOMP_MSU1 = 'D:/packs/ackman/ackman'
.\build\Release\AckmanRecomp.exe 'D:/roms/Go Go Ackman (Japan).sfc'
```

Files: `ackman-1.pcm`, `ackman-2.pcm`, `ackman.msu`. PCM starts with `MSU1` and a little-endian uint32 loop point measured in stereo frames, followed by signed 16-bit little-endian stereo at 44,100 Hz. The mixer resamples to the device rate. Read the pinned code: the old MSU1.md description of 735 samples does not describe the continuous mixer. Do not assume 735 samples per 60.0988 Hz NTSC frame.

**An audio pack alone does not replace Ackman's music.** Identify game-to-APU messages and implement command→track, loop, fade, and SPC fallback mapping. Intercept music while preserving SFX. The driver writes track to `$2004/5`, volume to `$2006`, and control to `$2007`; check busy/error flags before suppressing native music. Alternatively, analyze a temporary driver-patched ROM and document the bytes also required on the runtime bus. Compiling a patch without its runtime data/tables is insufficient.

**MSU-1 is not a video decoder.** `$2000–3` seek the data channel; `$2001` reads `.msu` bytes. FMV requires a frame format and a game driver to decompress/copy tiles, palettes, and tilemaps to VRAM within valid DMA windows. Define frame→offset indexing, DMA budget, and guest-clock synchronization rather than using audio callbacks/VSync. An SDL overlay video player is a different feature. No Ackman FMV driver was provided or implemented in this scaffold.

**Savestate gap:** this revision's `msu1.h` has no save/load API. Approval requires a versioned record of track, volume, play/repeat/error, latches, data offset, PCM cursor, loop point, fractional phase, and resampler carry. Never serialize `FILE*`. Store pack identity/hash; reopen/seek after validation. Pause the device, acquire the APU lock, restore, discard queued audio, and prefill again. Test removed/replaced packs between save/load. MSU snapshots remain unqualified until then.

## Mods

CMake enables the `.snesmod` loader and prepares `mods/preloaded`. The exact format is in `external/snesrecomp/docs/MOD_PACKAGES.md`; use the pinned schema rather than invented JSON. `game_id` is `go-go-ackman-jp`; ROM identity is checked before modifications.

The framework loads **data packages** backed by trusted C/C++ implementations statically registered by ID. Translations can use ROM/RAM/VRAM patches described in `RUNTIME_LOCALIZATION.md`. C-function replacements are compiled into the host and activated by packages. The loader does not promise arbitrary DLL or Lua loading.

HD textures require a compositor identifying tiles/palettes/scenes and replacing presentation; placing PNGs in a directory does not replace the PPU. Link the host's `draw_frame` to that compositor with bounded caches, palette invalidation, and native fallback. Store mod selections/hashes in states and reject incompatible combinations. Change mods while the session is stopped. No Ackman texture/translation package was included at this historical stage.

These requirements do not block basic port investigation. They block claims of a final port meeting every requested feature.
