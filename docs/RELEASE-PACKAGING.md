# End-user release packages

The scripts build Release binaries and stage only approved runtime files.
No local configuration, ROM, translation patch, mod, shader or save is copied.

## Windows x64

With MinGW-w64, CMake, Ninja and Python (Pillow for icon generation) installed:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_release_windows.ps1
```

Use `-CMake`, `-Ninja`, `-Toolchain`, `-Python` and `-IconPython` to select
installed tools. A previously downloaded SDL 3.4.10 source tree can be placed
in `.tools/SDL3-3.4.10`; otherwise CMake fetches the pinned SDL release.
The script runs all CTests before creating:

`releases/Go_Go_Ackman_Recompiled_Windows_x64.zip`

The package includes the executable, complete non-system DLL dependencies,
license notices, font fallback and instruction-only content directories.
SDL3 is normally linked statically by this project; a shared SDL build's DLL
is included when imported by the executable. Windows system DLLs are excluded.

## Linux / WSL

With Flatpak, flatpak-builder and the manifest's Freedesktop SDK available:

```sh
sh scripts/build_release_linux.sh
```

Sources are staged under the Linux cache directory to avoid WSL mounted-volume
permission problems. `ACKMAN_RELEASE_STAGE` can select another absolute Linux
directory. The wrapper runs `sh scripts/build_flatpak.sh` there and exports:

- `dist/AckmanRecomp-x86_64.flatpak`
- `releases/AckmanRecomp-x86_64.flatpak`

The read-only package contains clean user-directory templates. On first boot,
the application creates `docs/`, `mods/`, `rom/`, `saves/` and `shaders/` inside
its persistent XDG configuration root, seeds their instruction files, and
copies README and license notices beside them. Existing files are preserved.
Shaders are discovered in this writable directory rather than `/app`.

The Flatpak requires its declared runtime; it does not include a ROM or grant
rights to separately supplied game content.
