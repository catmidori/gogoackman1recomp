#include "ackman_translation.h"
#include "ackman_ips.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void test_ips(void) {
    unsigned char rom[32]={0};
    const unsigned char patch[]="PATCH\x00\x00\x01\x00\x02" "AB" "\x00\x00\x02\x00\x00\x00\x03" "ZEOF";
    assert(AckmanIpsApply(patch,sizeof(patch)-1,rom,sizeof(rom)));
    assert(!memcmp(rom+1,"AZZZ",4)); /* overlapping RLE wins in order */
    for(size_t size=0;size<sizeof(patch)-1;size++)assert(!AckmanIpsApply(patch,size,rom,sizeof(rom)));
    assert(!AckmanIpsApply(patch,sizeof(patch),rom,sizeof(rom))); /* trailing NUL */
    assert(!AckmanIpsApply(patch,sizeof(patch)-1,rom,3)); /* outside ROM */
    const unsigned char empty_rle[]="PATCH\0\0\1\0\0\0\0ZEOF";
    assert(!AckmanIpsApply(empty_rle,sizeof(empty_rle)-1,rom,sizeof(rom)));
    const unsigned char resizing[]="PATCHEOF\0\0\x20";
    assert(!AckmanIpsApply(resizing,sizeof(resizing)-1,rom,sizeof(rom)));
    unsigned char original[32]={0};char *table=AckmanTranslationBuildTable(original,rom,sizeof(rom));
    assert(table);assert(AckmanTranslationApplyEnglish(original,sizeof(original),table));
    assert(!memcmp(original,rom,sizeof(rom)));SDL_free(table);
    puts("PASS bounded IPS literal/RLE/overlap/EOF/bounds and guarded-table roundtrip.");
}
int main(int argc,char **argv) {
    assert(SDL_Init(0));test_ips();
    if(argc==4) {
        if(!AckmanTranslationStart(argv[1],argv[2],argv[3])){fprintf(stderr,"%s\n",AckmanTranslationError());return 2;}
        bool success=false;Uint64 end=SDL_GetTicks()+10000;
        while(AckmanTranslationBusy() && SDL_GetTicks()<end){if(AckmanTranslationPoll())success=true;SDL_Delay(1);}
        if(!success){fprintf(stderr,"%s\n",AckmanTranslationError());AckmanTranslationShutdown();return 3;}
    }
    SDL_Quit();return 0;
}
