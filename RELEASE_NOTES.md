# Go Go Ackman Recompiled v1.0

The first public release brings Go Go Ackman to Windows and Linux as a
high-performance native C port. Recompiled game code is the primary CPU
execution path, rather than conventional CPU interpretation. SNES video,
audio and other hardware subsystems remain modeled by the runtime, with
interpreter fallback available where required.

## Advanced graphical interface

- Minimalist Dear ImGui menu with keyboard, mouse and gamepad navigation.
- Unified focus, cyclic vertical navigation, configurable system hotkeys
  and individual or sequential control remapping.
- Embedded vector input prompts and contextual binding-conflict warnings.

## Modern video and audio

- Adaptive widescreen with scene-specific framing and selectable 4:3 CRT
  or 8:7 square-pixel presentation.
- Multiplied internal resolutions and optional integer scaling for crisp,
  pixel-perfect presentation.
- Background PNG screenshot compression with fully opaque pixels and
  selectable internal or window capture resolution.
- Recursive discovery of user-supplied MSU-1 high-fidelity audio packages.

## Adaptive mod management

- Native C conversion of the tested English and Brazilian Portuguese
  translation patches, identified by exact MD5 hashes.
- A self-contained PT-BR mod composes the English prerequisite and
  Portuguese changes in memory.
- Recursive mod discovery, metadata display, bulk selection and persistent
  activation, applied through a consolidated cold boot.

## Resilience and recovery

- Hold-to-rewind gameplay with a fluid VHS-style rewind mechanic.
- A watchdog supervisor and five rotating fallback backups protect against
  repeated startup failures.
- Automated cold boots preserve progress through an emergency snapshot
  and restore it independently of the optional autosave setting.

## User-supplied content

No game ROMs, translation patches, mod packages, MSU-1 audio packs or shader
filters are included. Follow the `Instructions.txt` files in `rom/`, `mods/`
and `shaders/` to supply compatible files. The supported unheadered Japanese
ROM has SHA-256:

```text
e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f
```

See [README.md](README.md) for setup and tested translation hashes, and
[THIRD_PARTY_ATTRIBUTION.md](THIRD_PARTY_ATTRIBUTION.md) for the retained
framework, library and asset notices.
