#pragma once
#include <stdbool.h>
#include "recomp_menu_core.h"
#define AckmanAutosave RecompMenuAutosave
#define AckmanAutosaveForced RecompMenuAutosaveForced
void AckmanPauseForDialog(void);
/* Implemented by the runner: pause audio immediately, reconcile on its loop. */
void AckmanHostDialogPause(void);
