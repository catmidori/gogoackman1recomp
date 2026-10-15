#pragma once
#include <stdint.h>
void AckmanHooksInit(void);
void AckmanHooksClose(void);
void AckmanObserveFrame(unsigned frame);
void AckmanRumblePollSpc(void);
