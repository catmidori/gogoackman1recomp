#include "snes/ppu.h"
#include "snes/snes.h"
Snes *g_snes;
int snes_frame_counter;
void snes_text_xlate_on_vram_write_c(uint16_t address) { (void)address; }
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) abort(); } while(0)
uint8_t g_snesrecomp_last_hdmaen;
int main(void) {
    Ppu p; memset(&p,0,sizeof(p));
    p.hScroll[0]=123; p.hScroll[1]=321; p.vScroll[1]=17;
    p.window1left=20; p.window1right=180; p.screenEnabled[1]=2;
    g_snesrecomp_last_hdmaen=2;
    ppu_rasterBegin(&p);
    /* CPU's end-of-frame values must not leak into earlier scanlines. */
    p.hScroll[1]=999; p.window1left=99;
    ppu_rasterRecord(0x210f,80,0x56);
    ppu_rasterRecord(0x210f,80,0x01);
    ppu_rasterRecord(0x2110,80,0x34);
    ppu_rasterRecord(0x2110,80,0x02);
    ppu_rasterRecord(0x2126,81,40);
    ppu_rasterRecord(0x212d,81,4);
    ppu_rasterRecord(0x420c,82,8);
    ppu_rasterRenderBegin(&p);
    CHECK(ppu_rasterInitialHdmaen()==2);
    ppu_rasterApplyLine(&p,79);
    CHECK(p.hScroll[0]==123 && p.hScroll[1]==321 && p.window1left==20);
    ppu_rasterApplyLine(&p,80);
    CHECK(p.hScroll[1]==0x156 && p.vScroll[1]==0x234 && p.window1left==20);
    ppu_rasterApplyLine(&p,81);
    CHECK(p.window1left==40 && p.window1right==180 && p.screenEnabled[1]==4);
    uint8_t mask=0; CHECK(!ppu_rasterTakeHdmaen(&mask));
    ppu_rasterApplyLine(&p,82); CHECK(ppu_rasterTakeHdmaen(&mask) && mask==8);
    CHECK(!ppu_rasterTakeHdmaen(&mask));
    /* Rewind restores the entire earlier journal, not the future frame's
     * raster splits; loading must rearm baseline and per-line replay. */
    size_t bytes=ppu_rasterSnapshotSize();void *history=malloc(bytes);CHECK(history);
    CHECK(ppu_rasterSaveSnapshot(history,bytes));
    p.hScroll[1]=999;p.window1left=77;ppu_rasterBegin(&p);
    CHECK(!ppu_rasterLoadSnapshot(history,bytes-1));
    CHECK(ppu_rasterLoadSnapshot(history,bytes));ppu_rasterRenderBegin(&p);
    CHECK(p.hScroll[1]==321 && p.window1left==20);
    ppu_rasterApplyLine(&p,81);CHECK(p.hScroll[1]==0x156 && p.window1left==40);
    ppu_rasterApplyLine(&p,82);CHECK(ppu_rasterTakeHdmaen(&mask) && mask==8);
    free(history);
    /* Vblank updates must become baseline, including BG2 and shared latch. */
    ppu_rasterBegin(&p);
    ppu_rasterRecord(0x210f,240,0xab); ppu_rasterRecord(0x210f,240,2);
    ppu_rasterRecord(0x2126,240,11);
    ppu_rasterRenderBegin(&p);
    CHECK(p.hScroll[1]==0x2ab && p.window1left==11);
    return 0;
}
