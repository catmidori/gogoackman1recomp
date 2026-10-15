#include "recomp_menu_paths.h"
#include "desktop/sdl_compat.h"
#include <string.h>

bool RecompMenuResolveConfigRoot(const char *xdg,const char *home,const char *app_id,
                                char *out,size_t capacity) {
    if(!out || !capacity)return false;
    out[0]=0;
    int size;
    if(xdg && xdg[0]=='/') {
        size=SDL_snprintf(out,capacity,"%s%s",xdg,xdg[strlen(xdg)-1]=='/'?"":"/");
    } else {
        if(!home || home[0]!='/' || !app_id || !*app_id ||
           strchr(app_id,'/') || strchr(app_id,'\\'))return false;
        size=SDL_snprintf(out,capacity,"%s%s.var/app/%s/config/",home,
                          home[strlen(home)-1]=='/'?"":"/",app_id);
    }
    if(size<0 || (size_t)size>=capacity){out[0]=0;return false;}
    return true;
}

const char *RecompMenuAssetDirectory(void) {
#ifdef RECOMP_MENU_FLATPAK
    return RECOMP_MENU_INSTALLED_ASSETS "/";
#else
    const char *base=SDL_GetBasePath();return base?base:"./";
#endif
}

static const char *asset_subdirectory(const char *folder,char *out,size_t size) {
    SDL_snprintf(out,size,"%s%s/",RecompMenuAssetDirectory(),folder);return out;
}
const char *RecompMenuFontDirectory(void) {
    static char path[4096];
#ifdef RECOMP_MENU_FLATPAK
    return asset_subdirectory("fonts",path,sizeof(path));
#else
    return asset_subdirectory("assets/fonts",path,sizeof(path));
#endif
}
const char *RecompMenuButtonDirectory(void) {
    static char path[4096];
#ifdef RECOMP_MENU_FLATPAK
    return asset_subdirectory("buttons",path,sizeof(path));
#else
    return asset_subdirectory("assets/buttons",path,sizeof(path));
#endif
}
const char *RecompMenuWritableDirectory(void) {
#ifdef RECOMP_MENU_FLATPAK
    static char root[4096];
    if(!root[0] && !RecompMenuResolveConfigRoot(SDL_getenv("XDG_CONFIG_HOME"),
       SDL_getenv("HOME"),RECOMP_MENU_FLATPAK_APP_ID,root,sizeof(root))) {
        SDL_SetError("No absolute persistent Flatpak configuration directory");
        return NULL;
    }
    return root;
#else
    const char *base=SDL_GetBasePath();return base?base:"./";
#endif
}

#ifdef RECOMP_MENU_FLATPAK
static bool seed_user_file(const char *root,const char *relative) {
    char destination[4096],source[4096];
    if(SDL_snprintf(destination,sizeof(destination),"%s%s",root,relative)>=(int)sizeof(destination) ||
       SDL_snprintf(source,sizeof(source),"%suser-template/%s",RecompMenuAssetDirectory(),relative)>=(int)sizeof(source))return false;
    SDL_PathInfo info;
    if(SDL_GetPathInfo(destination,&info))return info.type==SDL_PATHTYPE_FILE;
    size_t length=0;
    void *data=SDL_LoadFile(source,&length);
    if(!data)return false;
    bool saved=SDL_SaveFile(destination,data,length);
    SDL_free(data);
    return saved;
}
#endif

bool RecompMenuPathsPrepare(void) {
#ifdef RECOMP_MENU_FLATPAK
    const char *root=RecompMenuWritableDirectory();
    if(!root || !SDL_CreateDirectory(root))return false;
    static const char *folders[]={"saves","mods","fallback","rom","screenshots","shaders","docs"};
    for(size_t i=0;i<sizeof(folders)/sizeof(*folders);i++) {
        char path[4096];
        int size=SDL_snprintf(path,sizeof(path),"%s%s",root,folders[i]);
        if(size<0 || (size_t)size>=sizeof(path) || !SDL_CreateDirectory(path))return false;
    }
    static const char *files[]={"README.md","THIRD_PARTY_ATTRIBUTION.md","LICENSE","RELEASE_NOTES.md",
        "docs/Instructions.txt","mods/Instructions.txt","rom/Instructions.txt",
        "saves/Instructions.txt","shaders/Instructions.txt"};
    for(size_t i=0;i<sizeof(files)/sizeof(*files);i++)
        if(!seed_user_file(root,files[i]))return false;
#endif
    return true;
}
