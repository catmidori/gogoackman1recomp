# Native dialog pause lock

The atomic dialog-pending state is a mandatory pause source, independent of
General.PauseWhenInactive. It is acquired before invoking SDL's native open/save
dialogs. The runner pauses audio immediately and skips simulation, run-ahead,
rewind processing and script advancement while the lock is held. Presentation
continues, and the pacing clock is reset to avoid catching up after dismissal.

The callback queues its result for the main thread. The lock remains held until
that result has been applied, including confirmation, cancellation and errors.
If event delivery fails, callback-owned memory is freed and the lock is released.
Audio reconciliation also handles a callback that completes before the next
loop iteration; menu, manual and inactive-window pauses remain authoritative.

## Validation

- CMake Release build succeeded; 14/14 CTests passed.
- Regression checks cover acquisition before either native API, focus regain
  with PauseWhenInactive disabled, deferred callback handling, cancellation,
  file confirmation and a frozen AckmanRunFrame while the dialog is active.
- The native Windows dialogs were opened using the candidate's File menu,
  with PauseWhenInactive=0 and RunAhead=4:

| Dialog | Result | Frame at open / close | Duration |
| --- | --- | --- | --- |
| Save to file... | Saved private dialog-pause-validation.sav (315991 bytes) | 1483 / 1483 | 67.440 s |
| Load save from file... | Restored that private snapshot | 1899 / 1899 | 7.903 s |
| Select ROM... | Cancelled | 2589 / 2589 | 31.209 s |

Audio pause/resume records have the same frame numbers. After each dismissal,
the scene resumed; the session exited normally through the window close button
after 3111 frames. Native evidence: private/dialog-pause-candidate/runtime/
native-real.log and native-real.fade.csv.

The separate 1000-frame headless smoke passed. A clean pinned-framework fixture
accepted all 16 patches. Root config.ini and user saves were not modified.
