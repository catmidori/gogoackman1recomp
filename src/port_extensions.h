#pragma once
#include "host_main.h"
/* Query a VERIFIED game-specific background/sprite streaming implementation.
 * Returning margins means those columns are ready to render. No hook = 4:3.
 * Implementations must not change game simulation based on host window size.
 */
typedef void (*AckmanWorldMargins)(int requested, int *left, int *right);
void AckmanRegisterWorldMargins(AckmanWorldMargins hook);
void AckmanPrepareFrame(int dw, int dh, int *fw, int *fh);
void AckmanAfterFrame(const SnesDesktopHostFrameStats *stats);
void AckmanCloseTrace(void);
