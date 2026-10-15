#pragma once
#include <stddef.h>
void AckmanModsRomLoaded(size_t size);
void AckmanMsuSetRomPath(const char *rom);
int AckmanModsInitialize(const char *root,const char *game,const char *sha256);
void AckmanModsBeforeFrame(void);
void AckmanModsShutdown(void);
