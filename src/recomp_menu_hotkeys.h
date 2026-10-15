#pragma once
#include "desktop/sdl_compat.h"
#include "recomp_menu_types.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RECOMP_HOTKEY_COUNT 34
void RecompHotkeysInit(void);
const char *RecompHotkeysName(int item);
const char *RecompHotkeysKey(int item);
void RecompHotkeysLabel(int item,char *out,size_t size);
void RecompHotkeysRaw(int item,char *out,size_t size);
void RecompHotkeysConflict(int item,char *out,size_t size);
size_t RecompHotkeysConflicts(int item,RecompConflict *out,size_t capacity);
size_t RecompHotkeysGameplayConflicts(bool gamepad,int button,RecompConflict *out,size_t capacity);
void RecompHotkeysGameplayConflict(bool gamepad,int button,char *out,size_t size);
void RecompHotkeysBegin(int item);
void RecompHotkeysCancel(void);
bool RecompHotkeysActive(void);
bool RecompHotkeysCapturing(int item);
bool RecompHotkeysInputBlocked(void);
bool RecompHotkeysCaptureEvent(const SDL_Event *event);
bool RecompHotkeysClear(int item);
bool RecompHotkeysHostEvent(const SDL_Event *event);
/* Release-only path runs before dialog/Alt quarantine; never activates input. */
void RecompHotkeysReleaseRewindEvent(const SDL_Event *event);
bool RecompHotkeysFixedEvent(const SDL_Event *event,bool allow_press);
#ifdef __cplusplus
}
#endif

