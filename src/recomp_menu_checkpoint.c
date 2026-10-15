#include "recomp_menu_core.h"
#include "desktop/config.h"
#include "common_rtl.h"
#include <stdio.h>
static char autosave_directory[4096],autosave_path[4096];
void RecompMenuAutosaveConfigure(const char *directory,const char *filename) {
    SDL_strlcpy(autosave_directory,directory,sizeof(autosave_directory));
    int size=snprintf(autosave_path,sizeof(autosave_path),"%s%s",directory,filename);
    if(size<0 || (size_t)size>=sizeof(autosave_path))autosave_path[0]=0;
}
bool RecompMenuAutosave(bool save) {
    return g_config.autosave && RecompMenuAutosaveForced(save);
}
bool RecompMenuAutosaveForced(bool save) {
    if(!autosave_path[0])return false;
    if(save) {
        if(!SDL_CreateDirectory(autosave_directory))return false;
        char tmp[4096];if(SDL_snprintf(tmp,sizeof(tmp),"%s.tmp",autosave_path)>=(int)sizeof(tmp))return false;
        bool ok=RtlSaveSnapshot(tmp) && SDL_RenamePath(tmp,autosave_path);
        if(!ok)SDL_RemovePath(tmp);return ok;
    }
    SDL_PathInfo info;
    // First boot has no autosave: do not enter the core's load/error path.
    if(!SDL_GetPathInfo(autosave_path,&info) || info.type!=SDL_PATHTYPE_FILE)return false;
    return RtlLoadSnapshot(autosave_path);
}
