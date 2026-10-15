# Official English translation — stage 1

> Historical conversion-stage audit. Persistent activation and the trusted
> plugin are now implemented; see [ACTIVE-MODS.md](ACTIVE-MODS.md).
> The executable's converter is now entirely native C; see
> [NATIVE-TRANSLATION.md](NATIVE-TRANSLATION.md).

## Registered input

Only the supplied Ackman Translations v1.0 IPS is accepted:

- MD5: `0dd930e3738524cc4f8a3d83326616a9`
- Size: 19,872 bytes
- 48 IPS records, covering 19,624 ROM bytes
- Unheadered ROM offsets: `0x3ad30` through `0x43f9f` (inclusive)
- No RLE records or ROM resizing in this particular IPS
- Stock ROM SHA-256: `e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f`

The C registry is `src/ackman_translation.h`; the standalone Python tool carries
the same identity, checked by the existing tools test. A 512-byte copier header
is stripped from a supplied ROM before identity verification. IPS addresses
are already unheadered offsets and must **not** be shifted by 512 bytes.

## Conversion

```powershell
python scripts/convert_translation.py --ips 'R:\ChatGPT\Ackman1e.ips'
```

Optional `--rom` and `--output` arguments select other filesystem locations.
Defaults are `rom/Go Go Ackman (Japan).sfc` and `mods/GoGoAckman_EN.snesmod`, relative
to the project/script installation, independently of the working directory.
No third-party Python module is required. Neither the ROM nor the IPS is written.

The converter validates IPS identity and ROM SHA-256 before creating output.
It checks record boundaries, parses literal/RLE records, rejects resizing and
out-of-range addresses, and flattens any overlapping writes in IPS order.
Publication uses a temporary file in the destination directory followed by
`os.replace`; rejected input preserves any existing package.

The deterministic ZIP32 archive contains:

- `manifest.toml`: native format 1, package `ackman.translation.en`, version
  `1.0`, exact supplied name/author/description, verified game/ROM target,
  disabled-by-default `english` feature and game-owned plugin ID.
- `translation.toml`: native `snes_text_xlate` tables containing 48
  `[[rom_patch]]` records with `address`, `source_hex`, and `en_hex`.
  Every replacement has the same size as its verified original bytes.
- `conversion.json`: input identity, address convention, record/byte counts,
  target identity and patched-image SHA-256 for auditing.

Patched-image SHA-256:
`4dac0fca32d47e1d6929677b9f6ab8f6840a0504c9d20dc534a4c8cf41288f74`.

## Menu lifecycle

Selecting an IPS calculates its MD5 in the wrapper before launching anything.
Incorrect identities produce a short red message and do not create a process.
An accepted file starts the Python interpreter discovered by CMake, with an
argument array (no command shell), the currently loaded ROM path and the
executable-relative `mods/` destination. CMake stages the script beside build
executables under `scripts/`. Keep that script and a usable Python installation
when distributing this first-stage candidate.

SDL launches a hidden background process; polling never blocks the frame loop.
SDL background processes do not expose useful exit codes, so success requires
a fresh per-run receipt written only **after** atomic publication. A previous
package cannot masquerade as a successful failed conversion. Failures show a
red message; cancellation of the native picker changes nothing. Closing the
application terminates any still-running converter and removes its receipt.

On success, the Available list refreshes immediately. Existing RAM activation
flags, selected filename, and the first-open snapshot remain intact. A new mod
starts inactive; regenerating it does not add a duplicate row. Conversion does
not change `config.ini`, run the guest, or write a save state.

## Runtime boundary and second stage

The native package runtime accepts this ZIP/manifest and its installed table
loads in `snes_text_xlate_init_c`. Native guarded writes reproduce the patched
image byte-for-byte; selecting `off` restores the original source bytes.

The framework does **not** automatically execute arbitrary `rom_patch` files
from manifests. Packages select trusted plugins already linked into the game.
This first stage deliberately does not register or activate
`ackman.translation.en`. The existing Available/Active transfer remains a RAM
prototype. Stage 2 must connect installation/activation, package-relative table
resolution, the translation frame hook, reboot/save-state handling, and actual
English gameplay validation. No generic IPS interpreter was added to the exe.

## Validation

The 14 existing CTests include additional converter tests for malformed/RLE
records, overlaps, bounds, metadata, source guards, hash registry consistency,
and preservation of existing output on rejection. Native menu tests cover MD5
vectors and same-size impostor rejection.

The optional local positive integration test uses supplied files without
distributing copyrighted fixtures:

```powershell
$env:ACKMAN_TEST_OFFICIAL_IPS = 'R:/ChatGPT/Ackman1e.ips'
$env:ACKMAN_TEST_STOCK_ROM = 'R:/ChatGPT/snes_decomp/rom/Go Go Ackman (Japan).sfc'
ctest --test-dir build-validation -R '^menu_persistence$' --output-on-failure
```

It verifies asynchronous completion, catalog refresh, preserved active flags
and snapshot, reconversion without duplicate rows, and a failed conversion
with a missing ROM. `tests/test_translation_runtime.cpp` is the standalone
native integration harness for archive installation and guarded table writes,
idempotence, restoration and refusal to overwrite mismatching source bytes.
The 1,000-frame host smoke test also remains green.
