#include "recomp_menu_paths.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Standalone SDK probe: use an isolated XDG root and actual installed templates. */
int main(void) {
    assert(RecompMenuPathsPrepare());
    const char *root=RecompMenuWritableDirectory();
    const char *files[]={"README.md","THIRD_PARTY_ATTRIBUTION.md","LICENSE","RELEASE_NOTES.md",
        "docs/Instructions.txt","mods/Instructions.txt","rom/Instructions.txt",
        "saves/Instructions.txt","shaders/Instructions.txt"};
    for(size_t i=0;i<sizeof(files)/sizeof(*files);i++) {
        char path[4096];size_t length=0;
        SDL_snprintf(path,sizeof(path),"%s%s",root,files[i]);
        void *data=SDL_LoadFile(path,&length);
        assert(data && length);SDL_free(data);
    }
    char path[4096];
    SDL_snprintf(path,sizeof(path),"%sdocs/Instructions.txt",root);
    const char sentinel[]="User file must survive the next boot.";
    assert(SDL_SaveFile(path,sentinel,sizeof(sentinel)));
    assert(RecompMenuPathsPrepare());
    size_t length=0;void *data=SDL_LoadFile(path,&length);
    assert(data && length==sizeof(sentinel) && !memcmp(data,sentinel,length));
    SDL_free(data);
    puts("PASS native XDG bootstrap: all release guides created; user file preserved on second boot.");
    return 0;
}
