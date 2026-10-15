#include "ackman_video.h"
#include "port_extensions.h"
#include "snes/ppu.h"
#include "desktop/config.h"
#include "snes_osd.h"
Config g_config;
static int observed_black,observed_policy;
// Flow's latch itself is covered in test_flow; this suite isolates tile safety.
int AckmanFlowVideoPolicy(int requested) { observed_policy=requested;return requested; }
void AckmanFlowVideoEndpoint(bool black) { observed_black=black; }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
unsigned char g_ram[0x20000];
const unsigned char *g_rom;
void AckmanFlowShade(uint8_t *p,size_t pitch,int w,int h) { (void)p; (void)pitch; (void)w; (void)h; }
static Ppu ppu;
Ppu *g_ppu=&ppu;
static AckmanWorldMargins query;
static unsigned char pixels[480*224*4];
void AckmanRegisterWorldMargins(AckmanWorldMargins f) { query=f; }
void PpuSetExtraSideSpace(Ppu *p,int l,int r,int b) { (void)b; p->extraLeftCur=l; p->extraRightCur=r; }
void PpuSetWidescreenLayerClamp(Ppu *p,uint8_t v) { p->wsLayerClamp=v; }
void PpuSetWidescreenLayerRepeat(Ppu *p,uint8_t v) { p->wsLayerRepeat=v; }
void PpuSetWidescreenLayerRepeatBand(Ppu *p,uint8_t layer,uint8_t y0,uint8_t y1) {
    p->wsRepeatY0[layer]=y0;p->wsRepeatY1[layer]=y1;
}
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static void word(unsigned a,unsigned v) { g_ram[a]=v; g_ram[a+1]=v>>8; }
static void query_margins(int requested,int *l,int *r) {
    ppu.extraLeftRight=requested;
    AckmanVideoBegin(); *l=ppu.extraLeftCur; *r=ppu.extraRightCur; AckmanVideoEnd();
}
static void check_osd_scale(void) {
    // A fully opaque panel makes every edge measurable, independent of font
    // timing. Check both top-left status and right-centred volume anchoring.
    uint32_t solid[40*20];for(int i=0;i<40*20;i++)solid[i]=0xffffffff;
    for(int mode=0;mode<3;mode++) {
    const uint32_t *panel=solid;int pw=40,ph=20;
    if(mode) {
        snes_osd_set_fps_visible(1);snes_osd_set_turbo(mode==2);
        CHECK(snes_osd_image(&panel,&pw,&ph));
    }
    for(int scale=1;scale<=10;scale++)for(int volume=0;volume<=1;volume++) {
        int width=448*scale,height=224*scale,pitch=(width+3)*4;
        uint8_t *frame=calloc((size_t)pitch*height+16,1);CHECK(frame);
        memset(frame+(size_t)pitch*height,0xa5,16);
        AckmanVideoBlitOsd(frame,pitch,width,height,panel,pw,ph,2,scale,volume!=0);
        int min_x=width,min_y=height,max_x=-1,max_y=-1,count=0;
        for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
            uint32_t pixel;memcpy(&pixel,frame+(size_t)y*pitch+x*4,4);
            if(pixel) {if(!mode)CHECK(pixel==0xffffffff);if(x<min_x)min_x=x;if(y<min_y)min_y=y;
                if(x>max_x)max_x=x;if(y>max_y)max_y=y;count++;}
        }
        CHECK(max_x-min_x+1==(pw/2)*scale && max_y-min_y+1==(ph/2)*scale);
        CHECK(count==(pw/2)*(ph/2)*scale*scale);
        CHECK(min_x==(volume?448-pw/2-2:2)*scale);
        CHECK(min_y==(volume?(224-ph/2)/2:2)*scale);
        for(int y=0;y<height;y++)for(int x=width*4;x<pitch;x++)CHECK(frame[(size_t)y*pitch+x]==0);
        for(int i=0;i<16;i++)CHECK(frame[(size_t)pitch*height+i]==0xa5);
        free(frame);
    }
    }
    snes_osd_set_fps_visible(0);snes_osd_set_turbo(0);
}
int main(void) {
    check_osd_scale();
    // Native charging and cinematic geometry stay independent of viewport PAR.
    memset(g_ram,0,sizeof(g_ram));g_ram[0x43a]=0x80;g_ram[0xb3a]=1;
    assert(!AckmanVideoRumbleCharging());g_ram[0xb3a]=0;g_ram[0x414]=1;g_ram[0x448]=0x13;assert(AckmanVideoRumbleCharging());
    g_ram[0x448]=0x14;assert(AckmanVideoRumbleCharging());g_ram[0x491]=2;assert(!AckmanVideoRumbleCharging());
    g_ram[0x491]=0;g_ram[0x448]=0x12;assert(!AckmanVideoRumbleCharging());g_ram[0x448]=0x13;
    g_ram[0x414]=0x80;assert(!AckmanVideoRumbleCharging());g_ram[0x414]=1;g_ram[0x491]=0x80;assert(!AckmanVideoRumbleCharging());
    memset(g_ram,0,sizeof(g_ram));word(0x1126,0xa34d);word(0x1124,0xd5);
    word(0x2222,4);word(0x2212,0x100);word(0x2224,0x1b0);word(0x2226,0x177);word(0x2206,0x175);
    word(0x2204,0x1a7);assert(!AckmanVideoRumbleInterludeContact());word(0x2204,0x1a8);assert(AckmanVideoRumbleInterludeContact());
    word(0x1124,0xef);assert(!AckmanVideoRumbleInterludeContact());
    word(0x400,0x2800);word(0x1126,0xa22e);word(0x1124,0x1c);assert(AckmanVideoRumbleCastleIgnition());
    ppu.inidisp=0x80;word(0x1124,0x5c);assert(!AckmanVideoRumbleCastleIgnition() && AckmanVideoRumbleScene()==ACK_RUMBLE_CASTLE);
    word(0x1124,0x89);assert(AckmanVideoRumbleScene()==ACK_RUMBLE_NONE);
    ppu.inidisp=15;assert(AckmanVideoRumbleScene()==ACK_RUMBLE_CASTLE);
    word(0x400,0x2900);assert(AckmanVideoRumbleScene()==ACK_RUMBLE_NONE);
    word(0x1126,0xa34d);assert(AckmanVideoRumbleScene()==ACK_RUMBLE_NONE);
    word(0x400,0x2d00);word(0x1126,0x9be4);word(0x1124,0x1e);assert(!AckmanVideoRumbleClimaxFlash());
    word(0x1124,0x1f);assert(AckmanVideoRumbleClimaxFlash() && AckmanVideoRumbleScene()==ACK_RUMBLE_END_CLIMAX);
    memset(g_ram,0,sizeof(g_ram));ppu.inidisp=0;

    AckmanVideoInstall();
    g_ram[0x274]=0x39; g_ram[0x276]=0x50;
    word(0x40c,0x2800); word(0x416,0xf0); word(0x41a,0x1f0);
    ppu.extraLeftRight=64; ppu.renderBuffer=pixels; ppu.renderPitch=384*4;
    for(unsigned a=0x4000;a<0xe000;a+=2) word(a,a/2);
    for(int y=511;y<=513;y++) {
        word(0x402,256); word(0x406,y);
        for(unsigned i=0;i<0x8000;i++) ppu.vram[i]=0xbeef;
        memset(pixels,0x7f,sizeof(pixels));
        int l,r; query_margins(64,&l,&r); CHECK(l==0 && r==64);
        AckmanVideoBegin();
        /* Every native column remains owned by the guest during a jump. */
        int sy=(y-0x1f0-1)/16;
        int rows=((y-1)%16+224+15)/16;
        for(int row=0;row<rows;row++) for(int col=0;col<16;col++)
            CHECK(ppu.vram[0x5000+((sy+row)&31)*32+((1+col)&31)]==0xbeef);
        /* First exterior tile follows the vertical -1 even at Y=$0200. */
        int wy=(y-1)/16-16;
        unsigned source=0x4000+(wy/16)*0x2800+512+(wy%16)*32;
        CHECK(ppu.vram[0x5000+(sy&31)*32+17]==source/2);
        AckmanVideoEnd();
        for(unsigned i=0;i<0x8000;i++) CHECK(ppu.vram[i]==0xbeef);
        for(int row=0;row<224;row++) {
            for(int x=0;x<64*4;x++) CHECK(pixels[row*384*4+x]==0);
            CHECK(pixels[row*384*4+64*4]==0x7f);
        }
    }
    int l,r;
    word(0x402,272); query_margins(64,&l,&r); CHECK(l==16 && r==64);
    word(0x402,5120); query_margins(64,&l,&r); CHECK(l==64 && r==0);
    g_ram[0x274]=7; query_margins(64,&l,&r); CHECK(l==0 && r==0);
    g_ram[0x274]=0x39; word(0x40c,0x6000); word(0x402,386); word(0x406,272);
    query_margins(64,&l,&r); CHECK(l==64 && r==64);
    word(0x1124,0x57); word(0x1126,0x9447); query_margins(64,&l,&r); CHECK(l==0 && r==0);
    word(0x1126,0xbb63); query_margins(64,&l,&r); CHECK(l==64 && r==64);
    word(0x400,0x0200); word(0x1126,0xabd9); word(0x40c,0x2800); word(0x406,256);
    query_margins(64,&l,&r); CHECK(l==64 && r==64);
    word(0x400,0x0100); word(0x1126,0xa483); word(0x406,272);
    word(0x40c,0x0600); word(0x402,300);
    query_margins(64,&l,&r); CHECK(l==44 && r==64);
    word(0x40c,0); g_ram[0x274]=0x79;
    query_margins(64,&l,&r); CHECK(l==64 && r==64);
    /* ROM camera bounds, independent of padded map allocation. */
    static unsigned char rom[32768]; g_rom=rom;
    rom[0x302c]=0; rom[0x302d]=0xc0; /* selector 02 -> C000 */
    for(int i=0;i<128;i+=2) { rom[0x4000+i]=0; rom[0x4001+i]=0xc1; }
    rom[0x4100]=0xf0; rom[0x4101]=0x14; /* right=5360 inclusive */
    rom[0x4102]=0x10; rom[0x4103]=1; /* left=272 */
    word(0x400,0x0200); word(0x1126,0xa483); word(0x40c,0x2800);
    g_ram[0x274]=0x39; word(0x402,272); word(0x406,513);
    query_margins(64,&l,&r); CHECK(l==0 && r==64);
    word(0x402,5105); query_margins(64,&l,&r); CHECK(l==64 && r==0);
    word(0x400,0x1100); word(0x1126,0xaa11);
    query_margins(64,&l,&r); CHECK(l==0 && r==0);
    word(0x400,0); word(0x1126,0x9447); word(0x1124,0x37);
    query_margins(64,&l,&r); CHECK(l==64 && r==64);
    word(0x1124,0x57); query_margins(64,&l,&r); CHECK(l==0 && r==0);
    word(0x1126,0xa483); word(0x40c,0);
    query_margins(64,&l,&r); CHECK(l==0 && r==0);
    g_rom=NULL; word(0x402,320); word(0x406,272);
    unsigned strides[]={0x800,0x1000,0x1200,0x1800,0x1c00,0x2000,0x2c00};
    for(unsigned i=0;i<sizeof(strides)/sizeof(*strides);i++) {
        word(0x40c,strides[i]); query_margins(64,&l,&r); CHECK(l==64 && r==64);
    }
    /* Road: native columns survive every HDMA scroll, with full restoration. */
    word(0x400,0x2500); word(0x1126,0xa56e);
    for(unsigned i=0;i<0x8000;i++) ppu.vram[i]=(uint16_t)i;
    ppu.extraLeftRight=112;ppu.renderPitch=480*4;
    for(int scroll=0;scroll<1024;scroll++) {
        ppu.hScroll[0]=scroll; ppu.vScroll[0]=scroll/2;
        AckmanVideoBegin();
        for(int line=1;line<=224;line++) {
            AckmanVideoScanlineBegin(line);
            unsigned row=((ppu.vScroll[0]+line)/16)&31;
            int sx=scroll/16, cols=((scroll&15)+271)/16;
            for(int x=0;x<cols;x++) {
                unsigned a=0x5000+row*32+((sx+x)&31); CHECK(ppu.vram[a]==a);
            }
            AckmanVideoScanlineEnd();
        }
        AckmanVideoEnd();
        for(unsigned i=0;i<0x8000;i++) CHECK(ppu.vram[i]==i);
    }
    CHECK(AckmanVideoViewportExtra(64)==64); /* mask never resizes viewport */
    /* Repeated live switches preserve guest VRAM, HDMA scroll and native HUD.
     * Geometry switches at the next presentation, without stretching 4:3. */
    for(int i=0;i<500;i++) {
        AckmanVideoSetWidescreen(i&1);
        int fw,fh;SnesDisplayViewport viewport;
        AckmanVideoFrameSize(1920,1080,&fw,&fh);
        CHECK(fh==224 && fw==((i&1)?342:256));
        AckmanVideoComputeViewport(fw,fh,1920,1080,&viewport);
        if(!(i&1)) {
            CHECK(viewport.x==240 && viewport.y==0);
            CHECK(viewport.width==1440 && viewport.height==1080);
            CHECK(AckmanVideoViewportExtra(64)==0);
        } else CHECK(viewport.width>1900 && viewport.height==1080);
        ppu.extraLeftRight=(fw-256)/2;
        unsigned hs=ppu.hScroll[0],vs=ppu.vScroll[0];
        AckmanVideoBegin();
        for(int line=1;line<=224;line++) {
            AckmanVideoScanlineBegin(line);AckmanVideoScanlineEnd();
        }
        AckmanVideoEnd();
        CHECK(ppu.hScroll[0]==hs && ppu.vScroll[0]==vs);
        for(unsigned a=0;a<0x8000;a++) CHECK(ppu.vram[a]==a);
    }
    AckmanVideoSetWidescreen(0);
    {   /* Original mode overrides any stale widened frame and alternate PAR. */
        SnesDisplayViewport v;int fw,fh;
        AckmanVideoComputeViewport(384,224,1280,720,&v);
        CHECK(v.x==160 && v.y==0 && v.width==960 && v.height==720);
        AckmanVideoFrameSize(1344,672,&fw,&fh);CHECK(fw==256 && fh==224);
    }
    AckmanVideoSetWidescreen(1);
    { int fw,fh;AckmanVideoFrameSize(1344,672,&fw,&fh);CHECK(fw==384 && fh==224);
      AckmanVideoFrameSize(640,800,&fw,&fh);CHECK(fw==256 && fh==224); }
    for(int i=0;i<500;i++) {
        g_config.display_aspect=i&1;
        AckmanVideoSetWidescreen(0);
        SnesDisplayViewport v;int fw,fh;
        AckmanVideoComputeViewport(384,224,1280,720,&v);
        CHECK(v.height==720 && v.width==((i&1)?823:960));
        CHECK(v.x==(1280-v.width)/2 && v.y==0);
        AckmanVideoSetWidescreen(1);
        AckmanVideoFrameSize(1280,720,&fw,&fh);
        CHECK(fh==224 && fw==((i&1)?400:342));
        AckmanVideoComputeViewport(fw,fh,1280,720,&v);
        CHECK(v.height>=720 && v.width>=1280);
        CHECK(v.x==(1280-v.width)/2);
        // Square pixels need a wider raw field at the same target aspect.
        AckmanVideoFrameSize(960,720,&fw,&fh);CHECK(fw==((i&1)?300:256));
    }

    g_config.integer_scaling=true;
    for(int aspect=0;aspect<2;aspect++)for(int wide=0;wide<2;wide++) {
        g_config.display_aspect=aspect;AckmanVideoSetWidescreen(wide);
        int fw,fh;SnesDisplayViewport v;AckmanVideoFrameSize(1280,720,&fw,&fh);
        AckmanVideoComputeViewport(fw,fh,1280,720,&v);
        CHECK(v.height%224==0 && v.height>=224 && v.height<=720);
        CHECK(v.width<=1280 && v.x==(1280-v.width)/2 && v.y==(720-v.height)/2);
        AckmanVideoComputeViewport(fw,fh,200,100,&v);CHECK(v.height==224 && v.y<0);
    }
    g_config.integer_scaling=false;AckmanVideoSetWidescreen(1);
    CHECK(AckmanVideoViewportExtra(1000)==112);
    // Full physical width at both PARs, including default 2:1 and ultrawide.
    for(int aspect=0;aspect<2;aspect++) {
        g_config.display_aspect=aspect;
        const int sizes[][2]={{1344,672},{1920,1080},{2560,1080},{3840,1080}};
        for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++) {
            int fw,fh;SnesDisplayViewport v;
            AckmanVideoFrameSize(sizes[i][0],sizes[i][1],&fw,&fh);
            CHECK(fw<=480 && fw>=256);
            AckmanVideoComputeViewport(fw,fh,sizes[i][0],sizes[i][1],&v);
            CHECK(v.x<=0 && v.y<=0);
            CHECK(v.x+v.width>=sizes[i][0] && v.y+v.height>=sizes[i][1]);
        }
    }
    // Expanded mapped borders must not wrap over native tiles or leave VRAM changes.
    word(0x400,0);word(0x1126,0);word(0x40c,0x2800);
    g_ram[0x274]=0x39;g_ram[0x276]=0x50;
    word(0x416,0xf0);word(0x41a,0x1f0);word(0x406,513);
    for(int offset=0;offset<16;offset++) {
        word(0x402,512+offset);
        for(unsigned a=0;a<0x8000;a++) ppu.vram[a]=0xbeef;
        query_margins(112,&l,&r);CHECK(l==112 && r==112);
        AckmanVideoBegin();
        int sx=(512+offset-0xf0)/16,sy=(513-1-0x1f0)/16;
        int cols=(offset+256+15)/16;
        for(int y=0;y<14;y++)for(int x=0;x<cols;x++)
            CHECK(ppu.vram[0x5000+((sy+y)&31)*32+((sx+x)&31)]==0xbeef);
        AckmanVideoEnd();
        for(unsigned a=0;a<0x8000;a++) CHECK(ppu.vram[a]==0xbeef);
    }
    word(0x402,256);query_margins(112,&l,&r);CHECK(l==0 && r==112);
    word(0x402,5120);query_margins(112,&l,&r);CHECK(l==112 && r==0);
    // Reproduce the saved sky signature. Test every scroll phase and live PAR
    // changes: only exterior moon quads change, and all VRAM is restored.
    word(0x400,0x2a00);word(0x1126,0xa22e);
    ppu.bgmode=0x39;ppu.bgXsc[1]=0x54;
    ppu.vram[0x5400+4*32+11]=0x10a8;
    const unsigned moon[]={0x04a0,0x04a2,0x04a4,0x04a6};
    for(int col=11;col<=27;col+=16)for(int t=0;t<4;t++)
        ppu.vram[0x5400+(2+t/2)*32+col+t%2]=moon[t];
    for(int phase=0;phase<512;phase++)for(int aspect=0;aspect<2;aspect++) {
        g_config.display_aspect=aspect;ppu.hScroll[1]=phase;
        AckmanVideoBegin();AckmanVideoScanlineBegin(40);
        for(int col=11;col<=27;col+=16) {
            int x=(col*16-phase+512)&511;
            int exterior=x>=256 && x<=480;
            for(int t=0;t<4;t++)CHECK(ppu.vram[0x5400+(2+t/2)*32+col+t%2]==
                (exterior?0x10a8:moon[t]));
        }
        AckmanVideoScanlineEnd();AckmanVideoEnd();
        for(int col=11;col<=27;col+=16)for(int t=0;t<4;t++)
            CHECK(ppu.vram[0x5400+(2+t/2)*32+col+t%2]==moon[t]);
    }
    // A nearby scene and a changed tile signature must not trigger the exception.
    g_config.display_aspect=1;ppu.hScroll[1]=240;word(0x400,0x2b00);
    AckmanVideoBegin();AckmanVideoScanlineBegin(40);
    CHECK(ppu.vram[0x5400+2*32+11]==0x04a0);
    AckmanVideoScanlineEnd();AckmanVideoEnd();
    word(0x400,0x2a00);ppu.vram[0x5400+2*32+12]=0x1234;
    AckmanVideoBegin();AckmanVideoScanlineBegin(40);
    CHECK(ppu.vram[0x5400+2*32+11]==0x04a0);
    AckmanVideoScanlineEnd();AckmanVideoEnd();
    // Forced blank in a register cannot release the latch if rendered pixels
    // still exist; a composited all-black frame is the only release signal.
    ppu.inidisp=0x80;memset(pixels,0x7f,sizeof(pixels));
    AckmanVideoBegin();AckmanVideoEnd();CHECK(!observed_black);
    memset(pixels,0,sizeof(pixels));ppu.inidisp=15;
    AckmanVideoBegin();AckmanVideoEnd();CHECK(observed_black);
    // Save-derived scripts: exact story selector/script pairs pillarbox, while every
    // $A34D interlude shares background-only rendering, irrespective of stride.
    word(0x400,0x0600);word(0x1126,0xabd9);query_margins(96,&l,&r);CHECK(!l && !r);
    word(0x400,0x1100);word(0x1126,0xa964);query_margins(96,&l,&r);CHECK(!l && !r);
    // Stage-5 phase-3 cinematic: mask every exterior pixel, preserving the
    // guest letterbox and central picture byte-for-byte at both pixel aspects.
    for(int aspect=0;aspect<2;aspect++) {
        g_config.display_aspect=aspect;
        word(0x400,0x2800);word(0x1126,0xa1d9);
        ppu.extraLeftRight=96;ppu.renderPitch=448*4;
        memset(pixels,0x7f,sizeof(pixels));
        for(int y=0;y<224;y++)if(y<32 || y>=192)
            memset(pixels+y*448*4+96*4,0,256*4);
        AckmanVideoBegin();CHECK(observed_policy==0);
        CHECK(!ppu.extraLeftCur && !ppu.extraRightCur);
        AckmanVideoEnd();
        for(int y=0;y<224;y++)for(int x=0;x<448;x++)for(int c=0;c<4;c++)
            CHECK(pixels[(y*448+x)*4+c]==
                (x<96 || x>=352 || y<32 || y>=192 ? 0 : 0x7f));
        // Same script on another selector, and the successor gameplay script,
        // must not inherit the cinematic exception.
        word(0x400,0x2900);query_margins(96,&l,&r);CHECK(observed_policy==1 && l==96 && r==96);
        word(0x400,0x2800);word(0x1126,0xa22e);
        query_margins(96,&l,&r);CHECK(observed_policy==-1);
    }
    unsigned selectors[]={0,0x0600,0x1100,0x1800,0x2400};
    for(unsigned i=0;i<5;i++) {
        word(0x400,selectors[i]);word(0x1126,0xa34d);ppu.wsLayerClamp=0;
        AckmanVideoBegin();CHECK(ppu.extraLeftCur==96 && ppu.extraRightCur==96);
        CHECK(ppu.wsLayerClamp==0x15 && ppu.wsLayerRepeat==2);
        AckmanVideoEnd();CHECK(ppu.wsLayerClamp==0);
    }
    word(0x400,0x2d00);word(0x1126,0xac82);
    AckmanVideoBegin();CHECK(ppu.wsLayerClamp==0x14 && ppu.wsLayerRepeat==3);AckmanVideoEnd();
    // Castle OAM composition must extend, while native label/text BGs do not.
    unsigned castle_scripts[]={0x9a4a,0x9ab9,0x9af0,0x9b2f,0x9bdc,0x9be4};
    for(unsigned i=0;i<sizeof(castle_scripts)/sizeof(*castle_scripts);i++) {
        word(0x1126,castle_scripts[i]);AckmanVideoBegin();
        CHECK(ppu.wsLayerClamp==0x05 && ppu.extraRightCur==96);AckmanVideoEnd();
    }
    word(0x1126,0x9d0d);g_ram[0x274]=0x79;ppu.bgmode=0x79;
    ppu.hScroll[1]=240;ppu.vram[0x5400+2*32+12]=0x04a2;
    for(int aspect=0;aspect<2;aspect++) {
        g_config.display_aspect=aspect;
        AckmanVideoBegin();AckmanVideoScanlineBegin(40);
        CHECK(ppu.wsLayerClamp==0x15 && ppu.extraLeftCur==96);
        CHECK(ppu.vram[0x5400+2*32+11]==0x10a8);
        CHECK(ppu.vram[0x5400+2*32+27]==0x04a0);
        AckmanVideoScanlineEnd();AckmanVideoEnd();
        CHECK(ppu.vram[0x5400+2*32+11]==0x04a0);
    }
    word(0x1126,0x9f08);AckmanVideoBegin();
    CHECK(ppu.wsLayerClamp==0x14 && ppu.wsLayerRepeat==3);AckmanVideoEnd();
    // The same script uses two layer compositions; only landscape repeats BG2.
    word(0x1126,0xb523);g_ram[0x274]=0x79;AckmanVideoBegin();
    CHECK(ppu.wsLayerClamp==0x14 && ppu.wsLayerRepeat==3);AckmanVideoEnd();
    g_ram[0x274]=0x39;word(0x1126,0xb523);AckmanVideoBegin();
    CHECK(ppu.wsLayerClamp==0x17 && ppu.wsRepeatY0[0]==64 && ppu.wsRepeatY1[0]==144);
    AckmanVideoEnd();
    // Ending-only script IDs must not open unrelated stage dialogue borders.
    word(0x400,0x1100);word(0x1126,0x9d0d);query_margins(96,&l,&r);CHECK(observed_policy==-1);
    word(0x1126,0x9f08);query_margins(96,&l,&r);CHECK(observed_policy==-1);
    return 0;
}
