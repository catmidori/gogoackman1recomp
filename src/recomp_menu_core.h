#pragma once
#include "desktop/sdl_compat.h"
#include "recomp_menu_host.h"
#include "recomp_menu_snes.h"
#include "recomp_menu_paths.h"
#include "recomp_menu_boot.h"
#include "recomp_menu_cursor.h"
#include "recomp_menu_controls.h"
#include "recomp_menu_hotkeys.h"
#include "recomp_menu_devices.h"
#include <stddef.h>
#include "recomp_menu_metadata.h"
#ifdef __cplusplus
extern "C" {
#endif
void RecompMenuAutosaveConfigure(const char *directory,const char *filename);
bool RecompMenuAutosave(bool save);
bool RecompMenuAutosaveForced(bool save);
void RecompMenuShaderFailure(void);
/* False keeps the menu/core paused if checkpoint or atomic commit fails. */
bool RecompMenuClosed(void);
void RecompMenuOpened(void);
int RecompMenuActiveOutput(void);
bool RecompMenuActiveShaders(void);
const char *RecompMenuActiveShader(void);
void RecompMenuSetBootObserver(void (*observer)(bool));
void RecompMenuUiGamepadsChanged(void);
enum { RECOMP_MENU_ROM=100, RECOMP_MENU_FILE, RECOMP_MENU_RESET, RECOMP_MENU_QUIT,
       RECOMP_MENU_SAVE_FILE, RECOMP_MENU_IPS,
       RECOMP_MENU_SAVE=200, RECOMP_MENU_LOAD=300 };
void RecompMenuInit(SDL_Window *window,const char *rom);
const RecompModMetadata *RecompMenuModMetadata(int index);
void RecompMenuShutdown(void);
void RecompMenuSync(void);
bool RecompMenuEvent(SDL_Event *event);
void RecompMenuCommand(int command);
void RecompMenuQueueCommand(int command);
int RecompMenuRelaunch(void);

void RecompMenuConfigure(void);
const char *RecompMenuSaveDirectory(void);
const char *RecompMenuRomDirectory(void);
int RecompMenuResolveRom(int argc,char **argv,char *out,size_t capacity,const unsigned char *sha);
void RecompMenuSetDisplayAspect(int aspect);
void RecompMenuShadersRefresh(void);
int RecompMenuShaderCount(void);
const char *RecompMenuShaderName(int index);
const char *RecompMenuShaderDirectory(void);
void RecompMenuSelectShader(int index);
void RecompMenuSetShaders(bool enabled);
enum { RECOMP_SETTINGS_FULLSCREEN=1, RECOMP_SETTINGS_VSYNC=2,
       RECOMP_SETTINGS_OUTPUT=4, RECOMP_SETTINGS_RUNAHEAD=8, RECOMP_SETTINGS_SIZE=16,
       RECOMP_SETTINGS_AUDIO=32, RECOMP_SETTINGS_VOLUME=64, RECOMP_SETTINGS_REWIND=128 };
unsigned RecompMenuTakeSettings(void);
bool RecompMenuInactive(void);
bool RecompMenuDialogActive(void);
const char *RecompMenuModsDirectory(void);
const char *RecompMenuIpsPath(void);
void RecompMenuModsInit(void);
void RecompMenuModsOpened(void);
void RecompMenuModsRefresh(void);
int RecompMenuModsCount(void);
const char *RecompMenuModName(int index);
bool RecompMenuModActive(int index);
int RecompMenuModSelected(void);
void RecompMenuModSelect(int index);
bool RecompMenuModHighlighted(int index);
void RecompMenuModClick(int index,bool control,bool shift);
void RecompMenuModActivate(int index);
void RecompMenuModTransfer(void);
bool RecompMenuModsChanged(void);
bool RecompMenuModsSave(void);
void RecompMenuModSetActive(int index,bool active);
bool RecompMenuColdBootPending(void);
int RecompMenuRunAheadMax(void);
void RecompMenuSetRunAhead(int frames);
void RecompMenuSetRewindInterval(int interval);
void RecompMenuSetFullscreen(bool active);
bool RecompMenuSetFullscreenCheckbox(bool active,bool gamepad);
void RecompMenuUiLockMouseFocus(void);
bool RecompMenuFullscreen(void);
bool RecompMenuSoftwareBackbufferSize(SDL_Renderer *renderer,int *width,int *height);
void RecompMenuSetControllerVibration(bool active);
void RecompMenuSetVSync(bool active);
void RecompMenuSetOutputMethod(int method);
void RecompMenuSetWidescreen(bool active);
void RecompMenuSetWindowSize(int multiplier);
int RecompMenuWindowMultiplier(void);
void RecompMenuSetInternalScale(int scale);
void RecompMenuSetAudio(bool active);
void RecompMenuSetVolume(int volume);
void RecompMenuSetSampleRate(int frequency);
void RecompMenuShortcut(bool load,int slot,char *out,size_t capacity);
bool RecompMenuFullscreenKey(const SDL_Event *event);

int RecompMenuHeight(int output_height);
void RecompMenuDraw(SDL_Renderer *renderer,bool popup);
bool RecompMenuOwnsInput(void);
bool RecompMenuVisible(void);
bool RecompMenuAltOwnsInput(void);
void RecompMenuUiSuppressInput(void);
void RecompMenuUiMouseWakeOnly(void);

void RecompMenuUiInit(SDL_Window *window);
void RecompMenuUiShutdown(void);
void RecompMenuUiSync(void);
void RecompMenuUiToggle(void);
bool RecompMenuUiEvent(SDL_Event *event);
bool RecompMenuUiClearBinding(const SDL_Event *event);
void RecompMenuRendererShutdown(void);
#ifdef __cplusplus
}
#endif
