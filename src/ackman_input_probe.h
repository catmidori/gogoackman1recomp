#pragma once
void AckmanInputProbe(unsigned frame);
void AckmanInputProbeClose(void);
#include <stdint.h>
#include <stdbool.h>
const uint8_t *AckmanInputProbeKeyboard(const uint8_t *keys);
bool AckmanInputProbeLegacy(void);
