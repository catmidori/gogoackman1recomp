# Native rumble and immutable menu safety

Current behavior (2026-10-12 refinement): the event table below supersedes the
older map-address mine filter, fixed 400 ms ignition, contact-SFX,
pre-animation charge and single 900 ms climax behavior
described in the historical validation notes at the end of this document.

Escape toggles the menu through a fixed event path, consumes both edges and
ignores autorepeat. It is neither a configuration entry nor a Hotkeys item.
`[KeyMap] Menu` is a separate, optional shortcut (new-file default `None`);
existing customized bindings are preserved. Escape still cancels mapping capture
before it reaches the menu toggle. Native dialogs retain their mandatory lock.

`[General] ControllerVibration = 1` is the default, including empty configuration.
The Game checkbox updates RAM immediately; normal shutdown persists it atomically.

## Proven event sources

| Event | Trigger | Low / high motor | Duration |
|---|---|---|---|
| Player hurt | Ackman SFX command `$0B`; verified player hurt branch `$87:9085` | `$9000 / $A000` | 110 ms |
| Flying-kick hit | Actual enemy HP decrease committed at `$85:9905`, with native airborne kick state `$0414 & 2`; no input/contact-SFX trigger | `$9000 / $A000` | 110 ms |
| Charge held | Ready latch `$0414 & 1` AND rotating-arm animation `$0448 = $13/$14`; excludes dialogue/control lock `$0491 & 2` | `$6000 / $7000` | Continuous |
| Charged release | Ready latch, native button-release bit and strong-attack execution flag | `$B000 / $C000` | 150 ms |
| Enemy hit, including vehicle offense | Reduced enemy HP `$7F:C064 + slot` at `$85:9905` / `$86:9940`, after native damaging hitbox tests | `$9000 / $A000` | 110 ms |
| Boss/sub-boss final hit and defeat | Fatal HP commit or restored native death chain for one of the seven verified room/script/actor combinations; sixteen bursts from `$86:A452/$86:A4B8`, then SPC command tag `$9D` until its final clear | `$8000 / $8000` | Continuous, native chain plus actual final SFX tail |
| Bomb explosion | SFX `$04` emitted by explosion logic; launch `$05`, attack swing `$00` and charge `$0A` do not vibrate | `$E000 / $F000` | 240 ms |
| Ackman/Angel contact | `$A34D/+$D5`: wave crest reaches the angel torso using native metasprite coordinates, before the `+$EF` snap | `$9000 / $A000` | 110 ms |
| Car | Selector/script `$2500/$A56E` | `$2500 / $0000` | Continuous |
| Vehicle mine explosion | Nearby mine object state `$30/$70`, explosion SFX `$1D`, then native upward Y-vector commit `$87:AB06` consuming `$04D5 & $2000`, in the same state generation | `$9000 / $A000` | **110 ms**, identical to player hurt |
| Castle ignition | Audible KON for SFX `$38` (SPC channel tag `$B8`), until the SPC driver clears/replaces that tag | `$6000 / $2800` | **Dynamic**, actual SFX execution lifetime |
| Castle lift-off story | `$2800/$A22E`, native negative Y velocity at `+$5C`, through opaque final fade at `+$89` | `$6000 / $2800` | Continuous |
| Ending castle flight | `$2D00`, scripts `$9A4A/$9AB9/$9AF0` | `$6000 / $2800` | Continuous |
| Castle explosions | `$9B2F` explosion-actor spawns at `+$27/$3D/$53/$69/$7F/$95/$AF`, then `$04` events in `$9BDC` | `$FFFF / $FFFF` | 150 ms per event |
| Final white climax | DSP KON for channel tagged `$B0` (SFX `$30`), verified Ackman SPC mapping, gated to `$9BE4` white phase `+$1F` onward | `$FFFF / $FFFF` | 150 ms per audible onset |
| Tenshi landing | `$2D00/$9F08`, SFX `$45` after the ground-impact animation | `$FFFF / $E000` | **700 ms** |

Continuous effects use overlapping 150 ms native commands refreshed every
60 ms of host time; their intensity stays constant. Impact pulses take priority.
The active input device is preferred, with the first opened rumble-capable
joystick as fallback. Unsupported/disconnected devices fail silently.

The executor uses `SDL_RumbleJoystick`, not SDL haptic effect allocation.
Native pause/audio gates immediately send a stop on menu/dialog/focus pause,
explicit Pause, state load and rewind. Disabling the checkbox stops immediately.
Scene exit/black presentation removes the continuous base, and shutdown stops it.
Run-ahead speculative frames cannot enqueue or emit physical effects. Rewind
history and portable savestate formats remain unchanged.

## Validation and diagnostic traces

14 CTests include actual SDL virtual-joystick rumble callbacks, launch exclusion,
impact intensities, OFF behavior, pause cancellation, speculative suppression,
intermittent expiration and the 700 ms landing command (still active at 600 ms,
stopped by 720 ms). Menu tests cover the dual shortcut with the optional binding
changed or disabled, capture cancellation, checkbox persistence and navigation.

Ten save replays of 1,200 frames correlate rumble events with scene/script
positions, including all five `$A34D` encounters. Ending explosions start with
visual spawns, not the later group sound. Additional ending replays with vibration
OFF and run-ahead 2 prove zero effects and no duplicated/future effects.
Eight combat replays verify real player damage, enemy hits and bomb explosions.

`ACKMAN_RUMBLE_TRACE` logs host frame, script, position, event, intensities,
duration, target device and SDL acceptance. `ACKMAN_RUMBLE_AUDIT` and
`ACKMAN_RUMBLE_QUEUE` optionally log the proven SFX transport for inspection.

Evidence: `private/rumble-candidate/RESULTADOS.md`. The automated tests use a
virtual controller; subjective tactile feel on a physical controller still
requires user gameplay verification.


## Synchronism refinement — 12 October 2026

Air impacts follow the actual collision call sites, never mere jump/attack input
or every sound $07. Queue return addresses account for both native X/Y widths.
Charge warning starts only when the game is charging, stays on at full readiness,
and stops on interruption. Only a ready charged attack receives the stronger
release pulse; partial-charge release stops the warning without inventing a hit.

Metasprite pose records under $2200 were correlated with captured frames of all
five interludes. Contact uses the wave and angel anchors during +$D5. Stage 1
now pulses at frame 34 instead of 45 (11 frames earlier). Stage 5's save already
contains contact: one catch-up pulse is emitted when its load audio lock resumes.
The generation-based catch-up does not repeat on ordinary menu/pause resume.

The castle cinematic continues after A1D9 into A22E: ignition at frame 63,
upward motion at 169, engine until the fully black endpoint at 1416. The next
$2900 room retains a stale A22E script pointer during gameplay dialogue; it must
not inherit vibration. Final white phase starts at frame 874, with a 900 ms
strong pulse independent of the small-explosion sprites and force blank.

New evidence: private/rumble-refine-candidate/RESULTADOS.md. Eight save replays
of 2,400 frames, six extra 1,100-frame cases (run-ahead, OFF, partial charge and
actual flying-kick contact), and the 14 CTests passed. Virtual-controller rumble
callbacks were checked; no physical-controller tactile test was performed.
