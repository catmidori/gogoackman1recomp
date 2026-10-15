#pragma once
#include "desktop/sdl_compat.h"
void AckmanCaptureRequest(void);
void AckmanCaptureInit(void);
void AckmanCaptureShutdown(void);
void AckmanCapturePresent(SDL_Renderer *renderer,const void *native_pixels,int width,int height);
