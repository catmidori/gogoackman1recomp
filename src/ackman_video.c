/* Experimental Mode-1 world fill, derived from $80:AA3F and $80:ABD0.
 * A 256x256 world block contains 16x16 little-endian 16px tile words.
 * Changes are render-only; the guest camera and its VRAM queue stay intact. */
#include "ackman_video.h"
#include "ackman_flow.h"
#include "port_extensions.h"
#include "snes/ppu.h"
#include "desktop/config.h"
#include "snes_overlay_draw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
extern unsigned char g_ram[0x20000];
extern Ppu *g_ppu;
extern const unsigned char *g_rom;
/* A 32-column ring must retain up to 17 native columns plus both borders.
 * Seven 16px columns per side leave one spare column at every scroll phase.
 * Raising this beyond 112 requires a separate render-only tilemap, not VRAM wrap.
 */
enum { kAckmanExtra = 112, kAckmanBorderTiles = kAckmanExtra/16 };
_Static_assert(kAckmanExtra <= kPpuExtraLeftRight, "PPU border capacity");
static uint16_t saved[512], saved_address[512];
static unsigned saved_count;
static uint8_t saved_clamp;
static int patched, enabled, force, road, unique_sky;
/* Host presentation preference, separate from the experimental safety gate.
 * Never reset this on a guest reset or savestate load. */
static int widescreen=1;
void AckmanVideoSetWidescreen(int active) { widescreen=active!=0; }
int AckmanVideoInternalScale(void) {
    return g_config.window_scale<1?1:g_config.window_scale>10?10:g_config.window_scale;
}
void AckmanVideoBlitOsd(uint8_t *dst,int pitch,int width,int height,const uint32_t *panel,
                        int panel_width,int panel_height,int scale_div,int render_scale,bool volume) {
    if(scale_div<1 || render_scale<1 || render_scale>10)return;
    // Preserve the 1x OSD proportions and native-pixel margins in the enlarged
    // framebuffer. Both SDL and OpenGL consume this same composed image.
    int w=(panel_width/scale_div)*render_scale,h=(panel_height/scale_div)*render_scale;
    int margin=(4/scale_div)*render_scale;
    int x=volume?width-w-margin:margin;
    int y=volume?((height/render_scale-panel_height/scale_div)/2)*render_scale:margin;
    snes_ovl_blit_panel_rect(dst,pitch,width,height,panel,panel_width,panel_height,x,y,w,h);
}
int AckmanVideoWidescreen(void) { return enabled && widescreen; }
void AckmanVideoFrameSize(int dw,int dh,int *fw,int *fh) {
    int num,den;
    SnesDisplayAspect_GetPixelAspect(SnesDisplayAspect_Clamp(g_config.display_aspect),&num,&den);
    int64_t divisor=(int64_t)dh*num;
    int64_t requested=(int64_t)dw*224*den;
    int64_t width=dh>0 && dw>0 ? (requested+divisor-1)/divisor : 256;
    if(width<256)width=256;
    if(width>256+2*kAckmanExtra)width=256+2*kAckmanExtra;
    width=(width+1)&~1;
    *fw=256+2*AckmanVideoViewportExtra((width-256)/2);
    *fh=224;
}
void AckmanVideoComputeViewport(int fw,int fh,int dw,int dh,SnesDisplayViewport *viewport) {
    if(!viewport) return;
    /* IgnoreAspectRatio selects adaptive expansion in this port, never stretch.
     * Pixel aspect also determines the adaptive extra-column budget. */
    if(!AckmanVideoWidescreen()) { fw=256; fh=224; }
    if(g_config.integer_scaling && fw>0 && fh>0 && dw>0 && dh>0) {
        int num,den;SnesDisplayAspect_GetPixelAspect(SnesDisplayAspect_Clamp(g_config.display_aspect),&num,&den);
        int sy=dh/fh,sx=(int)((int64_t)dw*den/((int64_t)fw*num));
        int scale=sx<sy?sx:sy;if(scale<1)scale=1; /* No fractional downscale: crop if smaller than native. */
        viewport->width=(int)(((int64_t)fw*num*scale+den/2)/den);
        viewport->height=fh*scale;viewport->x=(dw-viewport->width)/2;viewport->y=(dh-viewport->height)/2;return;
    }
    else if(AckmanVideoWidescreen() && fw>256 && fh>0 && dw>0 && dh>0) {
        int num,den;
        SnesDisplayAspect_GetPixelAspect(SnesDisplayAspect_Clamp(g_config.display_aspect),&num,&den);
        /* Cover rather than contain: integer PPU columns may overshoot by a
         * fraction of a column. Crop that surplus, preserving pixel aspect.
         * Very wide windows crop vertically at the safe tilemap budget instead
         * of stretching pixels or reading beyond the authored map.
         */
        if((int64_t)dw*fh*den >= (int64_t)dh*fw*num) {
            viewport->width=dw;
            viewport->height=(int)(((int64_t)dw*fh*den+fw*num-1)/(fw*num));
        } else {
            viewport->height=dh;
            viewport->width=(int)(((int64_t)dh*fw*num+fh*den-1)/(fh*den));
        }
        viewport->x=(dw-viewport->width)/2;
        viewport->y=(dh-viewport->height)/2;
        return;
    }
    SnesDisplayAspect_ComputeViewport(fw,fh,dw,dh,SnesDisplayAspect_Clamp(g_config.display_aspect),false,false,viewport);
}
static int visible_left, visible_right,frame_policy;
static FILE *layout_trace,*fade_trace;
static unsigned video_frame;
static unsigned word(unsigned a) { return g_ram[a]|(unsigned)g_ram[a+1]<<8; }
static int floor16(int x) { return x>=0 ? x/16 : -((-x+15)/16); }
typedef struct View { int x,y,sx,sy,columns,rows; unsigned stride; } View;
static View view;
static int tile(int x,int y,unsigned *value) {
    if(x<0 || y<0 || x>=(int)(view.stride/512)*16) return 0;
    unsigned a=0x4000+(y/16)*view.stride+(x/16)*512+(y%16)*32+(x%16)*2;
    if(a>=0xe000) return 0;
    *value=word(a); return 1;
}
static int mapped(void) {
    if(!AckmanVideoWidescreen() || !g_ppu || (g_ram[0x274]&0x3f)!=0x39 || g_ram[0x276]!=0x50 ||
       (word(0x40c)<0x0600 || word(0x40c)>0x6000 || (word(0x40c)&0x1ff)) || word(0x402)<256 || word(0x406)<257) return 0;
    int cx=word(0x402), cy=word(0x406)-1;
    /* The same -1 applies to world Y and to the hardware scroll. Never
     * replace the guest's central columns when crossing a 16px boundary. */
    view=(View){floor16(cx)-16,floor16(cy)-16,
        floor16(cx-word(0x416)),floor16(cy-word(0x41a)),
        ((cx&15)+256+15)/16,((cy&15)+224+15)/16,word(0x40c)};
    if((word(0x416)|word(0x41a))&15) return 0;
    if(force) return 1;
    for(int y=0;y<view.rows;y++) for(int x=0;x<view.columns;x++) {
        unsigned value;
        if(!tile(view.x+x,view.y+y,&value) ||
           g_ppu->vram[0x5000+((view.sy+y)&31)*32+((view.sx+x)&31)]!=value) return 0;
    }
    return 1;
}
int AckmanVideoViewportExtra(int requested) {
    return AckmanVideoWidescreen() ? (requested>kAckmanExtra?kAckmanExtra:requested<0?0:requested) : 0;
}
/* The map addresses increase with X and Y. The last visible row gives
 * the tightest allocation bound; compute it once instead of probing every
 * pixel and then every tile in each column. No cache of mutable map data. */
static int region_bounds(int *left,int *right);
static void calculate_margins(int requested,int *left,int *right) {
    int last_y=view.y+view.rows-1;
    unsigned row=0x4000+(last_y/16)*view.stride+(last_y%16)*32;
    *left=*right=0;
    if(row>=0xe000) return;
    unsigned bytes=0xe000-row;
    unsigned tail=(bytes%512)/2;
    unsigned columns=(bytes/512)*16+(tail>16?16:tail);
    unsigned map_columns=view.stride/512*16;
    if(columns>map_columns) columns=map_columns;
    int origin=(int)word(0x402)-256;
    if(origin<0 || origin+256>(int)columns*16) return;
    requested=AckmanVideoViewportExtra(requested);
    *left=origin<requested?origin:requested;
    int remaining=(int)columns*16-origin-256;
    *right=remaining<requested?remaining:requested;
    int low,high;
    if(region_bounds(&low,&high)) {
        int l=(int)word(0x402)-low, r=high-(int)word(0x402)-256;
        if(l<*left) *left=l>0?l:0;
        if(r<*right) *right=r>0?r:0;
    }
}
enum { SCENE_NATIVE=0, SCENE_WRAP=1, SCENE_ROAD=2,
       SCENE_DIALOGUE=3, SCENE_FINAL=4, SCENE_CREDITS=5, SCENE_END=6,
       SCENE_END_LANDSCAPE=7, SCENE_EPILOGUE=8 };
static int scene_policy(void) {
    unsigned selector=word(0x400),script=word(0x1126);
    // Match scripts AND selectors for story exceptions: playable stages reuse
    // the selectors, while all five Ackman/Angel interludes share $A34D.
    /* $2800/$A1D9 is the stage-5 phase-3 letterboxed castle story.
     * Its finite sky map has no authored side columns. Mask the whole native
     * composition (including OBJ/backdrop), leaving its raster letterbox intact.
     * $2800/$A22E is the following gameplay and must keep adaptive rendering. */
    if((selector==0x0600 && script==0xabd9) || (selector==0x1100 && script==0xa964) ||
       (selector==0x2800 && script==0xa1d9)) return SCENE_NATIVE;
    if(script==0xa34d) return SCENE_DIALOGUE;
    if(selector==0x2d00) {
        // Save/replay-derived ending blocks only; never broaden global dialogue.
        switch(script) {
            case 0x9a4a: case 0x9ab9: case 0x9af0:
            case 0x9b2f: case 0x9bdc: case 0x9be4: return SCENE_FINAL;
            case 0x9d0d: return SCENE_EPILOGUE;
            case 0x9f08: case 0xac82: return SCENE_CREDITS;
            // B523 contains both the landscape conversation and THE END.
            // BG3's tile-size bit distinguishes these saved compositions.
            case 0xb523: return g_ram[0x274]==0x79 ? SCENE_END_LANDSCAPE : SCENE_END;
        }
    }
    if(selector==0 && script==0x9447) return word(0x1124)<=0x37 ? 1 : 0;
    if(selector==0 && script==0xbb63) return 1;
    if(selector==0 && script==0xa483 && !word(0x40c)) return 0;
    if(selector==0x1100 && script==0xaa11) return 0; /* stage 2 boss */
    if(selector==0x2d00 && script==0x98b4) return 1; /* final score */
    if(selector==0x2500 && script==0xa56e) return 2; /* frozen looping road */
    /* Story scenes retain the preceding stage selector. Require their script,
     * never use $0401 alone (00/02 is also playable stage 1-2). */
    switch(script) {
        case 0xabd9: case 0xa964: case 0xa7c4: case 0xa62d: case 0xa1d9: return 1;
    }
    return -1;
}
int AckmanVideoScoreScreen(void) { return word(0x400)==0x2d00 && word(0x1126)==0x98b4; }
/* $80:A8E0 selects an eight-byte camera region through $80:B028.
 * $80:A953 uses inclusive right bound minus $ff; $80:A973 uses left.
 * Read immutable ROM directly, without emulated bus accesses or clock changes. */
static unsigned romword(unsigned a) {
    if(!g_rom || a<0x8000 || a>0xfffe) return 0;
    return g_rom[a&0x7fff]|(unsigned)g_rom[(a&0x7fff)+1]<<8;
}
static int region_bounds(int *left,int *right) {
    unsigned selector=g_ram[0x401];
    if(selector>0x2d || word(0x402)<256 || word(0x406)<256) return 0;
    unsigned table=romword(0xb028+selector*2);
    unsigned index=(((word(0x406)>>8)-1)*g_ram[0x40d]+((word(0x402)>>8)-1)*2)&255;
    unsigned bounds=romword(table+index);
    *right=(int)romword(bounds)+1; *left=romword(bounds+2);
    return *left>=256 && *right>=*left+256 && *right<=0x8000;
}
static int wrapping(void) {
    if(!AckmanVideoWidescreen()) return 0;
    int policy=frame_policy;
    if(policy>=0) return policy;
    unsigned mode=g_ram[0x274]&7;
    return enabled && word(0x40c)==0 && (mode==0 || mode==1);
}
static void margins(int requested,int *left,int *right) {
    /* Host prepares the fixed canvas first. Resolve masks once, at Begin,
     * against the guest state that will actually be rendered. */
    (void)requested;
    *left=*right=0;
}
void AckmanVideoInstall(void) {
    const char *value=getenv("ACKMAN_WIDE_EXPERIMENTAL");
    enabled=!value || strcmp(value,"0");
    value=getenv("ACKMAN_WIDE_FORCE");
    force=!value || strcmp(value,"0");
    fprintf(stderr,"[ackman-video] enabled=%d force=%d (debug viewport)\n",enabled,force);
    const char *trace=getenv("ACKMAN_VIDEO_TRACE");
    if(trace && *trace && !layout_trace) {
        layout_trace=fopen(trace,"w");
        if(!layout_trace) { perror(trace); exit(1); }
        fputs("frame,mode,bg1sc,bg2sc,bg3sc,stride,camera_x,camera_y,origin_x,origin_y,mapped,scene_script\n",layout_trace);
    }
    const char *fade_path=getenv("ACKMAN_FADE_TRACE");
    if(fade_path && *fade_path && !fade_trace) {
        fade_trace=fopen(fade_path,"w");
        if(!fade_trace) { perror(fade_path); exit(1); }
        fputs("frame,selector,script,position,ram_0270,inidisp,policy,effective_policy,left,right,black\n",fade_trace);
    }
    AckmanRegisterWorldMargins(margins);
}
void AckmanVideoBegin(void) {
    patched=0;
    // Scripts advance before the visible fade begins (even at brightness 15).
    // Hold the outgoing scene policy until End verifies a completely black
    // composited frame; force blank or a low register alone is not an endpoint.
    frame_policy=AckmanFlowVideoPolicy(scene_policy());
    int accepted=frame_policy<0 && mapped();
    saved_count=0;
    visible_left=visible_right=0;
    if(wrapping()) visible_left=visible_right=g_ppu ? g_ppu->extraLeftRight : 0;
    if(accepted) calculate_margins(g_ppu->extraLeftRight,&visible_left,&visible_right);
    if(g_ppu) PpuSetExtraSideSpace(g_ppu,visible_left,visible_right,0);
    /* $0401 indexes the music table at $84:8BA7; its correspondence to
     * human stage numbers is unverified. Log the raw selector explicitly. */
    static unsigned previous[7];
    static int first=1, old_axes=-1;
    static unsigned old_x,old_y;
    unsigned state[]={g_ram[0x400],g_ram[0x401],g_ram[0x274],
        g_ram[0x276],g_ram[0x277],g_ram[0x278],word(0x40c)};
    int axes=(word(0x402)!=old_x) | ((word(0x406)!=old_y)<<1);
    if(first || memcmp(state,previous,sizeof(state)) || (axes!=old_axes && video_frame%30==0)) {
        fprintf(stderr,"[ackman-video] frame=%u stage=unmapped selector0400=%02X selector0401=%02X mode=%02X stride=%04X axes=%d fill=%d BG1=%04X,%04X BG2=%04X,%04X BG3=%04X,%04X camera=%04X,%04X\n",
            video_frame,state[0],state[1],state[2],state[6],axes,accepted,
            word(0x27c),word(0x27e),word(0x280),word(0x282),word(0x284),word(0x286),word(0x402),word(0x406));
        memcpy(previous,state,sizeof(state)); first=0; old_axes=axes;
    }
    old_x=word(0x402); old_y=word(0x406);
    ++video_frame;
    if(layout_trace) {
        fprintf(layout_trace,"%u,%02X,%02X,%02X,%02X,%04X,%04X,%04X,%04X,%04X,%d,%04X\n",
            video_frame,g_ram[0x274],g_ram[0x276],g_ram[0x277],g_ram[0x278],
            word(0x40c),word(0x402),word(0x406),word(0x416),word(0x41a),accepted,word(0x1126));
        if(!(video_frame%60)) fflush(layout_trace);
    }
    // The sky's repeating moon is scene-specific, independent of pixel aspect.
    unique_sky=AckmanVideoWidescreen() &&
        ((word(0x400)==0x2a00 && word(0x1126)==0xa22e) || frame_policy==SCENE_FINAL || frame_policy==SCENE_EPILOGUE);
    road=AckmanVideoWidescreen() && g_ppu && frame_policy==2;
    if(road) {
        saved_clamp=g_ppu->wsLayerClamp; patched=1;
        PpuSetWidescreenLayerClamp(g_ppu,saved_clamp|4);
        return;
    }
    if(AckmanVideoWidescreen() && g_ppu && frame_policy>=SCENE_DIALOGUE) {
        saved_clamp=g_ppu->wsLayerClamp;patched=1;
        if(frame_policy==SCENE_DIALOGUE) {
            // BG2 is the repeating Ackman pattern; BG1 holds the stage label
            // and cyan dialogue panel, BG3 holds text. OBJ stays native too.
            PpuSetWidescreenLayerClamp(g_ppu,saved_clamp|0x15);
            PpuSetWidescreenLayerRepeat(g_ppu,2);
        } else if(frame_policy==SCENE_FINAL || frame_policy==SCENE_EPILOGUE) {
            // The castle is assembled from OBJ: retain the PPU's extended OAM
            // evaluation and composition bounds during its entrance. Portraits
            // in the post-final dialogue stay in the native composition.
            PpuSetWidescreenLayerClamp(g_ppu,saved_clamp|
                (frame_policy==SCENE_FINAL ? 0x05 : 0x15));
        } else if(frame_policy==SCENE_CREDITS || frame_policy==SCENE_END_LANDSCAPE) {
            PpuSetWidescreenLayerClamp(g_ppu,saved_clamp|0x14);
            // Authored native landscape is periodic; never sample uninitialized
            // offscreen tilemap entries. Both layers retain native priorities.
            PpuSetWidescreenLayerRepeat(g_ppu,3);
        } else if(frame_policy==SCENE_END) {
            PpuSetWidescreenLayerClamp(g_ppu,saved_clamp|0x17);
            // Extend only the cyan horizontal backdrop, not THE END lettering.
            PpuSetWidescreenLayerRepeatBand(g_ppu,0,64,144);
        }
        return;
    }
    if (!accepted) return;
    saved_clamp=g_ppu->wsLayerClamp;
    patched=1;
    int left_columns=(visible_left+15)/16, right_columns=(visible_right+15)/16;
    for(int y=0;y<view.rows;y++) for(int x=-left_columns;x<view.columns+right_columns;x++) {
        if(x>=0 && x<view.columns) continue;
        unsigned value;
        if(tile(view.x+x,view.y+y,&value)) {
            unsigned address=0x5000+((view.sy+y)&31)*32+((view.sx+x)&31);
            saved_address[saved_count]=(uint16_t)address;
            saved[saved_count++]=g_ppu->vram[address];
            g_ppu->vram[address]=(uint16_t)value;
        }
    }
    PpuSetWidescreenLayerClamp(g_ppu,saved_clamp|4); /* BG3 HUD stays in its native viewport */
}
/* The road's HDMA changes horizontal scroll within the frame. Project only
 * the current row AFTER raster/HDMA, then restore before the next scanline. */
static uint16_t road_saved[2*kAckmanBorderTiles],road_address[2*kAckmanBorderTiles];
static unsigned road_count;
static uint16_t sky_saved[8],sky_address[8];
static unsigned sky_count;
/* Stage $2A/A22E sky: BG2's 32-column map repeats its 16-column artwork,
 * including the moon quad $04A0/$04A2/$04A4/$04A6. Keep every instance that
 * intersects the native viewport, and replace only wholly exterior copies.
 * Run after raster/HDMA so the phase is the PPU's current scroll, not WRAM.
 * Matching the complete artwork and plain sky tile prevents applying this
 * exception to another map or animated layout. CPU/save VRAM is restored.
 */
static void sky_scanline(void) {
    if(!unique_sky || !g_ppu || (g_ppu->bgmode&0x3f)!=0x39 || g_ppu->bgXsc[1]!=0x54) return;
    unsigned base=PPU_bgTilemapAdr(g_ppu,1);
    if(g_ppu->vram[base+4*32+11]!=0x10a8) return;
    for(int col=11;col<=27;col+=16) {
        unsigned addresses[]={base+2*32+col,base+2*32+col+1,base+3*32+col,base+3*32+col+1};
        const unsigned moon[]={0x04a0,0x04a2,0x04a4,0x04a6};
        int match=1;
        for(int i=0;i<4;i++) if(g_ppu->vram[addresses[i]]!=moon[i]) match=0;
        if(!match) continue;
        int x=(col*16-(g_ppu->hScroll[1]&511)+512)&511;
        // Either representative intersecting [0,256) belongs to the native image.
        if(x<256 || x-512+32>0) continue;
        for(int i=0;i<4;i++) {
            sky_address[sky_count]=(uint16_t)addresses[i];
            sky_saved[sky_count++]=g_ppu->vram[addresses[i]];
            g_ppu->vram[addresses[i]]=0x10a8;
        }
    }
}
void AckmanVideoScanlineBegin(int line) {
    road_count=sky_count=0;
    if(line) sky_scanline();
    if(!road || !line) return;
    int sx=g_ppu->hScroll[0]/16;
    int cols=((g_ppu->hScroll[0]&15)+271)/16;
    int extra=(AckmanVideoViewportExtra(g_ppu->extraLeftRight)+15)/16;
    unsigned row=((g_ppu->vScroll[0]+line)/16)&31;
    for(int x=-extra;x<cols+extra;x++) {
        if(x>=0 && x<cols) continue;
        unsigned dst=0x5000+row*32+((sx+x)&31);
        unsigned src=0x5000+row*32+((sx+((x%16+16)%16))&31);
        road_address[road_count]=dst; road_saved[road_count++]=g_ppu->vram[dst];
        g_ppu->vram[dst]=g_ppu->vram[src];
    }
}
void AckmanVideoScanlineEnd(void) {
    for(unsigned i=0;i<sky_count;i++) g_ppu->vram[sky_address[i]]=sky_saved[i];
    sky_count=0;
    for(unsigned i=0;i<road_count;i++) g_ppu->vram[road_address[i]]=road_saved[i];
    road_count=0;
}
void AckmanVideoEnd(void) {
    if (patched) {
        for(unsigned i=0;i<saved_count;i++) g_ppu->vram[saved_address[i]]=saved[i];
        PpuSetWidescreenLayerClamp(g_ppu,saved_clamp);
    }
    /* Final mask also excludes sprites/backdrop/HDMA from invalid margins. */
    if(g_ppu && g_ppu->renderBuffer &&
       (visible_left<g_ppu->extraLeftRight || visible_right<g_ppu->extraLeftRight)) {
        int budget=g_ppu->extraLeftRight;
        for(int y=0;y<224;y++) {
            unsigned char *row=g_ppu->renderBuffer+(size_t)y*g_ppu->renderPitch;
            memset(row,0,(size_t)(budget-visible_left)*4);
            memset(row+(budget+256+visible_right)*4,0,(size_t)(budget-visible_right)*4);
        }
    }
    if(g_ppu) AckmanFlowShade(g_ppu->renderBuffer,g_ppu->renderPitch,256+2*g_ppu->extraLeftRight,224);
    if(g_ppu) {
        int black=g_ppu->renderBuffer!=NULL;
        if(g_ppu->renderBuffer)for(int y=0;y<224 && black;y++) {
            const unsigned char *row=g_ppu->renderBuffer+(size_t)y*g_ppu->renderPitch;
            for(int x=0;x<256+2*g_ppu->extraLeftRight;x++)
                if(row[x*4]|row[x*4+1]|row[x*4+2]) { black=0;break; }
        }
        AckmanFlowVideoEndpoint(black!=0);
        if(fade_trace) {
            fprintf(fade_trace,"%u,%04X,%04X,%04X,%u,%u,%d,%d,%d,%d,%d\n",video_frame,word(0x400),word(0x1126),word(0x1124),g_ram[0x270],g_ppu->inidisp,scene_policy(),frame_policy,visible_left,visible_right,black);
            fflush(fade_trace);
        }
    }
    patched=0;
}

AckmanRumbleScene AckmanVideoRumbleScene(void) {
    unsigned selector=word(0x400),script=word(0x1126);
    // $A22E continues the letterboxed story: +$5C commits the castle's
    // negative Y velocity. Keep its engine through the whole remaining script,
    // including intermediate blank/flash frames. This does not change viewport policy.
    if(selector==0x2800 && script==0xa22e && word(0x1124)>=0x5c) {
        // +$89 is the final fade. Its opaque endpoint ends the cinematic.
        // The following $2900 room retains a stale A22E pointer during dialogue;
        // that room is gameplay and must never inherit the castle engine.
        if(word(0x1124)>=0x89 && g_ppu && (PPU_forcedBlank(g_ppu) || !PPU_brightness(g_ppu)))return ACK_RUMBLE_NONE;
        return ACK_RUMBLE_CASTLE;
    }
    if(selector==0x2d00 && script==0x9be4)return ACK_RUMBLE_END_CLIMAX;
    if(g_ppu && (PPU_forcedBlank(g_ppu) || !PPU_brightness(g_ppu)))return ACK_RUMBLE_NONE;
    if(selector==0x2500 && script==0xa56e)return ACK_RUMBLE_ROAD;

    if(selector==0x2d00) {
        if(script==0x9a4a || script==0x9ab9 || script==0x9af0)return ACK_RUMBLE_END_FLIGHT;
        if(script==0x9b2f || script==0x9bdc)return ACK_RUMBLE_END_BLASTS;
        if(script==0x9f08)return ACK_RUMBLE_FALL;
    }
    return ACK_RUMBLE_NONE;
}

bool AckmanVideoRumbleCharging(void) {
    // $87:AC16 latches a ready charge; $87:AC1B/$878CB6 select the
    // rotating-arm animations $13/$14. The earlier $0B3A count is silent.
    // $0491 bit 1 locks player control during text/dialogue ($87:80AF).
    return (g_ram[0x414]&1) && (g_ram[0x448]==0x13 || g_ram[0x448]==0x14) &&
           !(g_ram[0x413]&2) && !(g_ram[0x491]&0xc2) && !(g_ram[0x414]&0x90);
}
bool AckmanVideoRumbleInterludeContact(void) {
    if(word(0x1126)!=0xa34d || word(0x1124)!=0xd5)return false;
    // Verified metasprite render records for this script's traveling wave and
    // stationary angel. Contact is the wave crest reaching the front of the
    // torso (eight native pixels left of its anchor), BEFORE the +$EF snap.
    int wave=(int16_t)word(0x2204),angel=(int16_t)word(0x2224);
    int dy=(int16_t)word(0x2206)-(int16_t)word(0x2226);
    return word(0x2222)==4 && word(0x2212)!=0 && wave>=angel-8 && wave<angel+32 && dy>=-16 && dy<=16;
}
bool AckmanVideoRumbleCastleIgnition(void) {
    return word(0x400)==0x2800 && word(0x1126)==0xa22e &&
           word(0x1124)>=0x1c && word(0x1124)<0x5c;
}
bool AckmanVideoRumbleClimaxFlash(void) {
    // $81:9BE4 command $2C/$17 switches the expanding final blast into
    // its white flash phase. It is independent of the small-blast OBJ IDs.
    return word(0x400)==0x2d00 && word(0x1126)==0x9be4 && word(0x1124)>=0x1f;
}
