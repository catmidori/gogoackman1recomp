# Flatpak — Go Go Ackman

Application ID: `org.snesrecomp.AckmanRecomp`. The root manifest uses `org.freedesktop.Platform` and `org.freedesktop.Sdk`, stable branch **26.08**. This branch was checked against the [official Freedesktop SDK release registry](https://gitlab.com/freedesktop-sdk/freedesktop-sdk/-/tags/freedesktop-sdk-26.08.1).

## Building on Linux

Install `flatpak` and `flatpak-builder` for your distribution and configure Flathub for the user:

```sh
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
sh scripts/build_flatpak.sh
```

The script validates the manifest with the native parser, installs runtime/SDK dependencies from Flathub, builds the modules, and exports `dist/AckmanRecomp-<arch>.flatpak`. It uses the native architecture by default; `ARCH` selects an architecture supported by the host/toolchain. Each bundle contains a binary for one architecture; the runtime provides portability across Linux distributions.

Before building, `clean_flatpak_build.py` removes only temporary `.flatpak-builder/build/AckmanRecomp-*` directories, the module link, and local cache metadata. It rejects cache directories redirected through symlinks. `--force-clean` recreates `.flatpak-build`, and `--disable-cache` prevents reuse of earlier compiled stages. Downloads remain available; dependencies are installed with `--assumeyes`.

On WSL, build in a Linux filesystem directory, such as one under `~/`. Windows volumes mounted under `/mnt/` may reject permission operations required by Flatpak/ccache. Copy the exported bundle back to the Windows volume afterward.

The build uses generated files already present in `src/gen/`, without regeneration or inclusion of a ROM in the build sandbox. The framework is fetched at the commit in `framework.lock`; 34 local patches are applied during source extraction, before CMake. zlib 1.3.2 and SDL 3.4.10 have pinned SHA-256 hashes. The main module uses CMake/Ninja, `ACKMAN_FLATPAK=ON`, and `BUILD_TESTING=OFF`.

Before patch application, `normalize_patches.py` converts CRLF to LF in all `.patch`/`.diff` files and existing text targets named in their headers. This runs once in the manifest's `type: shell` stage, after copying local sources and checking out the framework. Next, `cmake -P cmake/AckmanPatches.cmake` applies the patch stack before the first configure. Flatpak CMake configuration/regeneration does not normalize, reverse, or reapply patches, preserving timestamps on inputs such as `runner.cmake`.

Conventional builds continue to include the patch applicator. If normalization is needed, explicitly run `python scripts/normalize_patches.py --patches patches --framework external/snesrecomp` before configuring. `git apply` uses `core.autocrlf=false` and `core.eol=lf` for that invocation only; global settings are untouched. `.gitattributes` also sets LF for patches, diffs, and shell scripts. Normalization is idempotent, preserves encoding/content, and does not rewrite binaries or Git metadata.

zlib is built only as a static library. CMake rejects dynamic zlib imports under the Flatpak flag, and the manifest checks with `readelf` that the executable has no dependency on `libz.so`. SDL is installed as a shared library under `/app`; the main build does not download through FetchContent.

## Installation and execution

```sh
flatpak install --user "dist/AckmanRecomp-$(flatpak --default-arch).flatpak"
flatpak run org.snesrecomp.AckmanRecomp
```

The application menu entry is **Go Go Ackman Recompiled**. On first boot, an empty ROM preference causes the file portal to request a compatible ROM. The user supplies the ROM.

## Directories

| Content | Sandbox location |
| --- | --- |
| Executable | `/app/bin/AckmanRecomp` |
| Desktop entry | `/app/share/applications/org.snesrecomp.AckmanRecomp.desktop` |
| Icon | `/app/share/icons/hicolor/scalable/apps/org.snesrecomp.AckmanRecomp.png` |
| Fonts, shaders, and README | `/app/share/org.snesrecomp.AckmanRecomp/` |
| Configuration | `$XDG_CONFIG_HOME/config.ini` |
| Saves | `$XDG_CONFIG_HOME/saves/` |
| Mods and package cache | `$XDG_CONFIG_HOME/mods/` |
| Five recovery backups | `$XDG_CONFIG_HOME/fallback/` |
| Initial ROM directory | `$XDG_CONFIG_HOME/rom/` |
| Screenshots | `$XDG_CONFIG_HOME/screenshots/` |

Flatpak normally sets `XDG_CONFIG_HOME` to `~/.var/app/org.snesrecomp.AckmanRecomp/config`. The engine uses this value directly without adding the application ID again. Relative values are ignored; the fallback is `$HOME/.var/app/<id>/config/`. Without a valid absolute persistent root, startup fails before attempting to write to `/app`.

The supervisor, child, and cold boot retain this root as their working directory, keeping reports, checkpoints, and recovery IPC persistent. Installed assets and shaders are read from `/app/share`; this is not a save/mod destination.

With `ACKMAN_FLATPAK=ON`, the static catalog is registered through `snesrecomp_target_mod_catalog(AckmanRecomp NONE)`. `mods/preloaded` need not exist in sandbox sources: runtime mods remain enabled, and their packages/cache are managed in persistent XDG storage. Conventional builds retain the call using `mods/preloaded`.

## Icon

The official root `icon.png` is 360×360. `build_flatpak_icon.py` always produces a **360×360 RGBA** canvas, satisfying the [Flatpak square-icon requirement](https://github.com/flatpak/flatpak/blob/main/icon-validator/validate-icon.c). The current square artwork is copied without resizing or extra margins. Rectangular inputs are centered with transparent top/bottom or side margins; inputs larger than the canvas are reduced proportionally with nearest-neighbour sampling. The source file is never rewritten. Both Windows RC generation and Flatpak installation depend on the same root `icon.png`.

## Sandbox and validation

Permissions include X11, Wayland, IPC, DRI, PulseAudio, and `device=all` for peripherals/rumble. SDL3 file dialogs use the portal; the game has neither unrestricted home access nor network permission. XDG variables follow the [Flatpak sandbox documentation](https://docs.flatpak.org/en/latest/sandbox-permissions.html).

Additional Linux-host validation requires no extra Python dependencies:

```sh
flatpak-builder --show-manifest org.snesrecomp.AckmanRecomp.yaml > /tmp/ackman-manifest.json
python3 scripts/validate_flatpak.py --canonical-json /tmp/ackman-manifest.json
```

Direct YAML parsing is optional and requires PyYAML only in the development environment. The Windows CTest suite retains 14 cases, including Flatpak path checks in the configuration test.

On 2026-10-13, the complete ELF x86_64 build and bundle export passed on Ubuntu/WSL using a Linux filesystem. The bundle was imported into a test repository, and the ELF was checked for absence of a dynamic `libz.so` dependency. All 14 Windows CTests also passed. Video, audio, and rumble still require practical validation in the installed Linux application.

## Linux module inspection

`ackman-host-report-gnu.patch` moves the guarded `_GNU_SOURCE` definition to the top of `host_report.c`; `<link.h>` stays within the Linux conditional. CMake also defines `_GNU_SOURCE=1` for that translation unit on Linux because forced logging includes load `<stdio.h>` before the source file begins. The compiler definition exposes GNU APIs before any glibc header. The patch is applied during staging without changing sources during Ninja regeneration.

## Clock, fonts, and HiDPI

The Flatpak font is read from `/app/share/org.snesrecomp.AckmanRecomp/fonts/OpenSans-Regular.ttf`; SVGs are installed under `/app/share/org.snesrecomp.AckmanRecomp/buttons/`. Fonts and icons are also embedded in the binary. If font construction fails, ImGui uses `AddFontDefault()`. SDL overlay rendering applies `DisplayFramebufferScale` to geometry and restores the previous scale afterward, keeping geometry and HiDPI clip rectangles consistent.

`--disable-cache` alone does not correct future timestamps. After source extraction and patch application, the manifest runs `normalize_build_timestamps.py` to clamp only future timestamps to the current clock without changing content. This never runs in Ninja's generation/build cycle.

The script uses `flatpak-builder --clear-cache` only when the installed version advertises support in its help. Version 1.4.8 does not provide that option; the fallback removes local `.flatpak-builder/cache` metadata and temporary module builds while preserving downloads and user data.

Bundle update on 2026-10-13: build/export/import passed on Ubuntu/WSL; 14 CTests and the 2× HiDPI geometry check passed. No visual run was performed on CachyOS, which is inaccessible in this session.
