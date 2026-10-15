# Hold-to-rewind

Hold the configured `[KeyMap] Rewind` shortcut (default `8`) to restore one
older whole-machine snapshot per presentation. Release it to resume from that
point. Keyboard chords, gamepad buttons and signed gamepad axes use the existing
Hotkeys bindings. Release processing precedes Alt/dialog input quarantine;
focus loss also releases the hold, and keyboard autorepeat cannot reactivate a
cancelled chord.

`RewindInterval` is INI-only, defaults to `4`, and accepts `2`, `4`, or `6`.
`RewindDepth` defaults to `240` (bounded to 4..240). At about 60 guest frames
per second, default settings retain 16 seconds. Missing or invalid cadence
values fall back to 4; changing cadence starts a fresh history.

Escape is an immutable, hidden menu safety shortcut. A second shortcut is
configurable through `[KeyMap] Menu` (default `None`) and the Hotkeys tab.
Escape still cancels input capture. Vertical navigation
routes first-row Up to the open tab and last-row Down to that same tab.
Up from ANY focused tab label goes to the open content's final enabled row,
even when the label belongs to a different tab. Focus does not select a tab.
Scrolling child lists follow the same rules.

During a hold the normal CPU/APU frame loop, run-ahead and forward snapshot
capture stop. The audio stream is paused and locked until release, which clears
the output/resampling timeline through the existing deep-flush transaction.
The usual menu, inactive-window and explicit pause rules still apply.

Snapshots retain CPU, RAM, APU/DSP, DMA/PPU and Ackman execution state. A small
process-local raster journal and fade/presentation context accompany each
snapshot. Restored raster splits are rearmed and drawn without CPU execution;
the logical snapshot is restored again after drawing so HDMA presentation work
cannot modify the state resumed on release. Run-ahead records the real frame's
journal, rather than its speculative picture. The abandoned future is trimmed.
The oldest available state remains stationary until release.

There are no rewind thumbnails, filmstrip allocations or bottom diagnostic
panel. The ring stays in RAM; its size is learned from the actual state instead
of assuming 258 KB. The current complete blob is 315,991 bytes, plus small raster
and allocation overhead. At depth 240 a warmed ring uses roughly 74 MiB.
Portable `.sav` files and their layout are unchanged. Existing MSU/PCM cursor
serialization limitations remain; native SPC/DSP state is restored.

Reproducible validation:

- The 14 CTests include cadence/wrap/clamp/future-trim tests, journal rearming,
  keyboard/modifier releases, repeat suppression, menu navigation and persistence.
- `private/hold-rewind-candidate/runtime.py`: real SDL-event holds/releases for
  keyboard, gamepad and run-ahead, 1,000 forward frames each. Host frame counters
  remain constant during backward stepping; restored guest clocks decrease at
  the selected capture cadence, allowing existing instruction overshoot.
- `ACKMAN_REWIND_TRACE` optionally records diagnostics. `hosthold KEY MS` and
  `hostpadhold BUTTON MS` in a replay script release through an SDL timer, so
  testing does not depend on advancing the suspended guest/script clock.
- `patches/ackman-hold-rewind.patch` keeps framework changes reproducible.
  `ackman-mod-catalog-shell.patch` fixes the Windows post-build catalog separator
  for multiple translation packages without changing their selection.
