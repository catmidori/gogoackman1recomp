# Mods catalog and multi-selection

The Mods tab keeps the global five-percent inset, small metadata/instruction font
and 42% / 16% / 42% horizontal layout. `Load .ips file` retains its native dialog,
MD5 gate and asynchronous C conversion. The two lists have single white borders
and inner padding; the center contains one outlined `<->` button.

The metadata panel has exactly half its previous available height. Its released
space is added to both list heights, and the warning row stays reserved. Name,
Author, Version and Description remain scrollable inside the padded white frame.

## Recursive catalog

Only `.snesmod` and `.msu` files are listed, recursively beneath executable-relative
`mods/`. Extension checks are case insensitive. PCM tracks, IPS inputs, XML/BML
sidecars and other files are excluded from the lists. The native `preloaded/`
installation cache is reserved and excluded to avoid duplicate installed assets.
Links/junctions are not followed. Traversal is bounded to 32 levels and 4,096 entries.

Identifiers are relative paths with forward slashes, for example:

```ini
[ActiveMods]
GoGoAckman_EN.snesmod
Music Pack/audio/ackman.msu
```

Absolute paths, parent components and INI control characters are rejected. The
existing persistence, missing-file pruning, restart snapshot and native package
installation work with these identifiers. Directory prefixes distinguish files
with the same basename. Conversion refresh preserves selections and snapshots.

Stored `.snesmod` manifests retain their CRC-checked metadata preview. For MSU,
sidecars are searched in the same directory: matching basename XML/BML first,
then `manifest.xml`/`manifest.bml`, then other XML/BML files. Root metadata and
BML `information`/`metadata` sections support name/title, author/authors, version
and description/note. XML entities and CDATA, and BML indented description lines,
are decoded for display. No external XML entities or resources are fetched.
Missing fields remain blank; hardware/ROM/track names are not used as titles.

At cold boot standalone MSU selections bypass ZIP installation. The first active
MSU path in catalog order supplies the one native MSU-1 device's base prefix.
Additional selections are retained, but the device has one source at a time.
This does not convert another game's soundtrack into Ackman's command mapping;
the existing experimental Ackman music routing remains separately opt-in.

## Selection confined to these lists

- A plain click selects one item, replacing other selections; clicking a selected
  item again deselects it. Ctrl toggles individual items without clearing peers.
- Shift selects the inclusive visible range between the anchor and clicked item.
  Ctrl+Shift adds the range. A selection is confined to one list at a time.
- Mouse hover does not teleport or replace the selection in these lists. Other
  menu items retain the global single-cursor behavior and idle-pointer lock.
- Keyboard/gamepad navigation selects its new row. Enter/gamepad A transfers the
  selected batch in that list, or the focused item if no batch is selected.
  `<->` transfers the selected batch. All transferred selections are cleared.
- Membership changes remain in RAM until menu close, exit or Reboot persists them.
  The restart warning disappears when membership returns to its boot snapshot.

Tests cover actual Ctrl/Shift clicks, Enter/gamepad batch transfer, toggle-off,
range selection, recursive paths, filtering, XML/BML metadata, persistence,
single borders and the half-height panel. Screenshot evidence is exported from
`build-validation/menu-mods-multiselect.bmp` and the metadata top/end captures.
