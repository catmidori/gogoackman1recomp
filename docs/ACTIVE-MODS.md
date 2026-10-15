# Persistent active mods and cold boot

`config.ini` is the authoritative selection source. Immediately after
`[Keyboard.Keybinds.Player1]`, the following section lists exact archive
relative filenames, without `=` or values:

```ini
[ActiveMods]
GoGoAckman_EN.snesmod
```

An empty section disables all installed packages. Transfers still change RAM
while the menu is open. Closing the menu, quitting, or choosing File → Reboot
atomically persists the selection; an unchanged selection does not rewrite the
file. The first-open snapshot remains the running-session baseline, so the
restart warning does not disappear merely because the INI has been saved.

Recursive catalog support now also accepts safe relative paths and standalone
`.msu` files. Ctrl/Shift selects batches in either list. Refer to
[MODS-MENU.md](MODS-MENU.md) for catalog boundaries, sidecar metadata and the
native MSU-1 device's single-source selection rule.

At boot, the wrapper enumerates executable-relative `mods/`, restores the
selection, and removes missing, duplicate and unsafe filename records. Paths
outside that directory and non-`.snesmod` entries cannot become active. Selected
archives are installed through the native ZIP/CRC/manifest/SHA-256 loader into
`mods/preloaded`; native package targets, trusted plugin IDs and conflicts are
validated before the guest runs. Unsupported/corrupt archives are deactivated
and their records removed, allowing stock boot to continue. Deleting the source
archive or clearing the INI prevents stale installed state from reviving it.

The trusted `ackman.translation.en` plugin resolves `translation.toml` from its
installed package, initializes the native `snes_text_xlate` system, and applies
the guarded ROM hooks after SnesInit and before frame 1. Reset/save-state
generation changes refresh its hook state. Shutdown releases the translation
state. No IPS parser or external mod code runs inside the executable.

The ROM's canonical 1,310,720 bytes produce this SHA-256 after activation:
`4dac0fca32d47e1d6929677b9f6ab8f6840a0504c9d20dc534a4c8cf41288f74`.
The core internally mirrors this image to 2 MiB; that internal cartridge buffer
has a different full-buffer hash. `ACKMAN_MODS_TRACE` optionally records the
canonical translated-image digest for integration diagnostics.

File → Reboot now requests an ordinary runner shutdown and starts a new OS
process after core, audio, renderer and logger cleanup. On Windows it uses
`CreateProcessW` with `CREATE_NO_WINDOW`, preserving the selected configuration
and ROM as `--config <path> --no-launcher <ROM>`. It does not replay an old script.
The Reset hotkey and the game's final-score reset retain their console-reset
behavior. The ROM-picker relaunch also preserves the selected configuration.
Fullscreen and foreground startup are detailed in
[NATIVE-TRANSLATION.md](NATIVE-TRANSLATION.md), which also documents the compiled
native package converter replacing the original external Python invocation.

The framework extension is reproducible in `patches/ackman-active-mods.patch`:
it adds a narrow native package-selection API with validation/rollback, ignores
wrapper-owned ActiveMods records in the core config parser, and adds script
commands `hostoption Mod <index>` / `hostoption Reboot 1` to exercise the exact
menu commands in integration tests. CMake renames only the desktop host's
runtime-initialization call to the game-owned bootstrap.

Validation completed:

- All 14 CTests, including active-list round trips, deletion pruning, immutable
  unrelated settings, idempotent saves and Reboot requesting exit rather than
  calling the CPU reset routine.
- Clean pinned framework reconstruction: 20 patches across 14 files.
- Existing 1,000-frame host/audio/video smoke test.
- `python tests/test_active_mods_runtime.py`: English activation in the real
  core, missing/unsafe entries, empty selection versus cached state, corrupt
  archive fallback, deleted source versus installed cache, and a real OS cold
  boot after a RAM selection change. The child applies English and completes
  another 1,000 frames.
- Native presented-frame capture at frame 850 visibly displays English text
  (“NO ONE KNOWS IT, BUT...”); evidence:
  `private/active-mods-candidate/runtime/english.png`.
