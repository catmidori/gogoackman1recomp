# Third-party provenance

The host files `main.c`, `game_rtl.c`, `host_contract.c`, `gen_stubs.c`,
`variables.h`, and `game_rtl.h` originated from SNESRecomp templates at the
commit in framework.lock. Local adaptations to `game_rtl.c` and `main.c`
are described in the build guide.

Preserve the licenses and notices in `external/snesrecomp`, including LakeSnes
components, code attributed to Snes9x, and components under PolyForm
Noncommercial. This document does not relicense those dependencies.
SDL3, CMake, and Ninja retain their respective licenses.

Input glyphs are from [Kenney Input Prompts](https://kenney.nl/assets/input-prompts),
created by Kenney and distributed under CC0. Open Sans is distributed under
SIL OFL 1.1; its notice is included in `assets/fonts/OFL.txt`.

ROM-generated code and local game assets are not part of the redistributable
source package. Review each file's license before publishing.
