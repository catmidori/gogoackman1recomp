#pragma once
#include "desktop/sdl_compat.h"
#include <stdint.h>
#include "recomp_menu_types.h"
#ifdef __cplusplus
extern "C" {
#endif
void RecompControlsInit(void);
void RecompControlsBegin(bool gamepad);
void RecompControlsBeginSingle(bool gamepad,int button);
bool RecompControlsWizard(void);
bool RecompControlsCapturing(bool gamepad,int button);
bool RecompControlsClear(bool gamepad,int button);
void RecompControlsRaw(bool gamepad,int button,char *out,size_t capacity);
/* Shared presentation only: serialized bindings remain unchanged. */
void RecompGamepadLabel(const char *binding,char *out,size_t capacity);
const char *RecompControlsName(int button);
void RecompControlsLabel(bool gamepad,int button,char *out,size_t capacity);
void RecompControlsConflict(bool gamepad,int button,char *out,size_t capacity);
size_t RecompControlsConflicts(bool gamepad,int button,RecompConflict *out,size_t capacity);
void RecompControlsCancel(void);
bool RecompControlsActive(void);
bool RecompControlsInputBlocked(void);
const char *RecompControlsPrompt(void);
/* Captures exclusively for the wizard; never forwards a captured host hotkey. */
bool RecompControlsEvent(const SDL_Event *event);
uint16_t RecompControlsReadGamepad(SDL_JoystickID device);
/* Single-player SNES host: published SDL devices feed P1, independent of host seats. */
uint16_t RecompControlsReadDevices(void);
#ifdef __cplusplus
}
#endif
