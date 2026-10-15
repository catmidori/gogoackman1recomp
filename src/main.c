/*
 * Go Go Ackman — desktop host shim.
 *
 * Identity and hooks, nothing else. The host itself is the framework's
 * (snesrecomp/runner/src/desktop/host_main.h): the pre-boot launcher, ROM
 * resolution and digest checks, config.ini, the window and
 * the SDL / OpenGL presenters, audio, gamepads, the in-game save-state
 * browser and hold-to-rewind, the OSD, the pacing clock, crash handlers and
 * the post-mortem report, mod packages, Generate & rebuild, and the netplay
 * barrier when the project is built with it. A fix there reaches this
 * project on a submodule pull; nothing in this file needs to change for it.
 *
 * This file used to BE the host -- a ~700-line copy made at scaffold time --
 * so every project scaffolded before a fix kept the bug. Two ports of the
 * same game, a week apart, had different hosts.
 *
 * Nothing here ever ships a ROM. The player supplies one; the host makes
 * that easy (launcher, then positional argument, then a copy beside the
 * executable, then config.ini [General] Rom, then a file picker) and checks what it
 * is handed against the digests in rom_identity.txt.
 *
 * Grow this file only with what is specific to THIS title: a custom
 * presenter (prepare_frame / draw_frame), an SPC player, a pacing rule
 * (keep_pacing_debt), a Mods provider. See SnesDesktopHostGame for the
 * complete list of hooks and what each is for.
 */

#include "host_main.h"
#include "game_rtl.h"
#include "rom_guard.h"
#include "port_extensions.h"
#include "ackman_hooks.h"
#include "ackman_audio.h"
#include "ackman_log.h"
#include "ackman_menu.h"
#include "ackman_mods.h"
#include "ackman_config.h"
#include "ackman_cursor.h"
#include "ackman_video.h"
#include "ackman_debug.h"
#include "ackman_rumble.h"
#include "ackman_devices.h"
#include "ackman_input_probe.h"
#include "ackman_boot.h"
#include "ackman_shader.h"
#include "desktop/config.h"
#include "snesrecomp_rom_identity.h"  /* generated from rom_identity.txt */

#ifndef __ANDROID__
#define SDL_MAIN_HANDLED 1
#endif
#include "desktop/sdl_compat.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#undef FORCEINLINE
#endif
#ifdef __ANDROID__
/* SDLActivity loads libmain.so and dlsyms its SDL_main. */
#define main SDL_main
#endif

#ifndef SNES_GAME_VERSION
#define SNES_GAME_VERSION "dev"
#endif

static bool boot_fullscreen;
SDL_Window *AckmanCreateWindow(const char *title,int width,int height,SDL_WindowFlags flags) {
    SDL_Window *created=SDL_CreateWindow(title,width,height,flags);
    if(created) {
#ifdef _WIN32
        /* SDL's ordinary raise can succeed without foreground activation on
         * Windows. Its native force path temporarily joins input queues. */
        SDL_SetHint(SDL_HINT_FORCE_RAISEWINDOW,"1");
        SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED,"1");
#endif
        bool raised=SDL_RaiseWindow(created);
        const char *trace=SDL_getenv("ACKMAN_WINDOW_TRACE");
        if(trace && *trace) {
            SDL_IOStream *file=SDL_IOFromFile(trace,"ab");
            if(file) {
                int foreground=-1;
#ifdef _WIN32
                HWND handle=(HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(created),SDL_PROP_WINDOW_WIN32_HWND_POINTER,NULL);
                if(handle)foreground=GetForegroundWindow()==handle;
#endif
                char line[96];int length=SDL_snprintf(line,sizeof(line),"fullscreen=%d raised=%d foreground=%d\n",
                    (SDL_GetWindowFlags(created)&SDL_WINDOW_FULLSCREEN)!=0,raised,foreground);
                SDL_WriteIO(file,line,(size_t)length);SDL_CloseIO(file);
            }
        }
    }
    return created;
}
static void AckmanLoadedRom(const uint8_t *rom,size_t size) {
    AckmanVerifyLoadedRom(rom,size);AckmanModsRomLoaded(size);
}
static void AckmanVideoBootFailure(void) {
    AckmanBootShaderFailure();
    AckmanMenuShaderFailure();
}
static bool AckmanShaderProbe(bool applying) {
    if(applying && (AckmanBootStable() || !AckmanBootResume())) {
        /* A driver crash cannot run cleanup: checkpoint before entering it. */
        if(!AckmanAutosaveForced(true)) {
            /* Do not enter a risky driver without a durable checkpoint. */
            AckmanMenuShaderFailure();return false;
        }
    }
    AckmanBootApplying(applying);
    return true;
}
static void AckmanConfigureVideo(void) {
    if(boot_fullscreen)g_config.fullscreen=1;
    g_config.renderer[0]=0; /* OutputMethod is the sole boot driver authority. */
    AckmanShaderSetFailureHandler(AckmanVideoBootFailure);
    AckmanShaderSetProbeHandler(AckmanShaderProbe);
    AckmanRumbleSetEnabled(g_config.controller_vibration);
    AckmanConfigMigrateHotkeys();
    ConfigReloadKeyMap(AckmanConfigPath());
    AckmanMenuRomDirectory();
    RecompMenuAutosaveConfigure(AckmanMenuSaveDirectory(),RecompMenuServices()->emergency_snapshot);
    // Match the real core bound, with the menu's additional ten-frame ceiling.
    AckmanMenuSetRunAhead(g_config.run_ahead<0?0:
        g_config.run_ahead>AckmanMenuRunAheadMax()?AckmanMenuRunAheadMax():g_config.run_ahead);
    // Hidden cadence preference: the config parser defaults to 4 (16 seconds).
    AckmanMenuSetRewindInterval(g_config.rewind_interval);
    AckmanVideoSetWidescreen(g_config.ignore_aspect_ratio);
    /* Shaders are validated here, then pinned by MenuInit. UI edits belong to
     * the next process and cannot reload an effect in this renderer. */
    AckmanMenuShadersRefresh();
}
static void AckmanFrameComplete(const SnesDesktopHostFrameStats *stats) {
    AckmanAfterFrame(stats);
    AckmanInputProbe(stats->frame); /* Disabled unless the regression environment flag is set. */
}
static const SnesDesktopHostGame kGameHost = {
    .display_name        = "Go Go Ackman Recompiled",
    /* Boot parser resolves this complete KeyMap dictionary through the runner;
     * no parallel key bindings belong in main or in the SDL event wrapper.
     * Gameplay bindings are isolated in Keyboard/Controller.Keybinds sections;
     * the Controls wizard commits a complete map before the host polls it. */
    .default_config_ini  = AckmanDefaultConfig,
    .window_title        = "Go Go Ackman Recompiled",
    .region              = SNESRECOMP_ROM_REGION,
    .rom_file            = SNESRECOMP_ROM_FILE,
    .expected_sha256_hex = SNESRECOMP_ROM_EXPECTED_SHA256,
    .expected_crc32_hex  = SNESRECOMP_ROM_EXPECTED_CRC32,
    .game_id             = SNESRECOMP_ROM_GAME_ID,
    .build_version       = SNES_GAME_VERSION,
    .game_info           = &kGameInfo,
    .num_players         = 1,
    /* Battery-backed SRAM shows the launcher's SAVES panel. Leave NULL for a
     * title without one. The path is exe-relative. */
    .sram_path           = NULL, /* this cartridge has no SRAM */
    .on_rom_loaded      = AckmanLoadedRom,
    .after_config       = AckmanConfigureVideo,
    /* Native file dialogs use the wrapper's mandatory pause/audio lock,
     * independently of the configurable inactive-window policy. */
    .compute_viewport   = AckmanVideoComputeViewport,
    .prepare_frame      = AckmanPrepareFrame,
    .before_run_frame   = AckmanModsBeforeFrame,
    .after_run_frame    = AckmanFrameComplete,
    .native_widescreen  = 1,
    .state_menu_hotkeys = 1,
    /* Mod catalog flags; native Ackman video/audio modules are separate. */
    .widescreen_supported = 0,
    .msu1_supported = 0,
};

#ifndef __ANDROID__
#undef main   /* desktop: keep plain main() even if SDL_main.h remapped it */
#endif
int main(int argc, char **argv)
{
#ifdef ACKMAN_FLATPAK
    SDL_SetAppMetadata("Go Go Ackman Recompiled",SNES_GAME_VERSION,ACKMAN_APPLICATION_ID);
    SDL_SetHint(SDL_HINT_APP_ID,ACKMAN_APPLICATION_ID);
    SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER,"portal");
#endif
    if(!AckmanMenuRegister())return 1;
    int supervised=AckmanBootEnter(&argc,argv);
    if(supervised>=0)return supervised;
    /* Consume the private restart option before the runner resolves the ROM.
     * Fullscreen is applied before its initial SDL window flags are computed. */
    int retained=1;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--ackman-fullscreen"))boot_fullscreen=true;
        else argv[retained++]=argv[i];
    }
    argc=retained;argv[argc]=NULL;
    AckmanCursorReset();
    AckmanMenuSetBootObserver(AckmanBootPresent);
    AckmanMenuConfigure();
    if(!AckmanLogInit()) return 1;
    AckmanHooksInit();
    int result = snesrecomp_desktop_main(&kGameHost, argc, argv);
    AckmanInputProbeClose();
    AckmanDevicesShutdown();
    AckmanModsShutdown();
    AckmanHooksClose();
    AckmanAudioClose();
    AckmanCloseTrace();
    AckmanLogShutdown();
    int relaunch_result=AckmanMenuRelaunch();
    return result ? result : relaunch_result;
}

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show) {
    (void)instance;(void)previous;(void)command;(void)show;
    return main(__argc,__argv);
}
#endif
