#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pure resolver: Flatpak's XDG_CONFIG_HOME is already scoped to this app.
 * Relative XDG values are ignored; the fallback is HOME/.var/app/ID/config. */
bool RecompMenuResolveConfigRoot(const char *xdg,const char *home,const char *app_id,
                                char *out,size_t capacity);
/* Installed assets are read-only. Writable paths never fall back to /app. */
const char *RecompMenuAssetDirectory(void);
const char *RecompMenuFontDirectory(void);
const char *RecompMenuButtonDirectory(void);
const char *RecompMenuWritableDirectory(void);
bool RecompMenuPathsPrepare(void);
#ifdef __cplusplus
}
#endif
