/* Recheck the exact bytes passed to SnesInit, after file resolution. */
#include "rom_guard.h"
#include "sha256.h"
#include "snesrecomp_rom_identity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool AckmanRomMatches(const uint8_t *data, size_t size) {
    uint8_t hash[32];
    char hex[65];
    static const char digits[] = "0123456789abcdef";
    if (size % 32768 == 512) { data += 512; size -= 512; }
    if (size != 1310720) return false;
    sha256_compute(data, size, hash);
    for (size_t i = 0; i < 32; ++i) {
        hex[2*i] = digits[hash[i] >> 4];
        hex[2*i+1] = digits[hash[i] & 15];
    }
    hex[64] = 0;
    return !strcmp(hex, SNESRECOMP_ROM_EXPECTED_SHA256);
}
void AckmanVerifyLoadedRom(const uint8_t *data,size_t size) {
    if(!AckmanRomMatches(data,size)) {
        fputs("Rejected ROM: size or SHA-256 differs from the analyzed revision.\n",stderr);
        exit(EXIT_FAILURE);
    }
}
