#include "recomp_menu_types.h"
#pragma once
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef RecompIniHandler AckmanIniHandler;
typedef RecompIniValue AckmanIniValue;
extern const char AckmanDefaultConfig[];
const char *AckmanConfigPath(void);
bool AckmanConfigSelect(const char *path);
bool AckmanIniRead(const char *path, AckmanIniHandler handler, void *context);
/* ActiveMods uses bare basenames, one per line, rather than key=value pairs. */
bool AckmanConfigWriteActiveMods(const char *const *names,size_t count);
/* One atomic publication of graphics preferences and the complete mod list. */
bool AckmanConfigCommitRuntime(const AckmanIniValue *values,size_t count,
                              const char *const *mods,size_t mod_count);
bool AckmanModFilenameValid(const char *name);
/* A NULL value removes a key; an empty string keeps it explicitly unbound. */
bool AckmanConfigMigrateHotkeys(void);
bool AckmanConfigUpdate(const AckmanIniValue *values, size_t count);
bool AckmanConfigUpdateRemoving(const AckmanIniValue *values,size_t count,
                               const char *const *sections,size_t section_count);
#ifdef __cplusplus
}
#endif
