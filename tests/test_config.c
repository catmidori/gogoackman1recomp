#include "ackman_config.h"
#include "desktop/config.h"
#include "keybinds.h"
#include "launcher_cache.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void Die(const char *format) { (void)format; abort(); }
static void write_file(const char *path,const char *text) {
    SDL_IOStream *f=SDL_IOFromFile(path,"wb");assert(f);
    assert(SDL_WriteIO(f,text,strlen(text))==strlen(text));assert(SDL_CloseIO(f));
}
int main(void) {
    assert(SDL_Init(SDL_INIT_EVENTS));
    const char *ini="unified-config-test.ini";
    SDL_RemovePath(ini);ParseConfigFile(ini);
    assert(g_config.output_method==kOutputMethod_OpenGL);
    assert(g_config.pause_when_inactive && g_config.audio_freq==48000 && g_config.volume==100 && g_config.vsync);
    assert(g_config.window_width==1344 && g_config.window_height==672);
    assert(g_config.window_scale==1 && g_config.ignore_aspect_ratio);
    assert(!g_config.enable_shaders && !g_config.integer_scaling && !g_config.selected_shader[0]);
    assert(!g_config.screenshot_internal);
    write_file(ini,"[Graphics]\nScreenshotResolution = InternalResolution\n");ParseConfigFile(ini);assert(g_config.screenshot_internal);
    write_file(ini,"[Graphics]\nScreenshotResolution = invalid\n");ParseConfigFile(ini);assert(!g_config.screenshot_internal);
    write_file(ini,"[Graphics]\nEnableShaders = 1\nIntegerScaling = 1\nSelectedShader = crt-test.glsl\n");
    ParseConfigFile(ini);assert(g_config.enable_shaders && g_config.integer_scaling);
    assert(!strcmp(g_config.selected_shader,"crt-test.glsl"));
    SDL_RemovePath(ini);ParseConfigFile(ini);
    assert(g_config.rewind_depth==240 && g_config.rewind_interval==4 && g_config.controller_vibration);
    write_file(ini,"[General]\nRewindDepth = 1000\nRewindInterval = 3\n");
    ParseConfigFile(ini);assert(g_config.rewind_depth==240 && g_config.rewind_interval==4);
    write_file(ini,"[General]\nRewindDepth = 12\nRewindInterval = 6\n");
    ParseConfigFile(ini);assert(g_config.rewind_depth==12 && g_config.rewind_interval==6);
    write_file(ini,"[Graphics]\nWidescreen = 0\nIgnoreAspectRatio = 1\nWindowScale = 99\n");
    ParseConfigFile(ini);assert(!g_config.ignore_aspect_ratio && g_config.window_scale==10);
    write_file(ini,"[Graphics]\nIgnoreAspectRatio = 0\nWindowScale = -1\n");
    ParseConfigFile(ini);assert(!g_config.ignore_aspect_ratio && g_config.window_scale==1);
    write_file(ini,"[Graphics]\nInternalResolution = 3\nWindowScale = 9\n");
    ParseConfigFile(ini);assert(g_config.window_scale==3);
    write_file(ini,"[Graphics]\nWindowScale = 9\nInternalResolution = 2\n");
    ParseConfigFile(ini);assert(g_config.window_scale==2);
    SDL_RemovePath(ini);ParseConfigFile(ini);
    assert(FindCmdForSdlKey(SDLK_F1,0)==kKeys_Load);
    assert(FindCmdForSdlKey(SDLK_F1,SDL_KMOD_SHIFT)==kKeys_Save);
    assert(FindCmdForSdlKey(SDLK_F2,0)==kKeys_Load+1);
    {
        const struct { SDL_Keycode key; SDL_Keymod mod; int command; } bindings[] = {
            {SDLK_RETURN, SDL_KMOD_ALT, kKeys_Fullscreen},
            {SDLK_f, 0, kKeys_DisplayPerf}, {SDLK_TAB, 0, kKeys_Turbo},
            {SDLK_m, 0, kKeys_Mute}, {SDLK_o, 0, kKeys_WindowBigger},
            {SDLK_i, 0, kKeys_WindowSmaller}, {SDLK_r, SDL_KMOD_CTRL, kKeys_Reset},
            {SDLK_p, 0, kKeys_Pause}, {SDLK_p, SDL_KMOD_SHIFT, kKeys_PauseDimmed},
            {SDLK_KP_PLUS, 0, kKeys_VolumeUp}, {SDLK_KP_MINUS, 0, kKeys_VolumeDown},
            {SDLK_r, 0, kKeys_ToggleRenderer}, {SDLK_8, 0, kKeys_Rewind},
            {SDLK_u, 0, kKeys_ToggleWidescreen}
        };
        char *data=SDL_LoadFile(ini,NULL);assert(data && !strstr(data,"Controls ="));SDL_free(data);
        for(size_t n=0;n<sizeof(bindings)/sizeof(bindings[0]);++n)
            assert(FindCmdForSdlKey(bindings[n].key,bindings[n].mod)==bindings[n].command);
        AckmanIniValue moved[]={
            {"KeyMap","Fullscreen","Ctrl+F9"}, {"KeyMap","ToggleFPS","F13"},
            {"KeyMap","Turbo","F11"}, {"KeyMap","Mute","F12"},
            {"KeyMap","ScaleUp","PageUp"}, {"KeyMap","ScaleDown","PageDown"},
            {"KeyMap","Screenshot","Ctrl+F10"}, {"KeyMap","ToggleWidescreen","Alt+w"}
        };
        assert(AckmanConfigUpdate(moved,sizeof(moved)/sizeof(moved[0])));ConfigReloadKeyMap(ini);
        assert(!FindCmdForSdlKey(SDLK_RETURN,SDL_KMOD_ALT));
        assert(!FindCmdForSdlKey(SDLK_f,0) && !FindCmdForSdlKey(SDLK_m,0));
        assert(!FindCmdForSdlKey(SDLK_TAB,0) && !FindCmdForSdlKey(SDLK_o,0) && !FindCmdForSdlKey(SDLK_i,0));
        assert(FindCmdForSdlKey(SDLK_F9,SDL_KMOD_CTRL)==kKeys_Fullscreen);
        assert(FindCmdForSdlKey(SDLK_F13,0)==kKeys_DisplayPerf);
        assert(FindCmdForSdlKey(SDLK_F11,0)==kKeys_Turbo);
        assert(FindCmdForSdlKey(SDLK_F12,0)==kKeys_Mute);
        assert(FindCmdForSdlKey(SDLK_PAGEUP,0)==kKeys_WindowBigger);
        assert(FindCmdForSdlKey(SDLK_PAGEDOWN,0)==kKeys_WindowSmaller);
        assert(FindCmdForSdlKey(SDLK_F10,SDL_KMOD_CTRL)==kKeys_Screenshot);
        assert(FindCmdForSdlKey(SDLK_w,SDL_KMOD_ALT)==kKeys_ToggleWidescreen);
    }
    for(int i=0;i<10;i++) {
        assert(FindCmdForSdlKey(SDLK_F1+i,0)==kKeys_Load+i);
        assert(FindCmdForSdlKey(SDLK_F1+i,SDL_KMOD_SHIFT)==kKeys_Save+i);
    }
    keybinds_init("ignored/executable.exe");
    assert(keybinds_get()->p1.a==SDL_SCANCODE_X && keybinds_get()->p2.a==SDL_SCANCODE_X);
    {
        char *fresh=SDL_LoadFile(ini,NULL);assert(fresh);
        assert(strstr(fresh,"[Controller.Keybinds.Player1]") && strstr(fresh,"[Keyboard.Keybinds.Player1]"));
        assert(!strstr(fresh,"[Keybinds.Player2]") && !strstr(fresh,"[Keybinds.Player1]"));SDL_free(fresh);
        keybinds_save();fresh=SDL_LoadFile(ini,NULL);assert(fresh && !strstr(fresh,"[Keybinds.Player2]"));SDL_free(fresh);
    }
    /* Same parser handles BOM, CRLF, case, comments, Unicode and literal slashes. */
    write_file(ini,"\xef\xbb\xbf[General]\r\nRom = \"R:\\ROMs\\a #; Ã§.sfc\" ; comment\r\n"
        "RunAhead = 1\r\n[Graphics]\r\nVSync = 0\r\n[Sound]\r\nVolume = 43\r\n"
        "[KeyMap]\r\nLoad = F2\r\nSave = Ctrl+Shift+F2\r\nPause = F3\r\n"
        "[Keybinds.Player1]\r\na = Q\r\nup = W\r\nb = None\r\n"
        "[keybinds.player2]\r\nA = E\r\nup = I\r\n; preserved comment\r\n");
    ParseConfigFile(ini);keybinds_init(NULL);
    assert(!g_config.vsync && g_config.volume==43 && g_config.run_ahead==1);
    assert(FindCmdForSdlKey(SDLK_F1,0)==0 && FindCmdForSdlKey(SDLK_F1,SDL_KMOD_SHIFT)==0);
    assert(FindCmdForSdlKey(SDLK_F2,0)==kKeys_Load);
    assert(FindCmdForSdlKey(SDLK_F2,SDL_KMOD_SHIFT|SDL_KMOD_CTRL)==kKeys_Save);
    assert(FindCmdForSdlKey(SDLK_F2,SDL_KMOD_ALT)==0);
    assert(FindCmdForSdlKey(SDLK_F2,SDL_KMOD_GUI)==0);
    assert(FindCmdForSdlKey(SDLK_F3,SDL_KMOD_CAPS|SDL_KMOD_NUM)==kKeys_Pause);
    assert(FindCmdForSdlKey(SDLK_p,0)==0);
    assert(FindCmdForSdlKey(SDLK_q,0)==0); /* gameplay is independent */
    uint8_t keys[SDL_SCANCODE_COUNT]={0};keys[SDL_SCANCODE_Q]=1;keys[SDL_SCANCODE_W]=1;
    assert(keybinds_read_player_runner(keys,1)==((1<<8)|(1<<4)));
    assert(keybinds_read_player_runner(keys,2)==0);
    keys[SDL_SCANCODE_E]=1;keys[SDL_SCANCODE_I]=1;
    assert(keybinds_read_player_runner(keys,2)==((1<<8)|(1<<4)));
    assert(keybinds_get()->p1.b==SDL_SCANCODE_UNKNOWN);
    {
        char *migrated=SDL_LoadFile(ini,NULL);assert(migrated);
        assert(strstr(migrated,"[Keyboard.Keybinds.Player1]") && !strstr(migrated,"[Keybinds.Player1]"));SDL_free(migrated);
    }
    char rom[4096];assert(snesrecomp_rom_cache_read(rom,sizeof(rom)));
    assert(!strcmp(rom,"R:\\ROMs\\a #; Ã§.sfc"));
    assert(snesrecomp_rom_cache_write("R:\\new #; Ã§\\game.sfc"));
    keybinds_set_button(2,0,SDL_SCANCODE_K);keybinds_save();
    WriteConfigFile(ini); /* core persistence must preserve ROM and player maps */
    ParseConfigFile(ini);keybinds_init(NULL);
    assert(snesrecomp_rom_cache_read(rom,sizeof(rom)));
    assert(!strcmp(rom,"R:\\new #; Ã§\\game.sfc"));
    assert(keybinds_get()->p1.a==SDL_SCANCODE_Q && keybinds_get()->p2.a==SDL_SCANCODE_K);
    assert(g_config.volume==43 && FindCmdForSdlKey(SDLK_F2,0)==kKeys_Load);
    char *data=SDL_LoadFile(ini,NULL);assert(data && strstr(data,"; preserved comment"));SDL_free(data);
    AckmanIniValue disabled[]={{"KeyMap","Load","None"},{"KeyMap","Save","None"}};
    assert(AckmanConfigUpdate(disabled,2));ConfigReloadKeyMap(ini);
    assert(!FindCmdForSdlKey(SDLK_F1,0) && !FindCmdForSdlKey(SDLK_F2,0));
    char label[128];ConfigFormatShortcut(kKeys_Save,label,sizeof(label));assert(!*label);
    /* Original framework names remain aliases for existing user configs. */
    write_file(ini,"[KeyMap]\nDisplayPerf = F4\nWindowBigger = F5\nWindowSmaller = F6\n");
    ParseConfigFile(ini);
    assert(FindCmdForSdlKey(SDLK_F4,0)==kKeys_DisplayPerf);
    assert(FindCmdForSdlKey(SDLK_F5,0)==kKeys_WindowBigger);
    assert(FindCmdForSdlKey(SDLK_F6,0)==kKeys_WindowSmaller);
    /* Deleting a binding and reparsing cannot retain a previous command. */
    write_file(ini,"[KeyMap]\nPause = Gui+F4\n");ParseConfigFile(ini);
    assert(!FindCmdForSdlKey(SDLK_F3,0) && !FindCmdForSdlKey(SDLK_F2,0));
    assert(FindCmdForSdlKey(SDLK_F4,SDL_KMOD_GUI)==kKeys_Pause);
    write_file(ini,"[General]\nRunAhead = 100\nPauseWhenInactive = 0\n[Graphics]\nOutputMethod = Software\n");
    ParseConfigFile(ini);assert(g_config.run_ahead==4 && !g_config.pause_when_inactive);
    assert(g_config.output_method==kOutputMethod_SDLSoftware);
    write_file(ini,"[General]\nRunAhead = -9\n");ParseConfigFile(ini);
    assert(g_config.run_ahead==0 && g_config.pause_when_inactive);
    write_file(ini,"[General]\nRom = preserved.sfc\n[KeyMap]\nSaveStateMenu = F7\nRewind = F8\nPause = Shift+F4\nReset = Ctrl+F6\nMute = F10\nLoad = F2\nSave = Ctrl+Shift+F2\nSaveSlot10 = \n");
    assert(AckmanConfigMigrateHotkeys());ConfigReloadKeyMap(ini);
    char *canonical=SDL_LoadFile(ini,NULL);assert(canonical && !strstr(canonical,"SaveStateMenu") && !strstr(canonical,"\nLoad =") && !strstr(canonical,"\nSave ="));
    assert(strstr(canonical,"Rewind = 8") && strstr(canonical,"Pause = Shift+4") && strstr(canonical,"Reset = Ctrl+F6") && strstr(canonical,"Rom = preserved.sfc"));
    assert(strstr(canonical,"Mute = 0"));
    assert(strstr(canonical,"SaveSlot10 = \n") && strstr(canonical,"LoadSlot10 = F10"));SDL_free(canonical);
    assert(FindCmdForSdlKey(SDLK_F2,SDL_KMOD_SHIFT|SDL_KMOD_CTRL)==kKeys_Save);
    assert(FindCmdForSdlKey(SDLK_F10,SDL_KMOD_SHIFT)==0);
    assert(FindCmdForSdlKey(SDLK_8,0)==kKeys_Rewind && FindCmdForSdlKey(SDLK_4,SDL_KMOD_SHIFT)==kKeys_Pause);
    char *migration_before=SDL_LoadFile(ini,NULL);assert(AckmanConfigMigrateHotkeys());char *migration_after=SDL_LoadFile(ini,NULL);
    assert(!strcmp(migration_before,migration_after));SDL_free(migration_before);SDL_free(migration_after);
    AckmanIniValue deliberate={"KeyMap","Rewind","F8"};assert(AckmanConfigUpdate(&deliberate,1));assert(AckmanConfigMigrateHotkeys());
    canonical=SDL_LoadFile(ini,NULL);assert(strstr(canonical,"Rewind = F8"));SDL_free(canonical);
    AckmanIniValue runtime[]={ {"Graphics","OutputMethod","OpenGL"},{"Graphics","EnableShaders","1"},{"Graphics","SelectedShader","nested/test.glslp"} };
    const char *active[]={"nested/test.snesmod"};
    assert(AckmanConfigCommitRuntime(runtime,3,active,1));
    char *committed=SDL_LoadFile(ini,NULL);assert(committed && strstr(committed,"SelectedShader = nested/test.glslp") && strstr(committed,"[ActiveMods]\nnested/test.snesmod\n"));
    const char *unsafe[]={"../invalid.snesmod"};runtime[0].value="SDL";
    assert(!AckmanConfigCommitRuntime(runtime,3,unsafe,1));
    char *unchanged=SDL_LoadFile(ini,NULL);assert(unchanged && !strcmp(unchanged,committed));SDL_free(unchanged);SDL_free(committed);
    SDL_RemovePath(ini);SDL_Quit();return 0;
}
