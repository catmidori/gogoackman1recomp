#include "recomp_menu_types.h"
#pragma once
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif

bool RecompDevicesInit(void (*changed)(void));
void RecompDevicesRequest(SDL_JoystickID id);
void RecompDevicesRemove(SDL_JoystickID id);
void RecompDevicesEvent(const SDL_Event *event);
bool RecompDevicesReady(const SDL_Event *event,RecompDeviceInfo *info);
int RecompDevicesGamepads(SDL_Gamepad **pads,int capacity);
/* Cached publication only: no bus enumeration or opening on the frame thread. */
int RecompDevicesJoysticks(SDL_JoystickID *ids,int capacity);
void RecompDevicesShutdown(void);
#ifdef __cplusplus
}
#endif
