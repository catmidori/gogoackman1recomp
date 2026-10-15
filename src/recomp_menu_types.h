#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "desktop/sdl_compat.h"
typedef void (*RecompIniHandler)(void *,const char *,const char *,const char *);
typedef struct RecompIniValue { const char *section,*key,*value; } RecompIniValue;
typedef struct RecompDeviceInfo {
 SDL_JoystickID id;SDL_Gamepad *pad;SDL_Joystick *joystick;int player;char name[128];
} RecompDeviceInfo;
typedef enum { RECOMP_CONFLICT_HOTKEY,RECOMP_CONFLICT_KEYBOARD,RECOMP_CONFLICT_GAMEPAD } RecompConflictOrigin;
typedef struct { const char *name;RecompConflictOrigin origin; } RecompConflict;
#define RECOMP_CONFLICT_MAX 64
static inline void RecompConflictAppend(RecompConflict *out,size_t capacity,size_t *count,
 const char *name,RecompConflictOrigin origin) {
 if(*count<capacity){out[*count].name=name;out[*count].origin=origin;(*count)++;}
}
