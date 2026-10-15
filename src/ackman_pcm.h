#pragma once
#include <stdint.h>
/* 1: unity/reset, 2: fade out, 3: fade in. */
void AckmanPcmEnvelope(int command);
void AckmanMixPcm(int16_t *out,int frames);
