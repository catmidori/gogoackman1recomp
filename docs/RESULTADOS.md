# Evidence from this run

Date: 2026-09-19 in America/Sao_Paulo (2026-09-20 UTC).

- Normalized ROM: SHA-256 pinned in rom_identity.txt; 1,310,720 bytes.
- SNESRecomp: e119994bcde03dec95f9ebaeeda305d72a6668b2.
- Official CLI: nine roots, 89 AOT variants, three banks, zero emitted LLE variants. The last number does not establish complete dynamic coverage.
- Build: CMake 3.31.6, Ninja 1.12.1, MinGW GCC/G++ 13.2.0, RelWithDebInfo.
- SDL3 3.4.10 linked statically. The tarball SHA-256 matches runner.cmake. No SDL2 dependency appears in the PE import table.
- Executable: `build-ninja/AckmanRecomp.exe`.
- `python tests/test_tools.py`: two tests passed, covering synthetic ROM normalization/truncation and trace mismatch/length detection.
- CTest: 1/1 suite passed; these are not game-fidelity tests.
- Dummy-SDL smoke: exit 0; 120 simulated/presented frames in approximately two seconds; dummy audio opened at 48 kHz stereo. No visual, listening, or physical-controller test was performed.
- Trace: frame 1 at H=28/V=0 and frame 120 at H=212/V=3 in the first run. This beam displacement indicates that the frame model needs analysis; it was not validated against hardware or a reference emulator.
- Upstream warnings included FORCEINLINE redefinition; no compile/link failure remained.

Local raw evidence: `private/build.log`, `private/configure.log`, `private/smoke.log`, `private/smoke.csv`, and `build-ninja/last_run_report.json`. The report contains local paths/diagnostics and should be reviewed before publication.

Not performed at this stage: full-game playthrough, ASan, MSVC, eight-hour soak, savestatee corruption resistance, per-event differential timing, MSU with the game driver, FMV, HD textures, translation, widescreen HUD, or physical controls/remapping.

**Historical classification: buildable scaffold with an approved smoke test; the game port and enhancements were not yet qualified.** Do not present this historical executable as a fully qualified playable release.
