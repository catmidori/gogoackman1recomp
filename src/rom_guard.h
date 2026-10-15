#pragma once
#include <stddef.h>
#include <stdint.h>
void AckmanVerifyLoadedRom(const uint8_t *data, size_t size);

#include <stdbool.h>
bool AckmanRomMatches(const uint8_t *data, size_t size);
