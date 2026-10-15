#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Bounded native fallback. Records are applied in IPS order to a private ROM.
 * ROM resizing is rejected because the recompilation targets the stock image. */
bool AckmanIpsApply(const unsigned char *ips,size_t size,unsigned char *rom,size_t rom_size);
bool AckmanTranslationApplyEnglish(unsigned char *rom,size_t size,const char *table);
char *AckmanTranslationBuildTable(const unsigned char *source,const unsigned char *target,size_t size);
