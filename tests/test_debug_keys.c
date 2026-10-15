#include "desktop/sdl_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "desktop/config.h"
#include "ackman_debug.h"
#include "ackman_menu.h"
Config g_config;
static int autosaves,autoloads;
static bool fail_snapshot;
static int dialog_audio_pauses;
void AckmanHostDialogPause(void) { dialog_audio_pauses++; }
static char autosaved[4096],autoloaded[4096];
bool RtlSaveSnapshot(const char *path) {
    autosaves++;SDL_strlcpy(autosaved,path,sizeof(autosaved));
    SDL_IOStream *file=SDL_IOFromFile(path,"wb");if(!file)return false;
    SDL_WriteIO(file,"state",5);bool closed=SDL_CloseIO(file);return closed && !fail_snapshot;
}
bool RtlLoadSnapshot(const char *path) {
    autoloads++;SDL_strlcpy(autoloaded,path,sizeof(autoloaded));return true;
}
bool AckmanPollEvent(SDL_Event *event);
static int next,fade;
void AckmanLogShutdown(void) {}
void AckmanMenuSync(void) {}
bool AckmanMenuEvent(SDL_Event *event) { (void)event;return false; }
void AckmanAudioDebug(int advance) { if(advance) next++; else fade++; }
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"failed line %d\n",__LINE__); exit(1); } } while(0)
static void key(Uint32 type,SDL_Keycode code,bool repeat) {
    SDL_Event e={0}; e.type=type; e.key.key=code; e.key.repeat=repeat;
    CHECK(SDL_PushEvent(&e));
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    AckmanPauseForDialog();CHECK(dialog_audio_pauses==1);
    const char *directory="autosave-test/saves/";
    const char *path="autosave-test/saves/GGA1_autosave.sav";
    SDL_RemovePath(path);SDL_RemovePath(directory);SDL_RemovePath("autosave-test");
    RecompMenuAutosaveConfigure(directory,"GGA1_autosave.sav");
    g_config.autosave=true;
    CHECK(!AckmanAutosave(false) && autoloads==0); /* missing: no core call */
    CHECK(AckmanAutosave(true) && autosaves==1 && !strcmp(autosaved,"autosave-test/saves/GGA1_autosave.sav.tmp"));
    CHECK(AckmanAutosave(true) && autosaves==2); /* overwrite, not another slot */
    SDL_PathInfo info;CHECK(SDL_GetPathInfo(path,&info) && info.size==5);
    CHECK(AckmanAutosave(false) && autoloads==1 && !strcmp(autoloaded,path));
    g_config.autosave=false;
    CHECK(!AckmanAutosave(false) && !AckmanAutosave(true));
    CHECK(autoloads==1 && autosaves==2);
    CHECK(AckmanAutosaveForced(false) && autoloads==2 && !g_config.autosave);
    CHECK(AckmanAutosaveForced(true) && autosaves==3 && !g_config.autosave);
    fail_snapshot=true;CHECK(!AckmanAutosaveForced(true));
    CHECK(SDL_GetPathInfo(path,&info) && info.size==5);
    CHECK(!SDL_GetPathInfo("autosave-test/saves/GGA1_autosave.sav.tmp",&info));
    SDL_RemovePath(path);
    fail_snapshot=false;
    RecompMenuAutosaveConfigure(directory,"OtherGame_autosave.sav");
    CHECK(AckmanAutosaveForced(true));
    CHECK(!strcmp(autosaved,"autosave-test/saves/OtherGame_autosave.sav.tmp"));
    CHECK(SDL_GetPathInfo("autosave-test/saves/OtherGame_autosave.sav",&info));
    SDL_RemovePath("autosave-test/saves/OtherGame_autosave.sav");
    SDL_RemovePath(directory);SDL_RemovePath("autosave-test");
    SDL_FlushEvents(SDL_EVENT_FIRST,SDL_EVENT_LAST);
    _putenv("ACKMAN_DEBUG_KEYS=");
    key(SDL_EVENT_KEY_DOWN,SDLK_F5,false);
    SDL_Event e;
    CHECK(AckmanPollEvent(&e)); CHECK(e.key.key==SDLK_F5 && next==0 && fade==0);
    CHECK(!AckmanPollEvent(&e)); /* finish SDL poll cycle before next batch */
    _putenv("ACKMAN_DEBUG_KEYS=1");
    key(SDL_EVENT_KEY_DOWN,SDLK_F5,false);
    key(SDL_EVENT_KEY_DOWN,SDLK_F5,true);
    key(SDL_EVENT_KEY_UP,SDLK_F5,false);
    key(SDL_EVENT_KEY_DOWN,SDLK_F6,false);
    key(SDL_EVENT_KEY_UP,SDLK_F6,false);
    key(SDL_EVENT_KEY_DOWN,SDLK_RETURN,false);
    CHECK(AckmanPollEvent(&e));
    CHECK(e.type==SDL_EVENT_KEY_DOWN && e.key.key==SDLK_RETURN);
    CHECK(next==1 && fade==1);
    SDL_Quit();
    return 0;
}
