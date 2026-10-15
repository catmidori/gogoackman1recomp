#include "port_extensions.h"
#include "ackman_hooks.h"
#include "ackman_video.h"
#include "ackman_log.h"
#include "cpu_state.h"
#include "snes/snes.h"
#include "snes/ppu.h"
#include "widescreen.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

extern CpuState g_cpu;
extern Snes *g_snes;
extern Ppu *g_ppu;
static AckmanWorldMargins world_margins;
static FILE *trace;
static int trace_initialized;

void AckmanRegisterWorldMargins(AckmanWorldMargins hook) { world_margins = hook; }
void AckmanPrepareFrame(int dw, int dh, int *fw, int *fh) {
    /* Preserve the configured pixel aspect and HUD logical size.
     * Horizontal expansion only; don't invent additional vertical game space.
     */
    int width,height;
    AckmanVideoFrameSize(dw,dh,&width,&height);
    int requested = (width - 256) / 2, left = 0, right = 0;
    if (world_margins) world_margins(requested, &left, &right);
    if (left < 0) left = 0;
    if (right < 0) right = 0;
    if (left > requested) left = requested;
    if (right > requested) right = requested;
    /* Symmetric budget; unavailable room edges are blanked by the PPU. */
    int budget = AckmanVideoViewportExtra(requested);
    g_ws_extra = budget;
    g_ws_active = budget != 0;
    if (g_ppu) {
        PpuSetExtraSpaceCentered(g_ppu, (uint16_t)budget);
        PpuSetExtraSideSpace(g_ppu, left, right, 0);
    }
    *fw = 256 + 2 * budget;
    *fh = height;
}

void AckmanAfterFrame(const SnesDesktopHostFrameStats *s) {
    AckmanLogFrame();
    AckmanObserveFrame(s->frame);
    if (!trace_initialized) {
        const char *path = getenv("ACKMAN_TRACE");
        trace_initialized = 1;
        if (path && *path) {
            trace = fopen(path, "w");
            if (!trace) { perror("ACKMAN_TRACE"); exit(EXIT_FAILURE); }
            fputs("frame,master_cycles,hpos,vpos,irq_latches,irq_missed,guest_ms\n", trace);
        }
    }
    if (trace && g_snes) {
        fprintf(trace, "%u,%" PRIu64 ",%u,%u,%u,%u,%.6f\n",
            s->frame, g_cpu.master_cycles, g_snes->hPos, g_snes->vPos,
            g_snes->dbgIrqLatches, g_snes->dbgIrqMissed, s->guest_seconds * 1000.0);
        if (!(s->frame % 60)) fflush(trace);
    }
}
void AckmanCloseTrace(void) { if (trace) fclose(trace); trace = NULL; }
