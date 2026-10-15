#include "snes_rewind.h"
#include "desktop/config.h"
#include "snes/ppu.h"
#include "ackman_flow.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
Config g_config;
static uint32_t frame,journal,presentation[5];
size_t RtlSaveSnapshotToMemory(void *out,size_t size){if(size<4)return 0;memcpy(out,&frame,4);return 4;}
bool RtlLoadSnapshotFromMemory(const void *in,size_t size){if(size!=4)return false;memcpy(&frame,in,4);return true;}
size_t ppu_rasterSnapshotSize(void){return 4;}
bool ppu_rasterSaveSnapshot(void *p,size_t n){if(n!=4)return false;memcpy(p,&journal,4);return true;}
bool ppu_rasterLoadSnapshot(const void *p,size_t n){if(n!=4)return false;memcpy(&journal,p,4);return true;}
void AckmanFlowSaveRewind(uint32_t p[5]){memcpy(p,presentation,sizeof(presentation));}
void AckmanFlowLoadRewind(const uint32_t p[5]){memcpy(presentation,p,sizeof(presentation));}
static void record(uint32_t n){frame=n;journal=n+100;presentation[0]=n;snes_rewind_note_frame();}
int main(void) {
    g_config.rewind_depth=4;
    for(int interval=2;interval<=6;interval+=2) {
        g_config.rewind_interval=interval;snes_rewind_configure();assert(!snes_rewind_open());
        for(int i=1;i<=interval*8;i++)record(i);
        snes_rewind_configure();assert(snes_rewind_open());
        for(int i=0;i<4;i++) {
            assert(snes_rewind_tick());uint32_t expected=interval*(8-i);
            assert(frame==expected && journal==expected+100 && presentation[0]==expected);
            frame+=1000;journal=0;assert(snes_rewind_restore_current());assert(frame==expected && journal==expected+100);
        }
        assert(!snes_rewind_tick() && frame==(uint32_t)interval*5); /* clamp oldest */
        snes_rewind_commit();assert(!snes_rewind_is_open() && !snes_rewind_open());
        for(int i=1;i<=interval*3;i++)record(interval*5+i);
        assert(snes_rewind_open());assert(snes_rewind_tick());assert(snes_rewind_tick());
        assert(frame==(uint32_t)interval*7);snes_rewind_commit();
        for(int i=1;i<=interval;i++)record(interval*7+i);
        assert(snes_rewind_open());assert(snes_rewind_tick() && frame==(uint32_t)interval*8);
        assert(snes_rewind_tick() && frame==(uint32_t)interval*7);snes_rewind_close();
        const uint32_t *pixels=(const uint32_t *)1;int w=1,h=1;
        assert(!snes_rewind_overlay_image(&pixels,&w,&h) && !pixels && !w && !h);
        snes_rewind_shutdown();
    }
    puts("PASS hold ring: cadence 2/4/6, wrap, oldest clamp, exact redraw restore, future trim, no thumbnails.");
    return 0;
}
