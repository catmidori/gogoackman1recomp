#pragma once
#include <stdbool.h>
void AckmanShaderSetFailureHandler(void (*handler)(void));
void AckmanShaderSetProbeHandler(bool (*handler)(bool));
#include "desktop/sdl_compat.h"
bool AckmanShaderDraw(SDL_Renderer *renderer,const void *pixels,int width,int height,const SDL_Rect *viewport);
void AckmanShaderShutdown(void);
