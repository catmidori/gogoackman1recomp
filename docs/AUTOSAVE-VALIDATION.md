# Dedicated Autosave validation

- Autosave uses executable-relative `saves/GGA1_autosave.sav` exclusively.
- Startup checks for a regular file before calling the snapshot loader. A missing file starts normal boot without failure logs or dialogs.
- The existing runner policy remains load at boot and save at normal exit; the live checkbox controls `g_config.autosave`, and shutdown persists that preference.
- Manual slots `GGA1_save_01.sav` through `GGA1_save_10.sav` are independent.
- CMake Release build succeeded. All 14 CTests passed. The debug_keys test now also covers missing autosave, dedicated naming, directory creation, overwrite and disabled behavior.
- Four isolated 1000-frame runs passed: missing file, existing state restore, disable during session, enable during session. Manual slot sentinel stayed unchanged; disabling preserved autosave bytes and mtime; both toggle values persisted in config.ini.
- Fresh pinned-framework fixture accepted all 15 patches; a repeated CMake configure succeeded.
- Evidence: private/autosave-candidate/runtime/*.log and *.fade.csv. User saves and root config.ini were not modified by validation.
