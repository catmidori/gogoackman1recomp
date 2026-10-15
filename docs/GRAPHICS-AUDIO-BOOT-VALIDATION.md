# Graphics, Audio and safe ROM initialization

The canonical configuration now includes Graphics.Widescreen (default 1),
Graphics.WindowScale (internal presentation scale, default 1) and the existing
WindowSize/EnableAudio/Volume/AudioFreq keys. Menu rows share the existing
two-column guides, five-percent inset, full-row gray highlight, white checkbox
and globally registered carousel width.

Window sizes are the established 448x224 widescreen canvas multiplied by 1..9;
the menu displays the calculated pixel dimensions. Internal scale creates a
separate nearest-neighbour SDL target at native frame width/height times 1..10.
It does not increase the PPU tilemap budget or change native guest buffers.
The requested HD/4K strings are preset labels, not extra SNES rendering detail.

Audio volume uses the existing percent-to-mixer conversion. Enabling/disabling
or changing between 32000/44100/48000 reconfigures the audio stream between
frames. A failed new stream allocation preserves the previous output. Opening
a new device reconciles the current dialog/menu/manual/focus pause state before
resuming audio, so it cannot bypass any existing pause reason.

`IgnoreAspectRatio` remains a legacy input alias. `Widescreen` has precedence;
new writes use Widescreen. The root configuration was migrated preserving its
widescreen preference; WindowScale was set to the requested new default of 1.
The previous config and executable are backed up in private/pre-av-settings/.

## Validation

- Release CMake build and all 14 CTests passed. New checks exercise keyboard
  navigation for every new row, selector wrapping, legacy/canonical precedence,
  bounds, the rom directory, missing-ROM recovery, and shutdown/reload of all
  six new menu preferences with changed values.
- A 1000-frame dummy-SDL runtime replay changes internal scale through 3 and
  10, window multipliers, widescreen, audio disable/enable, all three rates and
  volume. The scale/size/widescreen hotkeys remain functional. Persisted values
  match the final requested state. Evidence: private/av-settings-candidate/smoke/.
- Native Windows startup used Rom=missing-deleted-rom.sfc. The only initial
  window was the native picker, whose address was runtime/rom. Selecting the
  correct ROM persisted its path. The log records resolution at 36.616 seconds
  and SnesInit at 36.876 seconds: the guest was initialized only after selection.
- Graphics and Audio layouts were inspected in the native game window. The
  32 kHz (original) option switched the actual audio stream and persisted numeric
  AudioFreq=32000 on close. That native session exited normally after 3350 frames.
  Evidence: private/av-settings-candidate/runtime/native-real.log and config.ini.
- All 17 patches apply to an untouched fixture of the pinned framework. A
  repeated CMake configure/build also succeeded.

User saves and ROM bytes were not modified. The user-supplied ROM is still
external to the executable; private validation copies are not distribution assets.
