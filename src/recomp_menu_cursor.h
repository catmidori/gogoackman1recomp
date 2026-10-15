#pragma once
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
void RecompMenuCursorReset(void);
void RecompMenuCursorUpdate(const SDL_Event *event);
#ifdef __cplusplus
}
#endif
