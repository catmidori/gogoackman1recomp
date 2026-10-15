# Go Go Ackman Recompiled

A desktop port of the Snes game **Go Go Ackman** built with [SNESRecomp](https://github.com/RetroPortingToolKit/snesrecomp), SDL3, and Dear ImGui. Available for Windows and Linux, including Flatpak packaging.

The original game ROM is supplied separately by the user. No ROM image is embedded in the executable.

## Features

- In-game menu - press Esc
- Widescreen support - User can switch between widescreen and 4:3 in real time by pressing "u" or by enabling/disabling the option on the menu
- Switch between 4:3 and 8:7 aspect ratio (both have widescreen support)
- Vibration/rumble support
- Shader support when "output method" is set to OpenGL (only .glsl and .glslp shaders are supported) - shader files go in the "shaders" folder
- Turbo function - press Tab
- Rewind function - press "r"
- Support for the English translation by Ackman Translations and the Brazilian Portuguese translation by E2P Traduções
- Mod support - mods use the native format for the snesrecomp framework (.snesmod) and go in the "mods" folder
- MSU support (untested, should work)
- Savestates up to 10 slots and autosave support, also support for saving and loading the savestate file under a custom name
- Run-ahead support

## Getting started

Launch `AckmanRecomp.exe` on Windows or `AckmanRecomp-1.0-x86_64.flatpak` on Linux. If the configured ROM cannot be found, the native file picker opens before the emulation core starts. Place your ROM in `rom/` for convenient selection.

The supported unheadered Japanese ROM is **1,310,720 bytes**, with SHA-256:

```text
e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f
```

An optional 512-byte copier header is normalized during verification. A different revision requires separate analysis and validation.

Press **Esc** to open or close the menu. **Q/E** or **LB/RB** switch tabs; arrows/D-pad and Enter/gamepad A navigate and confirm. Fixed safety shortcuts also include **Tab** for turbo, **R** for rewind, and **F12** for screenshots. Additional shortcuts are configurable in Hotkeys.

The overlay contains **File, Game, Graphics, Audio, Controls, Hotkeys, Mods, and Shaders**. Opening it pauses emulation. Driver, shader, and active-mod changes are applied together when it closes, through a cold boot that restores the emergency checkpoint. Changing only the selected shader while shaders remain disabled does not trigger a restart.

## Compatible translations

In the software's menu, when clicking the "Open .ips file" button in the [Mods] tab, an embedded script will try to convert the selected .ips file into a .snesmod file (the native format the framework used in this project recognizes), but since it's an automated script, It's success is not guaranteed. However, I already tested it with some translations, and since they successfully converted, I embedded the code for the conversion for those specific files in the program's code. The translated confirmed working are the English translation made listed below

Use **Mods → Load .ips file** to convert a supported patch into a native package, then transfer it to **Active**. Activate one translation at a time. These hashes identify the IPS inputs, not the original ROM or generated package.

| Name | MD5 Hash of the .ips file | Where to find it |
| --- | --- | --- |
| English translation by Ackman Translations | `0dd930e3738524cc4f8a3d83326616a9` | https://www.romhacking.net/translations/363/ |
| Brazilian Portuguese translation by E2P Traduções | `7cc729dde829946791d18a2b27e48da6` | https://romhackers.org/traducoes/console/super-nes/go-go-ackman-e2p-traducoes/ |

These hashes match [`src/ackman_translation.h`](src/ackman_translation.h) and the supplied IPS files. The PT-BR converter applies the embedded English base before the Portuguese overlay in memory, producing one self-contained package. Unknown IPS files use a bounded native fallback; conversion alone does not establish gameplay compatibility. See [translation conversion](docs/TRANSLATION-CONVERSION.md) and [native conversion](docs/NATIVE-TRANSLATION.md).

## Configuration and user files

All preferences use one `config.ini`. Gameplay mappings are separate from system hotkeys. The program creates factory defaults when the file is missing; existing user values are preserved.

| Directory | Purpose |
| --- | --- |
| `rom/` | Default ROM picker location |
| `saves/` | Save slots, custom states, and `GGA1_autosave.sav` |
| `mods/` | Native `.snesmod` packages and MSU-1 packages |
| `shaders/` | Recursive `.glsl` and `.glslp` discovery |
| `screenshots/` | Opaque, compressed PNG captures |
| `fallback/` | Circular recovery configuration backups |

Flatpak stores writable files under `$XDG_CONFIG_HOME`, normally `~/.var/app/org.snesrecomp.AckmanRecomp/config/`. Installed read-only assets reside under `/app/share/org.snesrecomp.AckmanRecomp/`. See [Flatpak instructions](docs/FLATPAK.md).

## Building and testing

Requirements include a C/C++17 toolchain, CMake, Python for build-time asset generation, Git, and SDL3. The framework revision is pinned by `framework.lock`; local integration patches are applied by the build pipeline. See [the complete build guide](docs/BUILDING.md) for framework setup, ROM analysis, toolchain options, and generated sources.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The current automated suite contains **14 CTests**. Automated results do not replace physical gamepad, display, audio, or full-game validation. Older reports in `docs/` retain their original dates and test counts as development history.

For a native Linux Flatpak bundle:

```sh
sh scripts/build_flatpak.sh
flatpak install --user "dist/AckmanRecomp-$(flatpak --default-arch).flatpak"
flatpak run org.snesrecomp.AckmanRecomp
```

Build on a Linux filesystem when using WSL; Windows-mounted volumes may not support the permission operations required by Flatpak tooling.

## Documentation

- [Technical context and development history](docs/CONTEXTO_GERAL.md)
- [Configuration](docs/CONFIGURACAO.md)
- [Reusable menu core](docs/RECOMP_MENU_CORE.md)
- [Controller feedback](docs/HAPTIC-RUMBLE.md)
- [Mods interface](docs/MODS-MENU.md)
- [Validation protocol](docs/VALIDACAO.md)

## Credits and third-party assets

- [SNESRecomp](https://github.com/RetroPortingToolKit/snesrecomp) and its contributors provide the recompilation framework and runner.
- SDL3 and Dear ImGui provide the desktop platform and interface foundations; NanoSVG rasterizes embedded vector assets.
- **Input glyphs are from [Kenney Input Prompts](https://kenney.nl/assets/input-prompts), created by Kenney and distributed under CC0.** The embedded keyboard, mouse, and controller prompts retain this attribution.
- Open Sans provides the redistributable font fallback under the SIL Open Font License; its notice is included in `assets/fonts/OFL.txt`.
- Translation credits belong to **Ackman Translations** and **E2P Traduções / Sonic_Spin**. Their original metadata and Readmes are retained in generated packages.

See [third-party provenance](THIRD_PARTY.md) and individual dependency notices. This project does not relicense the original game or its dependencies. ROMs, personal saves, and external MSU-1 audio assets are not part of the source distribution.

## License and Acknowledgements

Code specific to this port is licensed under the
[PolyForm Noncommercial License 1.0.0](LICENSE), consistent with the
SNESRecomp framework. It is source-available for noncommercial use;
PolyForm Noncommercial is not an OSI-approved open-source license.

Distributed binaries include third-party code under the MIT, ISC and
zlib licenses. Full copyright notices, license texts and asset
acknowledgements, including Kenney Input Prompts (CC0 1.0 Universal),
are provided in [THIRD_PARTY_ATTRIBUTION.md](THIRD_PARTY_ATTRIBUTION.md).
Include that file and LICENSE with every binary distribution.
