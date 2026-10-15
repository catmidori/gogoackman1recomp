/* Frame scheduler derived from the SNESRecomp template, anchored to the
 * shared hardware beam. This core's fixed NTSC field is not cycle-exact. */
#include "game_rtl.h"
#include "ackman_audio.h"
#include "ackman_video.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "snes/dma.h"
#include "snes/interp_bridge.h"
#include "snes/ppu.h"
#include "snes/snes.h"
#include "snes/saveload.h"
#include <stdio.h>
#include <stdlib.h>
#include "desktop/sdl_compat.h"
#include "desktop/host_main.h"
#include "snes_rewind.h"
extern CpuState g_cpu;
extern Ppu *g_ppu;
static uint32_t resume_pc;
static uint8_t vblank_due;
static int execution_loaded;
static FILE *timing_trace;
static unsigned timing_frame;
static int trace_stalls;
static void report_stall(const char *stage,uint64_t start) {
    if(!trace_stalls) return;
    uint64_t us=(SDL_GetTicksNS()-start)/1000;
    if(us>=20000) {
        extern uint8_t g_ram[0x20000];
        fprintf(stderr,"[ackman-stall] stage=%s us=%llu pc=%06X inidisp=%02X clock=%llu\n",
            stage,(unsigned long long)us,resume_pc,g_ram[0x272],
            (unsigned long long)g_cpu.master_cycles);
    }
}
static uint64_t traced_boundary, crossing_start, crossing_base, crossing_end;
static uint32_t crossing_pc;
void AckmanTimingCrossing(uint32_t pc,uint64_t start,uint64_t base,uint64_t end) {
    if (!timing_trace || start>=traced_boundary || end<traced_boundary) return;
    crossing_pc=pc; crossing_start=start; crossing_base=base; crossing_end=end;
}
static uint32_t vector(uint32_t a) {
    return snes_read(g_snes,a) | (uint32_t)snes_read(g_snes,a+1)<<8;
}
static void interrupt(uint32_t address) {
    cpu_push_interrupt_frame_at(&g_cpu,resume_pc);
    interp_bridge_set_master_deadline(0); /* handler must reach RTI */
    if (!interp_bridge_run_interrupt(&g_cpu,vector(address))) {
        fputs("Ackman: interrupt failed to reach RTI\n",stderr); exit(1);
    }
    uint32_t pc=interp_bridge_lle_resume_pc();
    if (pc) resume_pc=pc;
    snes_sync_master_clock(g_snes,g_cpu.master_cycles);
}
void GameRunOneFrame(void) {
    uint64_t started=trace_stalls?SDL_GetTicksNS():0;
    snes_sync_master_clock(g_snes,g_cpu.master_cycles);
    if (!resume_pc) resume_pc=vector(0xfffc);
    uint32_t distance=snes_master_clocks_until_line(g_snes,225);
    if (!distance) distance=262u*1364u;
    /* Account for previous instruction overshoot, don't accumulate it. */
    const uint64_t boundary=g_snes->beamMasterLast+distance;
    traced_boundary=boundary;
    crossing_pc=0; crossing_start=crossing_base=crossing_end=0;
    if (vblank_due) {
        vblank_due=0;
        g_snes->inNmi=true;
        ppu_checkOverscan(g_ppu);
        ppu_handleVblank(g_ppu);
        if (g_snes->nmiEnabled) interrupt(g_cpu.emulation?0xfffa:0xffea);
    }
    ppu_rasterBegin(g_ppu);
    for (unsigned slice=0;g_cpu.master_cycles<boundary;++slice) {
        if (slice==8192) { fputs("Ackman: slice watchdog\n",stderr); exit(1); }
        if (g_snes->inIrq && !g_cpu._flag_I) interrupt(g_cpu.emulation?0xfffe:0xffee);
        uint64_t deadline=boundary;
        uint32_t irq=snes_master_clocks_until_irq(g_snes);
        if (irq && g_snes->beamMasterLast+irq+1<deadline)
            deadline=g_snes->beamMasterLast+irq+1;
        if (g_cpu.master_cycles+1364<deadline) deadline=g_cpu.master_cycles+1364;
        interp_bridge_set_master_deadline(deadline);
        if (!interp_bridge_run_until_quiescent(&g_cpu,resume_pc)) {
            fputs("Ackman: guest execution failed before frame boundary\n",stderr);
            exit(1);
        }
        interp_bridge_set_master_deadline(0);
        uint32_t pc=interp_bridge_lle_resume_pc();
        if (pc) resume_pc=pc;
        int wai=interp_bridge_lle_took_wai();
        int quiet=interp_bridge_lle_took_quiescent();
        if ((wai||quiet) && g_cpu.master_cycles<deadline) {
            g_cpu.master_cycles=deadline; /* hardware time while guest waits */
            snes_refresh_exempt();
        }
        snes_sync_master_clock(g_snes,g_cpu.master_cycles);
        snes_catchupApu(g_snes);
    }
    vblank_due=1;
    report_stall("guest",started);
    if (timing_trace) {
        fprintf(timing_trace,"%u,%llu,%llu,%llu,%06X,%06X,%llu,%llu,%llu\n",++timing_frame,
            (unsigned long long)boundary,(unsigned long long)g_cpu.master_cycles,
            (unsigned long long)(g_cpu.master_cycles-boundary),resume_pc,crossing_pc,
            (unsigned long long)crossing_start,
            (unsigned long long)(crossing_base-crossing_start),
            (unsigned long long)(crossing_end-crossing_base));
        if(!(timing_frame%60)) fflush(timing_trace);
    }
}
void GameDrawPpuFrame(void) {
    uint64_t started=trace_stalls?SDL_GetTicksNS():0;
    AckmanVideoBegin();
    SimpleHdma channels[8];
    uint8_t hdma=ppu_rasterInitialHdmaen();
    dma_startDma(g_snes->dma,hdma,true);
    for(int ch=0;ch<8;++ch) SimpleHdma_Init(&channels[ch],&g_snes->dma->channel[ch]);
    ppu_rasterRenderBegin(g_ppu);
    for(int line=0;line<=224;++line) {
        ppu_rasterApplyLine(g_ppu,line);
        uint8_t next;
        if(ppu_rasterTakeHdmaen(&next)) {
            dma_startDma(g_snes->dma,next,true);
            for(int ch=0;ch<8;++ch)
                if((next & ~hdma) & (1<<ch)) SimpleHdma_Init(&channels[ch],&g_snes->dma->channel[ch]);
            hdma=next;
        }
        /* Line zero initializes the PPU; it does not consume a visible HDMA row. */
        if(line) for(int ch=0;ch<8;++ch)
            if(hdma & (1<<ch)) SimpleHdma_DoLine(&channels[ch]);
        AckmanVideoScanlineBegin(line);
        ppu_runLine(g_ppu,line); /* no second execution of IRQ in renderer */
        AckmanVideoScanlineEnd();
    }
    AckmanVideoEnd();
    report_stall("raster",started);
}
static void save_extra(SaveLoadInfo *sli) {
    uint8_t bytes[5];
    for(int i=0;i<4;++i) bytes[i]=(uint8_t)(resume_pc>>(8*i));
    bytes[4]=vblank_due;
    sli->func(sli,bytes,sizeof(bytes));
    /* Includes executing CPU registers, pending APU ports and clock anchors. */
    RtlSaveExecutionState(sli);
    uint8_t tag[4]={'A','C','P',1};
    sli->func(sli,tag,sizeof(tag));
}
static void load_extra(SaveLoadInfo *sli,uint32_t version) {
    /* PCM position is not serialized yet. Never start a pending replacement
     * from the future after loading an earlier SPC/CPU state. */
    AckmanAudioReset();
    uint8_t bytes[5]={0}; (void)version;
    sli->func(sli,bytes,sizeof(bytes));
    resume_pc=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
    vblank_due=bytes[4]!=0;
    execution_loaded=RtlLoadExecutionState(sli);
    if(!execution_loaded) {
        fputs("[ackman-state] incompatible execution state\n",stderr);
    }
    uint8_t tag[4]; sli->func(sli,tag,sizeof(tag));
    execution_loaded=execution_loaded && tag[0]=='A' && tag[1]=='C' && tag[2]=='P' && tag[3]==1 && cpu_pc24_resumable(resume_pc);
    interp_bridge_set_master_deadline(0);
    interp_bridge_lle_took_wai();
    interp_bridge_lle_took_quiescent();
    snes_set_hdma_beam_enabled(g_snes,false);
}
bool AckmanCoreLoadState(const char *path);
bool RtlLoadSnapshot(const char *path) {
    /* Refuse incomplete legacy saves BEFORE modifying guest memory. Those
     * files contain only a resume PC, not the registers needed at that PC. */
    FILE *f=fopen(path,"rb");
    if(!f) return AckmanCoreLoadState(path);
    uint8_t tag[4]={0};
    int valid=fseek(f,-4,SEEK_END)==0 && fread(tag,1,4,f)==4 &&
        tag[0]=='A' && tag[1]=='C' && tag[2]=='P' && tag[3]==1;
    fclose(f);
    if(!valid) {
        fprintf(stderr,"[ackman-state] load refused: %s lacks CPU registers (legacy incomplete save); current game preserved\n",path);
        return false;
    }
    size_t capacity=2*1024*1024;
    void *backup=malloc(capacity);
    if(!backup) return false;
    size_t size=RtlSaveSnapshotToMemory(backup,capacity);
    if(!size) { free(backup); return false; }
    execution_loaded=0;
    AckmanHostStateLoadBegin();
    bool ok=AckmanCoreLoadState(path) && execution_loaded;
    if(!ok) {
        if(!RtlLoadSnapshotFromMemory(backup,size)) {
            fputs("[ackman-state] failed to restore safety snapshot\n",stderr); abort();
        }
        fputs("[ackman-state] invalid save rejected; previous session restored\n",stderr);
    }
    AckmanHostStateLoadEnd();
    free(backup);
    return ok;
}
static void state_loaded(uint32_t version) {
    (void)version;
    if(!execution_loaded) return;
    RtlApuLock();
    RtlApplyExecutionState();
    RtlApuUnlock();
    if(!snes_rewind_is_open())fprintf(stderr,"[ackman-state] restored pc=%06X D=%04X S=%04X clock=%llu\n",
        resume_pc,g_cpu.D,g_cpu.S,(unsigned long long)g_cpu.master_cycles);
    interp_bridge_set_master_deadline(0);
    interp_bridge_lle_took_wai();
    interp_bridge_lle_took_quiescent();
    snes_set_hdma_beam_enabled(g_snes,false);
}
void GameSessionReset(void) {
    resume_pc=0; vblank_due=0;
    AckmanAudioReset();
}
static void initialize(void) {
    const char *stalls=getenv("ACKMAN_HOST_STALL_TRACE");
    trace_stalls=stalls && atoi(stalls)!=0;
    const char *path=getenv("ACKMAN_TIMING_TRACE");
    if (path && *path && !timing_trace) {
        timing_trace=fopen(path,"w");
        if (!timing_trace) { perror(path); exit(1); }
        fputs("frame,vblank_edge,cpu_boundary,late_master_clocks,resume_pc,crossing_pc,instruction_start,instruction_clocks,refresh_clocks\n",timing_trace);
    }
    snes_set_hdma_beam_enabled(g_snes,false);
    GameSessionReset();
    AckmanAudioInstall();
    AckmanVideoInstall();
}
const RtlGameInfo kGameInfo={
    .title="go-go-ackman",.initialize=initialize,
    .run_frame=GameRunOneFrame,.draw_ppu_frame=GameDrawPpuFrame,
    .save_name_prefix="ackman-beam-v2",.state_save_extra=save_extra,
    .state_load_extra=load_extra,.on_state_loaded=state_loaded,.session_reset=GameSessionReset,.minimum_state_version=5,
};

