#pragma once
#include <stddef.h>
#include <SDL3/SDL.h>
#ifdef __cplusplus
#include "imgui.h"
struct ImRect;
struct RecompSvgAsset { const char *name;const unsigned char *data;size_t size; };
const RecompSvgAsset *RecompButtonAssets(size_t *count);
void RecompMenuIconsInit(SDL_Renderer *renderer);
void RecompMenuIconsShutdown();
void RecompMenuIconsEndFrame();
// Stable asset names are shared by gameplay bindings and host hotkeys.
const char *RecompButtonIcon(const char *binding,bool gamepad);
bool RecompDrawBindingIcons(ImDrawList *draw,const char *binding,bool gamepad,
                           ImVec2 min,ImVec2 max,float height,ImU32 color);
void RecompDrawTabHints(const ImRect &tabs,ImVec2 panel_min,ImVec2 panel_max);
struct RecompDeviceImage { ImTextureID texture;ImVec2 size,uv_min,uv_max; };
bool RecompDeviceIcon(bool gamepad,float height,RecompDeviceImage *image);
extern "C" {
#endif
SDL_Surface *RecompRasterizeSvg(const unsigned char *data,size_t size,int height);
#ifdef __cplusplus
}
#endif
