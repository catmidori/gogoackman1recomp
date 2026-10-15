# Native translation package generation

The executable routes IPS imports by MD5. Registered English:
`0dd930e3738524cc4f8a3d83326616a9` (19,872 bytes); registered E2P PT-BR:
`7cc729dde829946791d18a2b27e48da6` (9,943 bytes). Unknown identities use the
bounded native C IPS fallback in `src/ackman_ips.c`, rather than being rejected.
`src/ackman_translation_data.h` embeds the audited 48 guarded ROM hooks,
manifest and conversion audit as immutable C data. These preserve the original
Ackman Translations 1.0 metadata, full description, spelling and line breaks.

An SDL worker thread verifies the stock ROM (including optional 512-byte copier
header normalization), constructs a stored ZIP32 archive entirely in memory,
calculates entry CRC32 values and atomically publishes
`mods/GoGoAckman_EN.snesmod`. Failed input, ROM or output checks preserve an
existing package. The main thread refreshes the available list on completion;
conversion does not activate the mod or alter the running core.

There is no Python process, interpreter path, converter staging or script
dependency in any import path. `scripts/convert_translation.py` remains an
English reference/audit utility. Python is still a build/test dependency
for existing icon and verification tools.

The native English package is byte-identical to the previously validated archive:
SHA-256 `7171e0423a68bb6a571bb9e54b6fd4300ff0a7438599125c069d170c7e5ee14e`.
`tests/test_native_translation_runtime.py` exercises the real menu conversion
without a scripts directory and with an empty PATH, compares every archive byte,
verifies CRCs and checks temporary-file cleanup. The local IPS and ROM are not
distributed as test fixtures.

PT-BR composes the English table into a private stock-ROM copy first, then applies
the registered Portuguese IPS on top. The emitted source guards refer to stock
Japanese bytes, making `mods/GoGoAckman_PTBR.snesmod` independent of any active
English mod or external English IPS. Its fixed UTF-8 manifest embeds E2P's full
requested Readme excerpt, including quotes, accents, blank lines and history.
The stock ROM on disk and the running guest are never patched by import.

Unregistered patches produce `GoGoAckman_Custom_<MD5>.snesmod`, with unverified
metadata and a generic activation plugin. Normal/RLE/overlapping IPS records are
supported, but malformed records, resize trailers and writes beyond the fixed
stock ROM are rejected. Offsets are interpreted against an unheadered ROM;
headered IPS offsets are not guessed. A generated archive does not prove the
recompiled core can execute arbitrary patched machine code. Activate one
translation at a time. See [Compatible translations.txt](Compatible%20translations.txt).

`tests/test_translation_driver.c` covers parsing bounds, RLE overlap and native
guarded-table roundtrip inside the existing tools CTest. Optional menu integration
uses `ACKMAN_TEST_PTBR_IPS` and `ACKMAN_TEST_STOCK_ROM`, checks decoded metadata,
and captures both ends of the scrollable PT-BR preview. Private validation in
`private/ptbr-translation-candidate/validate.py` checks exact EN→PT-BR composition,
CRC/metadata, unknown IPS conversion, failure cleanup and 1,000 frames in the
actual core for both PT-BR and generic packages. Full PT-BR gameplay remains
subject to testing beyond that automated boot coverage.

# Cold boot window state

Reboot captures the actual SDL fullscreen state before quitting. A private
`--ackman-fullscreen` argument is consumed by the game shim and applied after
configuration parsing, before the runner computes initial window flags. This
creates the new window directly fullscreen even if a conflicting windowed
preference is read. The normal configuration persistence remains unchanged.

Windows starts the child with `STARTF_USESHOWWINDOW`, `SW_SHOWNORMAL`,
`CREATE_NO_WINDOW` and `CREATE_SUSPENDED`. The parent grants foreground permission
with `AllowSetForegroundWindow(child_pid)` before resuming the child. The game
shim calls `SDL_RaiseWindow` immediately after successful SDL window creation.
Windows also enables SDL's `FORCE_RAISEWINDOW` and
`WINDOW_ACTIVATE_WHEN_RAISED` hints: an ordinary successful raise does not
necessarily activate the window on this platform.
The same initial raise applies to an ordinary launch. OS foreground restrictions
still govern focus permission when the originating application is inactive.

`ACKMAN_WINDOW_TRACE` optionally records initial fullscreen flags, the raise
result and actual Windows foreground ownership. The active-mod integration test checks a windowed parent, fullscreen
selection, actual OS reboot, a child created fullscreen and 1,000 translated
frames. All 14 CTests also exercise the official native converter when the local
`ACKMAN_TEST_OFFICIAL_IPS` and `ACKMAN_TEST_STOCK_ROM` variables are supplied.
Set `ACKMAN_TEST_NATIVE_WINDOWS=1` for the real Windows driver and foreground
assertion. This requires an interactive desktop; the sandbox's isolated
`CodexSandboxDesktop` has no foreground window and cannot validate that property.

Interactive Windows validation recorded `fullscreen=0 raised=1 foreground=1`
for the parent and `fullscreen=1 raised=1 foreground=1` for its cold-boot child.
Native archive parity, the 1,000-frame audio/video smoke, translated cold boot
and clean reconstruction of all 20 framework patches passed.

## Mods panel metadata

The package manifest already carries the complete Readme. The Mods panel now
reads it instead of displaying the original placeholder values. The read-only C
preview verifies ZIP32 offsets, sizes and manifest CRC before decoding the four
root string fields. It supports the stored archives generated by this converter;
it neither installs nor activates packages. Escaped line breaks are decoded
verbatim, and nested feature names cannot overwrite the package display name.
Corrupt, truncated or unsupported archives yield empty metadata without a dialog.

Menu initialization and conversion completion refresh the cached previews, even
when a conversion replaces an existing filename. The white bordered panel uses
the existing small font and scrolls through the entire description. UI tests
capture both the beginning and the final Thanks paragraphs in
`build-validation/menu-mods-translation-top.png` and
`build-validation/menu-mods-translation-end.png` (PNG inspection exports).
The later recursive/MSU catalog and half-height preview layout are described
in [MODS-MENU.md](MODS-MENU.md).
