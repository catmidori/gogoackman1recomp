# Gameplay debug build

Run from PowerShell:

.\scripts\play-debug.ps1 -Rom 'R:\ChatGPT\Go Go Ackman (Japan).sfc'

Replace the PCM prefix with your own path. For example, `D:\Music\ackman-7.pcm`
uses prefix `D:\Music\ackman`. Files must be MSU1 PCM, with their header;
renaming WAV files is insufficient. Omitting PcmPrefix disables MSU playback.

The script launches the game in a 1344x672 window and prints the log directory
and a `Get-Content -Wait` command for monitoring console.log in another terminal.
It replaces build-ninja/config.ini with config-debug.ini. Resize the window to
experiment with the adaptive viewport, up to 64 extra pixels on each side.

- F5: cycle through existing, valid PCM IDs 0..52, wrapping around. Restart at
  full volume and loop. Missing files are skipped. No files produces a log.
- F6: fade PCM to zero while retaining the playback cursor.
- Repeated keydown events are ignored. Both keys are consumed, including keyup,
  so they do not also load savestates. These debug controls apply in host menus
  as well. Set ACKMAN_DEBUG_KEYS=0 for the normal host bindings when launching
  the executable directly.
- These keys operate on PCM only: they do not inject a guest SPC command or
  silence native music. Normal game commands may subsequently replace the
  debug track. This is useful for testing the envelope, not proof of exact
  synchronization with the SPC.

Widescreen is enabled and forced by default in this debug build. All layouts
get the wider viewport. The known BG1 filler bypasses its visible-tile equality
check, but still requires its mapped memory format and bounds. Other layouts
show their existing PPU data in the extra area. This exposes stale tiles and
sprite clipping; it does not fix or certify them. Use ACKMAN_WIDE_FORCE=0 to
restore validation, or ACKMAN_WIDE_EXPERIMENTAL=0 to disable extension when
launching directly (play-debug.ps1 explicitly enables both).

console.log emits layout/axis changes and the six shadow scroll values sent to
BG1/BG2/BG3, plus camera coordinates and raw selectors $0400/$0401. The true
stage ID is not yet mapped: `stage=unmapped` is intentional. $0401 is known to
index the music selection table at $84:8BA7, not yet a verified stage number.
HDMA may change scroll within a frame; the logged shadows are not all per-line
register values. video.csv logs layout/camera every rendered frame; timing.csv
logs frame boundaries; audio.csv logs the mapped guest audio entry points.

After playing, share the directory printed by the script together with the
phase/room name and approximate frame or log line where a glitch occurred.
Screenshots help distinguish missing objects from incorrect background tiles.


## Border/jump correction (2026-09-21)

The wide canvas now retains a constant size while invalid margins are masked
black after rendering (including backdrop and sprites). Forced mode no longer
exposes unknown layouts or world coordinates outside the mapped RAM allocation.
Only exterior tile columns are temporarily patched; central guest VRAM is
never rewritten. World Y uses the same -1 offset as BG1 scroll, including
16-pixel boundary crossings during jumps. At a map edge, valid columns are
revealed progressively. Unknown layouts currently have black sidebars.

VSync=1 is explicit in config-debug.ini and config.ini. Hardware SDL renderers
log requested/actual VSync and any driver error. Dummy/software smoke cannot
verify physical display tearing. Timing/video CSV flushes are batched every
60 frames; abrupt termination may lose the final buffered records.

Analysis of gameplay-20260920-151432: 19788 frame boundaries, every interval
357368 clocks. Guest execution median 2.2143 ms, p99 3.1428 ms, maximum
11.2323 ms. These timings do not include the entire video presentation path,
so they do not establish an end-to-end stutter measurement.


## Follow-up: wrapping and additional layouts

The mask now resolves once per rendered frame with constant-time allocation
bounds. There is no per-pixel or per-column scanning for margins, and no
masking pass when both margins are fully visible. Boot and observed Mode 0/1
intro/menu screens with zero map stride use native PPU wrapping instead of
world-map bounds. World layouts accept the observed $0600/$2800/$6000 strides
and Mode 1 with either BG3 tile-size flag, retaining allocation guards.

Session gameplay-20260921-050519 uses Mode 1 throughout the reported gameplay,
not Mode 5/6. The user's slot-0 savestate reproduces the car scene at stride
$0600; all 240 visible source tile words match VRAM. The corrected replay
capture is private/replay-stage.png (384x224). It does not reproduce the black
line visible in screenshots/02.png. Stride $6000 is enabled experimentally
with the same bounds, but has not yet received an equivalent savestate check.

The SDL audio consumer already runs separately with a 2136-native-sample
prefill (~67 ms), an 8192-sample ring, and a smoothed +/-0.5% resampling servo.
These were retained: increasing latency or inventing SPC cycles in the audio
callback would not diagnose a presentation stall. In the 300-frame slot-0
replay, raster capture averaged 0.691 ms (maximum 1.297 ms); audio underruns
stopped after startup/state-load prefill. This headless run is not a physical
GPU/display performance measurement.

play-debug.ps1 now enables host stage profiling, audio-buffer.txt statistics,
and ACKMAN_HOST_STALL_TRACE. A guest/raster operation taking >=20 ms emits
[ackman-stall] with elapsed time, PC, INIDISP shadow and master clock. Host
stage maxima are printed on exit. The two-second transition freeze has NOT
been reproduced or corrected; forced blank does not cause an explicit wait
in ackman_timing.c, and simulation/APU remain active while it is set.

## Buffered diagnostics and new saves (2026-10-02)

Diagnostics now arrive in batches every 180 simulated frames (about 3 s),
written by a background thread. Normal exit drains the final batch. Explicit
file close also drains it. Keep the process running while collecting gameplay;
force-killing it can lose the last RAM batch.

The opening scene script $9447 is pillarboxed; title script $BB63 and story
scripts $ABD9/$A964/$A7C4/$A62D/$A1D9 use native wrapping. BG2 scroll and window registers are now
replayed per scanline; visible phase/dialogue verification remains necessary.

New savestates include the actual executing CPU and audio clock state.
Legacy saves lacking these registers are refused with [ackman-state] in
console.log, preserving the running session. Keep old files for investigation,
but create a new save during gameplay to test this build. PCM playback position
is still not restored; the native SPC state is retained.

## Release of 2026-10-03

The root executable uses config.ini with an adaptive viewport and VSync.
F5/F6 do not force audio by default. Only ACKMAN_DEBUG_KEYS=1 enables
laboratory commands; scripts/play-debug.ps1 still enables them explicitly.
The RAM logger and its writer thread remain available in Release,
draining every 180 frames and on normal shutdown.
At the final score screen, Start (Enter with the default keyboard mapping)
performs a 60-frame visual fade and restarts CPU/core/audio and the session,
preserving save files. Scene policies and validation are in WIDESCREEN-SCENES.md.
