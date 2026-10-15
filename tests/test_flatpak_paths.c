#include "recomp_menu_paths.h"
#include "desktop/sdl_compat.h"
#include <assert.h>
#include <string.h>
int main(int argc,char **argv) {
    const char *id="org.snesrecomp.AckmanRecomp";
    char out[256];
    assert(RecompMenuResolveConfigRoot("/sandbox/config",NULL,id,out,sizeof(out)));
    assert(!strcmp(out,"/sandbox/config/"));
    assert(RecompMenuResolveConfigRoot("/sandbox/config/",NULL,id,out,sizeof(out)));
    assert(!strcmp(out,"/sandbox/config/"));
    assert(RecompMenuResolveConfigRoot("relative","/home/Example User",id,out,sizeof(out)));
    assert(!strcmp(out,"/home/Example User/.var/app/org.snesrecomp.AckmanRecomp/config/"));
    assert(!RecompMenuResolveConfigRoot(NULL,NULL,id,out,sizeof(out)) && !out[0]);
    assert(!RecompMenuResolveConfigRoot(NULL,"relative",id,out,sizeof(out)));
    assert(!RecompMenuResolveConfigRoot(NULL,"/home/test","../bad",out,sizeof(out)));
    assert(!RecompMenuResolveConfigRoot("/long/config",NULL,id,out,4) && !out[0]);
    assert(!RecompMenuResolveConfigRoot("/config",NULL,id,NULL,0));
    SDL_Environment *env=SDL_GetEnvironment();
    bool missing=argc>1 && !strcmp(argv[1],"missing");
    bool fallback=argc>1 && !strcmp(argv[1],"fallback");
    assert(SDL_SetEnvironmentVariable(env,"XDG_CONFIG_HOME",missing || fallback?"relative":"/sandbox/config",true));
    assert(SDL_SetEnvironmentVariable(env,"HOME",missing?"":"/home/test",true));
    assert(!strcmp(RecompMenuAssetDirectory(),"/app/share/org.snesrecomp.AckmanRecomp/"));
    assert(!strcmp(RecompMenuFontDirectory(),"/app/share/org.snesrecomp.AckmanRecomp/fonts/"));
    assert(!strcmp(RecompMenuButtonDirectory(),"/app/share/org.snesrecomp.AckmanRecomp/buttons/"));
    if(missing) {
        assert(!RecompMenuWritableDirectory());
        assert(!RecompMenuPathsPrepare()); /* no fallback write in the install tree */
    } else {
        const char *expected=fallback?"/home/test/.var/app/org.snesrecomp.AckmanRecomp/config/":"/sandbox/config/";
        assert(!strcmp(RecompMenuWritableDirectory(),expected));
    }
    return 0;
}
