# Validation protocol — approval requires evidence

## Reproducible preparation

1. Record ROM SHA, framework commit, compiler/flags, SDL3, Windows version, CPU/GPU, audio rate, and monitor. Disable mods, widescreen, rewind, run-ahead, and turbo for the baseline; retain native sprite limits.
2. Separate save profiles/directories. Use identical initial states and per-frame input rather than synchronizing tests by wall-clock seconds.
3. Use Mesen 2 with its debugger or bsnes as an independent reference. SNESRecomp LLE helps detect AOT differences but shares hardware implementation; it is not an independent PPU/APU/timing oracle.

## A. Build and identity

```powershell
python tests/test_tools.py
python scripts/regen.py --rom 'R:/ChatGPT/Go Go Ackman (Japan).sfc' --verify-only
ctest --test-dir build-mingw --output-on-failure
```

Test missing, empty, truncated, alternate-region/revision, and one-byte-modified ROMs, plus a private copy with a 512-byte header. Only the headered equivalent should normalize to the same identity; mismatches must be rejected before SnesInit. Use copies, never the original ROM. Test spaces/Unicode through the Windows loader rather than assuming support.

Audit dependencies with `dumpbin /dependents` (MSVC) or `objdump -p` (MinGW). Verify no SDL2 and correct resolution of required DLLs. Run on a clean Windows VM without the source tree. Inspect PE resources and ZIP contents. Searching for the full ROM cannot rule out partial embedded assets; review sources, resources, and packaging rules.

## B. Boot, gameplay, and AOT coverage

```powershell
$env:SNESRECOMP_RUN_FRAMES = '600'
$env:SNESRECOMP_TIER2_CAPTURE = '1'
$env:ACKMAN_TRACE = "$PWD/private/boot.csv"
.\build-mingw\AckmanRecomp.exe --no-launcher 'R:/ChatGPT/Go Go Ackman (Japan).sfc'
```

Confirm image, movement, music, Start, and transitions. Exercise every stage, boss, death, continue, menu, credits, and reset. Record fallback PCs and dynamic AOT/LLE counters. Function counts do not measure native execution percentage. A gap-free initial route does not establish all-route coverage. Watchdog failures, invalid opcodes, or an unbalanced stack fail this gate.

## C. Cycles, NMI, IRQ, DMA, and APU

`ACKMAN_TRACE` emits per-frame master cycles, H/V positions, IRQ counters, and host CPU time. It identifies coarse divergence:

```powershell
python scripts/compare_trace.py private/reference.csv private/candidate.csv
```

Export/normalize the reference to the same schema; this tool does not automatically convert Mesen traces. Accuracy validation also requires instrumentation of **every event** before/after `interp_bridge_run_interrupt` and every MMIO write at the bus, not only frame callbacks. Record master clock, PC/PB, A/X/Y/S/D/DB/P/E, H/V, event type, address/value, AOT/LLE origin, NMITIMEN, and pending/acknowledgment state. Prefer a bounded ring and a dump at first divergence; per-instruction printf changes performance.

Compare by guest timestamp:

- Vblank edge → pending NMI → actual entry → RTI and stack.
- `$4200`, `$4210/11`, late enabling, H-IRQ/V-IRQ/both, and I flag.
- `$420B/C`, DMA/HDMA, refresh, stalls, FastROM/SlowROM, and `$420D`.
- WAI/STP, WRAM polling, IRQ during long AOT routines, REP/SEP/XCE.
- `$2140–3` handshake, SPC timers, DSP, sample counts, and PCM sequence.
- WRAM/VRAM/CGRAM/OAM hashes and registers at identical boundaries.

NTSC is approximately 60.0988 Hz. Do not assume 262×1364 applies universally: mode/field/short scanline affect timing. Count accesses/master cycles rather than C duration. Inspect the first divergence; do not hide drift by changing FPS. Require zero event differences for faithful mode, or precisely document remaining limits; do not invent tolerance for delayed NMI.

The historical frame model had not yet passed this gate. Correct it before claiming exact original-hardware synchronization.

## D. Savestates and corruption

1. In each scene type, save, run 300 recorded inputs, capture state/framebuffer/PCM, reload, repeat, and compare.
2. Run 10,000 save/load cycles near IRQ, DMA, SPC upload, fades, and stage changes. Save only at a consistent guest yield point.
3. Test truncation at multiple offsets, invalid checksum, unknown version, different build/ROM/mod, read-only directories, and full disks. Rejection must not leave a partially restored host.
4. Kill the process during writes; the previous slot must remain valid. The historical upstream writer did not guarantee this. Implement temporary/atomic replacement and an envelope before approval.
5. MSU needs the extra serialization described in MODS_MSU_WIDE.md. Test loop-point loads, track changes, EOF, and data seeking.

## E. Memory, handles, and stability

```powershell
.\scripts\soak.ps1 -Exe .\build-mingw\AckmanRecomp.exe `
  -Rom 'R:/ChatGPT/Go Go Ackman (Japan).sfc' -Seconds 28800
```

The script samples Private Bytes, Working Set, handles, and threads each second; it bounds runtime and flags timeout/nonzero exit. It does not validate gameplay. During the session resize, maximize/minimize, alt-tab, change audio, hotplug, save/load, and reenter menus. Use a reproducible input route; staying at the title screen is not complete stress.

After five minutes of warm-up, compare Private Bytes slopes/allocation stacks rather than Working Set alone, which can grow from caching/paging. Require no per-iteration growth in equivalent cycles; document bounded caches. Handles/threads must return to baseline after disconnection.

MSVC AddressSanitizer:

```powershell
cmake -S . -B build-asan -G 'Visual Studio 17 2022' -A x64 -DACKMAN_ASAN=ON
cmake --build build-asan --config RelWithDebInfo --parallel 6
```

ASan detects out-of-bounds/use-after-free; **MSVC ASan does not replace a leak detector**. Use Visual Studio Memory Usage, UMDH/GFlags (Debugging Tools for Windows), or a compatible Dr. Memory. Example:

```powershell
gflags /i AckmanRecomp.exe +ust
# start the process; find its actual PID in Task Manager
umdh -p:<PID> -f:private/before.txt
# repeat the same sequence many times
umdh -p:<PID> -f:private/after.txt
umdh private/before.txt private/after.txt -f:private/diff.txt
gflags /i AckmanRecomp.exe -ust
```

Replace `<PID>` with the actual number and configure build symbols. GFlags may require administrator rights. Also run Application Verifier/PageHeap separately. Linux ASan+LSan/Valgrind are complementary, not qualification of the Windows binary. Track creation/destruction of SDL_Texture, SDL_AudioStream, gamepads, and state buffers. Stop callbacks/threads before shutting down SDL.

## F. MSU audio under stalls and concurrent input

After implementing the driver/serialization, create synthetic sine, impulse, and silent PCM with known loop points. Test 44.1/48/96 kHz devices, variable chunks, missing files, invalid headers, out-of-range loops, EOF, track changes, mute, pause, hotplug, and audio-device removal.

Inject deterministic 5/20/100/500 ms delays into the producer in a test build; separately stress CPU. **Do not sleep in the SDL callback** as a streaming solution. Measure queued frames, underruns, clipping, fractional phase, loop continuity, and one-hour drift. Prefetch with a bounded queue and explicit underrun policy; never allow unlimited queue growth. The historical MSU core did mixer file I/O, requiring a latency audit. Compare digital audio before the device.

Connect two gamepads and keyboards. Press buttons concurrently on all three, disconnect while held, reconnect, change order, and remap while another device controls gameplay. Use SDL3 instance IDs rather than persistent indices. Define opposing-direction policy and clear input on focus loss/disconnection. Ackman is single-player; multiple devices do not make it multiplayer. Require no stuck buttons, unintended player changes, or APU callback data races. The host supports remapping; validate reload/UI paths in launcher-free builds before promising live remapping.

## G. Widescreen and release qualification

Test 4:3, 16:9, 16:10, 21:9, portrait, minimum windows, DPI 100/150/200%, monitor switches, and continuous resizing. The historical expansion limit was 512×224. Compare the center with baseline; inspect map edges, unloaded tiles, sprite wrapping, priorities/transparency, HUD, menus, and transitions.

Release criteria: full-game coverage; strict identity; no unexplained timing divergence; zero ASan failures; no leak trend; failure-resilient states; exercised MSU/FMV; rejected incompatible mods; no protected ROM/assets in the source package. Keep dated logs/results. Mark unperformed checks **not executed**, never approved merely because code or documentation exists.
