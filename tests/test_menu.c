#ifdef _WIN32
#include <windows.h>
#undef FORCEINLINE
#endif
#include "ackman_menu.h"
#include "ackman_debug.h"
#include "ackman_translation.h"
#include "ackman_controls.h"
#include "ackman_hotkeys.h"
#include "keybinds.h"
#include "common_rtl.h"
#include "ackman_config.h"
#include "ackman_rumble.h"
#include "desktop/config.h"
#include "snes_osd.h"
#include "crc32.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
void Die(const char *format) { (void)format;abort(); }
static float display_scale=1;
static bool global_mouse_override;
static float global_mouse_x,global_mouse_y;
SDL_MouseButtonFlags __real_SDL_GetGlobalMouseState(float *x,float *y);
SDL_MouseButtonFlags __wrap_SDL_GetGlobalMouseState(float *x,float *y) {
    if(!global_mouse_override)return __real_SDL_GetGlobalMouseState(x,y);
    if(x)*x=global_mouse_x;if(y)*y=global_mouse_y;return 0;
}
void TestGlobalMousePosition(bool override,float x,float y) {
    global_mouse_override=override;global_mouse_x=x;global_mouse_y=y;
}
void TestDisplayScale(float scale) { display_scale=scale; }
void TestImGuiInteraction(SDL_Renderer *renderer);
float __wrap_SDL_GetWindowDisplayScale(SDL_Window *w) { (void)w;return display_scale; }
static SDL_SystemTheme theme=SDL_SYSTEM_THEME_LIGHT;
SDL_SystemTheme __wrap_SDL_GetSystemTheme(void) { return theme; }
static int saves,loads,resets;
static int host_hotkey_command,host_hotkey_presses,host_hotkey_releases;
void AckmanHostHotkey(int command,bool pressed) {host_hotkey_command=command;if(pressed)host_hotkey_presses++;else host_hotkey_releases++;}
static char saved[4096],loaded[4096],cached[4096];
static SDL_DialogFileCallback callback;
static void *callback_data;
static char filter_pattern[64],location[4096];
static bool save_dialog;
static int dialog_calls;
static const char *boot_selection;
static int dialog_pauses;
void AckmanPauseForDialog(void) { assert(AckmanMenuDialogActive());dialog_pauses++; }
void AckmanReboot(void) { resets++; }
bool AckmanAutosave(bool save) { (void)save;return true; }
static int forced_checkpoints;
static bool forced_ok=true;
bool AckmanAutosaveForced(bool save) { if(save)forced_checkpoints++;return forced_ok; }
void AckmanVideoSetWidescreen(int active) { assert(active==g_config.ignore_aspect_ratio); }
bool AckmanRomMatches(const uint8_t *data,size_t size) { (void)data;return size==5; }
bool RtlSaveSnapshot(const char *p) {
 saves++;SDL_strlcpy(saved,p,sizeof(saved));SDL_IOStream *f=SDL_IOFromFile(p,"wb");
 assert(f);SDL_WriteIO(f,"state",5);SDL_CloseIO(f);return true;
}
bool RtlLoadSnapshot(const char *p) { loads++;SDL_strlcpy(loaded,p,sizeof(loaded));return true; }
int snesrecomp_rom_cache_read(char *out,size_t n) { SDL_strlcpy(out,cached,n);return *out!=0; }
int snesrecomp_rom_cache_write(const char *p) { SDL_strlcpy(cached,p,sizeof(cached));return 1; }
void __wrap_SDL_ShowOpenFileDialog(SDL_DialogFileCallback cb,void *data,SDL_Window *w,
 const SDL_DialogFileFilter *filters,int n,const char *dir,bool many) {
 if(!w) {
  assert(n==1 && !many && !strcmp(dir,AckmanMenuRomDirectory()));
  const char *files[]={boot_selection,NULL};cb(data,files,0);return;
 }
 assert(w && n==1 && !many && AckmanMenuDialogActive());callback=cb;callback_data=data;
 assert(dialog_pauses==dialog_calls+1);
 save_dialog=false;dialog_calls++;
 SDL_strlcpy(filter_pattern,filters[0].pattern,sizeof(filter_pattern));SDL_strlcpy(location,dir,sizeof(location));
}
void __wrap_SDL_ShowSaveFileDialog(SDL_DialogFileCallback cb,void *data,SDL_Window *w,
 const SDL_DialogFileFilter *filters,int n,const char *dir) {
 assert(w && n==1 && AckmanMenuDialogActive());callback=cb;callback_data=data;save_dialog=true;
 assert(dialog_pauses==dialog_calls+1);dialog_calls++;
 SDL_strlcpy(filter_pattern,filters[0].pattern,sizeof(filter_pattern));SDL_strlcpy(location,dir,sizeof(location));
}
static void drain(void) { SDL_Event e;while(SDL_PollEvent(&e))AckmanMenuEvent(&e); }
static void test_individual_controls(void) {
 char *original=SDL_LoadFile(AckmanConfigPath(),NULL);assert(original);
 char value[128],conflict[256];
 AckmanControlsBeginSingle(false,0);assert(AckmanControlsActive() && !AckmanControlsWizard());
 SDL_Event e={0};e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;e.gbutton.which=123;e.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH;e.gbutton.down=true;
 assert(AckmanControlsEvent(&e) && AckmanControlsCapturing(false,0));
 e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_ESCAPE;
 assert(AckmanControlsEvent(&e) && !AckmanControlsActive());
 char *unchanged=SDL_LoadFile(AckmanConfigPath(),NULL);assert(unchanged && !strcmp(unchanged,original));SDL_free(unchanged);
 e.key.key=SDLK_F2;e.key.scancode=SDL_SCANCODE_F2;
 AckmanControlsBeginSingle(false,0);assert(AckmanControlsEvent(&e) && !AckmanControlsActive());
 assert(keybinds_get_button(1,8)==SDL_SCANCODE_F2);
 AckmanControlsConflict(false,0,conflict,sizeof(conflict));assert(strstr(conflict,"Load") && !strstr(conflict,"Save"));
 AckmanControlsBeginSingle(false,1);assert(AckmanControlsEvent(&e));
 AckmanControlsConflict(false,1,conflict,sizeof(conflict));assert(strstr(conflict,"Up") && strstr(conflict,"Load"));
 // Only identical key AND modifier tuples count as a host conflict.
 SDL_Scancode old_start=keybinds_get_button(1,6);keybinds_set_button(1,6,SDL_SCANCODE_RETURN);
 AckmanControlsConflict(false,11,conflict,sizeof(conflict));assert(!strstr(conflict,"Fullscreen"));
 const char *chords[]={"Alt+Return","Shift+Return","Ctrl+Return","Alt+Shift+Ctrl+Return","Return"};
 for(unsigned i=0;i<sizeof(chords)/sizeof(*chords);i++) {
  AckmanIniValue shortcut={"KeyMap","Fullscreen",chords[i]};assert(AckmanConfigUpdate(&shortcut,1));ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
  AckmanControlsConflict(false,11,conflict,sizeof(conflict));assert((strstr(conflict,"Fullscreen")!=NULL)==(i==4));
 }
 keybinds_set_button(1,6,old_start);
 assert(AckmanControlsClear(false,0));AckmanControlsLabel(false,0,value,sizeof(value));assert(!*value);
 AckmanControlsInit();assert(keybinds_get_button(1,8)==SDL_SCANCODE_UNKNOWN);
 AckmanControlsBeginSingle(false,1);e.key.key=SDLK_DELETE;assert(AckmanControlsEvent(&e) && !AckmanControlsActive());
 AckmanControlsLabel(false,1,value,sizeof(value));assert(!*value);
 const struct {const char *raw,*display;} cases[]={
  {"dpup","Dpad UP"},{"dpdown","Dpad DOWN"},{"dpleft","Dpad LEFT"},{"dpright","Dpad RIGHT"},
  {"a","A"},{"b","B"},{"x","X"},{"y","Y"},{"east","B"},{"south","A"},
  {"leftshoulder","LB / L"},{"rightshoulder","RB / R"},{"back","Back / Select"},{"start","Start"},
  {"Axis lefttrigger +","LT / L2"},{"Axis righttrigger +","RT / R2"},
  {"leftstick","LS / L3"},{"rightstick","RS / R3"},
  {"Axis lefty -","Left analog UP"},{"Axis lefty +","Left analog DOWN"},
  {"Axis leftx -","Left analog LEFT"},{"Axis leftx +","Left analog RIGHT"},
  {"Axis righty -","Right analog UP"},{"Axis righty +","Right analog DOWN"},
  {"Axis rightx -","Right analog LEFT"},{"Axis rightx +","Right analog RIGHT"}
 };
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
  AckmanIniValue bind={"Controller.Keybinds.Player1","up",cases[i].raw};assert(AckmanConfigUpdate(&bind,1));
  AckmanControlsInit();AckmanControlsLabel(true,0,value,sizeof(value));assert(!strcmp(value,cases[i].display));
  char serialized[128],hot_label[128];SDL_snprintf(serialized,sizeof(serialized),"Gamepad %s",cases[i].raw);
  AckmanIniValue shortcut={"KeyMap","Pause",serialized};assert(AckmanConfigUpdate(&shortcut,1));AckmanHotkeysInit();
  AckmanHotkeysLabel(7,hot_label,sizeof(hot_label));assert(!strcmp(hot_label,cases[i].display));
  AckmanControlsConflict(true,0,conflict,sizeof(conflict));assert(strstr(conflict,"Pause"));
  AckmanHotkeysConflict(7,conflict,sizeof(conflict));assert(strstr(conflict,"Up"));
 }
 // Single gamepad capture ignores keyboard, detects hidden L/R conflicts, and
 // Delete persists an empty value which remains empty after reinitialization.
 AckmanControlsBeginSingle(true,0);e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_F1;
 assert(AckmanControlsEvent(&e) && AckmanControlsCapturing(true,0));
 e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;e.gbutton.which=123;e.gbutton.button=SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;e.gbutton.down=true;
 assert(AckmanControlsEvent(&e) && !AckmanControlsActive());
 AckmanControlsConflict(true,0,conflict,sizeof(conflict));assert(strstr(conflict,"L"));
 AckmanControlsBeginSingle(true,0);e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_DELETE;
 assert(AckmanControlsEvent(&e));AckmanControlsInit();AckmanControlsLabel(true,0,value,sizeof(value));assert(!*value);
 // A failed file write must never apply the newly captured key in memory.
 char selected[4096];SDL_strlcpy(selected,AckmanConfigPath(),sizeof(selected));
 assert(SDL_CreateDirectory("controls-write-failure"));assert(!AckmanConfigSelect("controls-write-failure"));
 SDL_Scancode before=keybinds_get_button(1,0);AckmanControlsBeginSingle(false,4);
 e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_F10;e.key.scancode=SDL_SCANCODE_F10;
 assert(AckmanControlsEvent(&e) && !AckmanControlsActive() && keybinds_get_button(1,0)==before);
 assert(AckmanConfigSelect(selected));assert(SDL_RemovePath("controls-write-failure"));
 SDL_IOStream *f=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(f);assert(SDL_WriteIO(f,original,strlen(original))==strlen(original));assert(SDL_CloseIO(f));SDL_free(original);
 AckmanControlsInit();ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
}
static int mod_index(const char *name) {
 for(int i=0;i<AckmanMenuModsCount();i++)if(!strcmp(AckmanMenuModName(i),name))return i;
 assert(0);return -1;
}
static void metadata_fixture(const char *path) {
 const char *manifest="name = \"Metadata preview fixture\"\nauthor = \"Fixture author\"\nversion = \"1.0\"\n"
  "description = \"First paragraph\\n\\nSecond paragraph\\nLine 3\\nLine 4\\nLine 5\\nLine 6\\nLine 7\\nLine 8\\nLine 9\\nLine 10\\nLine 11\\nLine 12\\nFinal line\"\n"
  "[[feature]]\nname = \"Feature name must not replace package name\"\n";
 SDL_IOStream *file=SDL_IOFromFile(path,"wb");assert(file);
 Uint32 size=(Uint32)strlen(manifest),crc=crc32_compute((const uint8_t *)manifest,size),central=43+size;
 assert(SDL_WriteU32LE(file,0x04034b50));
 SDL_WriteU16LE(file,20);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,33);
 SDL_WriteU32LE(file,crc);SDL_WriteU32LE(file,size);SDL_WriteU32LE(file,size);SDL_WriteU16LE(file,13);SDL_WriteU16LE(file,0);
 SDL_WriteIO(file,"manifest.toml",13);SDL_WriteIO(file,manifest,size);
 SDL_WriteU32LE(file,0x02014b50);SDL_WriteU16LE(file,20);SDL_WriteU16LE(file,20);
 SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,33);
 SDL_WriteU32LE(file,crc);SDL_WriteU32LE(file,size);SDL_WriteU32LE(file,size);
 SDL_WriteU16LE(file,13);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);
 SDL_WriteU32LE(file,0);SDL_WriteU32LE(file,0);SDL_WriteIO(file,"manifest.toml",13);
 SDL_WriteU32LE(file,0x06054b50);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,0);SDL_WriteU16LE(file,1);SDL_WriteU16LE(file,1);
 SDL_WriteU32LE(file,59);SDL_WriteU32LE(file,central);SDL_WriteU16LE(file,0);assert(SDL_CloseIO(file));
}
static void test_metadata_reader(void) {
 const char *path="metadata-reader-test.snesmod";metadata_fixture(path);AckmanModMetadata m={0};
 assert(AckmanModMetadataRead(path,&m));assert(!strcmp(m.name,"Metadata preview fixture"));
 assert(!strcmp(m.author,"Fixture author") && !strcmp(m.version,"1.0"));
 assert(strstr(m.description,"First paragraph\n\nSecond paragraph") && strstr(m.description,"\nFinal line"));
 size_t size=0;unsigned char *archive=SDL_LoadFile(path,&size);assert(archive);
 archive[43]^=1;assert(SDL_SaveFile(path,archive,size));assert(!AckmanModMetadataRead(path,&m));
 assert(!m.name && !m.description);archive[43]^=1;
 assert(SDL_SaveFile(path,archive,size-1));assert(!AckmanModMetadataRead(path,&m));
 SDL_free(archive);AckmanModMetadataClear(&m);SDL_RemovePath(path);
}
static void mod_fixtures(bool create) {
 const char *names[]={"ackman-test-alpha.snesmod","ackman-test-beta.snesmod"};
 SDL_CreateDirectory(AckmanMenuModsDirectory());
 for(int i=0;i<2;i++) {
  char path[4096];SDL_snprintf(path,sizeof(path),"%s%s",AckmanMenuModsDirectory(),names[i]);
  if(create && i==0)metadata_fixture(path);
  else if(create){SDL_IOStream *f=SDL_IOFromFile(path,"wb");assert(f);SDL_WriteIO(f,"test",4);SDL_CloseIO(f);}
  else SDL_RemovePath(path);
 }
}
static void test_active_mod_persistence(void) {
 char *original=SDL_LoadFile(AckmanConfigPath(),NULL);assert(original);
 const char *names[]={"ackman-test-alpha.snesmod","ackman-test-beta.snesmod","deleted-package.snesmod"};
 assert(AckmanConfigWriteActiveMods(names,3));AckmanMenuModsInit();
 int alpha=mod_index(names[0]),beta=mod_index(names[1]);
 assert(AckmanMenuModActive(alpha) && AckmanMenuModActive(beta));
 char *clean=SDL_LoadFile(AckmanConfigPath(),NULL);assert(clean && !strstr(clean,names[2]));
 assert(strstr(clean,"[Keyboard.Keybinds.Player1]")<strstr(clean,"[ActiveMods]"));SDL_free(clean);
 AckmanMenuModSelect(alpha);AckmanMenuModTransfer();assert(AckmanMenuModsSave());AckmanMenuModsInit();
 assert(!AckmanMenuModActive(mod_index(names[0])) && AckmanMenuModActive(mod_index(names[1])));
 char *before=SDL_LoadFile(AckmanConfigPath(),NULL);assert(AckmanMenuModsSave());
 char *after=SDL_LoadFile(AckmanConfigPath(),NULL);assert(!strcmp(before,after));SDL_free(before);SDL_free(after);
 const char *unsafe[]={"../outside.snesmod"};assert(!AckmanConfigWriteActiveMods(unsafe,1));
 char path[4096];SDL_snprintf(path,sizeof(path),"%s%s",AckmanMenuModsDirectory(),names[1]);
 assert(SDL_RemovePath(path));AckmanMenuModsInit();assert(!AckmanMenuModsChanged());
 clean=SDL_LoadFile(AckmanConfigPath(),NULL);assert(!strstr(clean,names[1]));SDL_free(clean);mod_fixtures(true);
 SDL_IOStream *f=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(f);SDL_WriteIO(f,original,strlen(original));SDL_CloseIO(f);SDL_free(original);
 AckmanMenuModsInit();
}
static void test_mods(void) {
 test_metadata_reader();
 test_active_mod_persistence();
 char digest[33];AckmanTranslationMd5((const unsigned char *)"",0,digest);assert(!strcmp(digest,"d41d8cd98f00b204e9800998ecf8427e"));
 AckmanTranslationMd5((const unsigned char *)"abc",3,digest);assert(!strcmp(digest,"900150983cd24fb0d6963f7d28e17f72"));
 unsigned char padded[65];memset(padded,'a',sizeof(padded));
 AckmanTranslationMd5(padded,56,digest);assert(!strcmp(digest,"3b0c8ac703f828b04c6c197006d17218"));
 AckmanTranslationMd5(padded,65,digest);assert(!strcmp(digest,"c743a45e0d2e6a95cb859adae0248435"));
 AckmanMenuModsInit();int alpha=mod_index("ackman-test-alpha.snesmod"),beta=mod_index("ackman-test-beta.snesmod");
 assert(!AckmanMenuModsChanged());AckmanMenuModSelect(alpha);AckmanMenuModTransfer();
 assert(AckmanMenuModSelected()==-1 && AckmanMenuModActive(alpha));
 AckmanMenuModsOpened();assert(!AckmanMenuModsChanged());
 AckmanMenuModSelect(beta);AckmanMenuModTransfer();assert(AckmanMenuModsChanged());
 AckmanMenuModSelect(beta);AckmanMenuModTransfer();assert(!AckmanMenuModsChanged());
 AckmanMenuModSelect(alpha);AckmanMenuModTransfer();assert(AckmanMenuModsChanged());
 AckmanMenuModsOpened();assert(!AckmanMenuModsChanged()); // Every opening captures the current activation set.
 AckmanMenuModSelect(alpha);AckmanMenuModTransfer();assert(AckmanMenuModsChanged());
 int old_saves=saves,old_loads=loads;char *before=SDL_LoadFile(AckmanConfigPath(),NULL);assert(before);
 AckmanMenuCommand(ACK_MENU_IPS);assert(AckmanMenuDialogActive() && !save_dialog);
 assert(!strcmp(filter_pattern,"ips") && !strcmp(location,AckmanMenuModsDirectory()));
 const char *files[]={"R:/translation/test.ips",NULL};callback(callback_data,files,0);drain();
 assert(!AckmanMenuDialogActive() && !*AckmanMenuIpsPath() && *AckmanTranslationError() && !AckmanTranslationBusy());
 AckmanMenuCommand(ACK_MENU_IPS);callback(callback_data,NULL,0);drain();
 assert(!AckmanMenuDialogActive() && !*AckmanMenuIpsPath() && *AckmanTranslationError() && !AckmanTranslationBusy());
 char *after=SDL_LoadFile(AckmanConfigPath(),NULL);assert(!strcmp(before,after));SDL_free(before);SDL_free(after);
 assert(saves==old_saves && loads==old_loads); // IPS selection never changes the core or writes a state.
 // A same-size impostor with no IPS signature is rejected before work starts.
 char invalid[4096];SDL_snprintf(invalid,sizeof(invalid),"%s.invalid.ips",AckmanMenuModsDirectory());
 SDL_IOStream *bad=SDL_IOFromFile(invalid,"wb");assert(bad);unsigned char zeros[ACKMAN_TRANSLATION_IPS_SIZE]={0};
 assert(SDL_WriteIO(bad,zeros,sizeof(zeros))==sizeof(zeros));SDL_CloseIO(bad);
 assert(!AckmanTranslationStart(invalid,"unused",AckmanMenuModsDirectory()) && *AckmanTranslationError());SDL_RemovePath(invalid);
 // Optional local integration uses the user-supplied IPS; no patch/ROM fixture is distributed.
 const char *official=SDL_getenv("ACKMAN_TEST_OFFICIAL_IPS"),*rom=SDL_getenv("ACKMAN_TEST_STOCK_ROM");
 if(official && rom) {
  AckmanMenuModSelect(alpha);
  AckmanMenuCommand(ACK_MENU_IPS);const char *official_files[]={official,NULL};callback(callback_data,official_files,0);drain();
  assert(!AckmanMenuDialogActive() && AckmanTranslationBusy() && !strcmp(AckmanMenuIpsPath(),official));
  assert(!AckmanTranslationStart(official,rom,AckmanMenuModsDirectory()));
  Uint64 deadline=SDL_GetTicks()+10000;
  while(AckmanTranslationBusy() && SDL_GetTicks()<deadline){AckmanMenuSync();SDL_Delay(5);}
  assert(!AckmanTranslationBusy() && !*AckmanTranslationError());
  const AckmanModMetadata *metadata=AckmanMenuModMetadata(mod_index(ACKMAN_TRANSLATION_PACKAGE));
  assert(metadata->name && !strcmp(metadata->name,"Go Go Ackman english translation"));
  assert(!strcmp(metadata->author,"Ackman Translations") && !strcmp(metadata->version,"1.0"));
  assert(strstr(metadata->description,"Go Go Ackman1 English patch\nhttp://tasam.com/~voltron/ackman\n\nversion 1.0\n\nContributers:"));
  assert(strstr(metadata->description,"-Jair for having such a wonderful tuturial at http://fly.to/vale to get me started"));
  assert(mod_index(ACKMAN_TRANSLATION_PACKAGE)>=0 && !AckmanMenuModActive(mod_index(ACKMAN_TRANSLATION_PACKAGE)));
  assert(!strcmp(AckmanMenuModName(AckmanMenuModSelected()),"ackman-test-alpha.snesmod"));
  assert(AckmanMenuModActive(mod_index("ackman-test-alpha.snesmod")) && !AckmanMenuModsChanged());
  // Re-running the conversion must update the package without duplicating its row.
  int count=AckmanMenuModsCount();assert(AckmanTranslationStart(official,rom,AckmanMenuModsDirectory()));
  deadline=SDL_GetTicks()+10000;while(AckmanTranslationBusy() && SDL_GetTicks()<deadline){AckmanMenuSync();SDL_Delay(5);}
  assert(!AckmanTranslationBusy() && !*AckmanTranslationError() && AckmanMenuModsCount()==count);
  assert(AckmanTranslationStart(official,"missing-stock-rom.sfc",AckmanMenuModsDirectory()));
  deadline=SDL_GetTicks()+10000;while(AckmanTranslationBusy() && SDL_GetTicks()<deadline){AckmanMenuSync();SDL_Delay(5);}
  assert(!AckmanTranslationBusy() && *AckmanTranslationError() && AckmanMenuModsCount()==count);
 }
 const char *ptbr=SDL_getenv("ACKMAN_TEST_PTBR_IPS");
 if(ptbr && rom) {
  AckmanMenuCommand(ACK_MENU_IPS);const char *ptbr_files[]={ptbr,NULL};callback(callback_data,ptbr_files,0);drain();
  assert(AckmanTranslationBusy());
  Uint64 deadline=SDL_GetTicks()+10000;
  while(AckmanTranslationBusy() && SDL_GetTicks()<deadline){AckmanMenuSync();SDL_Delay(5);}
  assert(!AckmanTranslationBusy() && !*AckmanTranslationError());
  int index=mod_index(ACKMAN_TRANSLATION_PTBR_PACKAGE);assert(index>=0 && !AckmanMenuModActive(index));
  const AckmanModMetadata *meta=AckmanMenuModMetadata(index);
  assert(meta->name && !strcmp(meta->name,"Go Go Ackman PT-BR translation"));
  assert(meta->author && !strcmp(meta->author,"E2P Tradu\xc3\xa7\xc3\xb5" "es"));
  assert(!strcmp(meta->version,"1.0"));
  assert(strstr(meta->description,"Nome do jogo : Go Go Ackman\nEmpresa : Bampresto\nAno : 1994\n"));
  assert(strstr(meta->description,"\"Droga!\""));
  assert(strstr(meta->description,"\nBugs:\n\n") && strstr(meta->description,"www.e2p.cjb.net"));
  assert(strlen(meta->description)>3500 && !strchr(meta->description,'\r'));
 }

}
static void test_recursive_mods(void) {
 char *original=SDL_LoadFile(AckmanConfigPath(),NULL);assert(original);
 char folder[4096],msu[4096],xml[4096],bml[4096],zip[4096],ignored[4096];
 SDL_snprintf(folder,sizeof(folder),"%sackman-test-tree/deep",AckmanMenuModsDirectory());assert(SDL_CreateDirectory(folder));
 SDL_snprintf(msu,sizeof(msu),"%s/music.msu",folder);
 SDL_snprintf(xml,sizeof(xml),"%s/music.xml",folder);SDL_snprintf(bml,sizeof(bml),"%s/music.bml",folder);
 SDL_snprintf(zip,sizeof(zip),"%s/demo.snesmod",folder);SDL_snprintf(ignored,sizeof(ignored),"%s/music-1.pcm",folder);
 assert(SDL_SaveFile(msu,"",0));metadata_fixture(zip);assert(SDL_SaveFile(ignored,"PCM",3));
 const char *xml_text="<package><name>XML Music</name><author>Team &amp; &#65;</author><version>2.0</version><description><![CDATA[First\n\nLast &amp; literal]]></description></package>";
 assert(SDL_SaveFile(xml,xml_text,strlen(xml_text)));AckmanModMetadata metadata={0};
 assert(AckmanModMetadataRead(msu,&metadata));assert(!strcmp(metadata.name,"XML Music"));
 assert(!strcmp(metadata.author,"Team & A") && !strcmp(metadata.version,"2.0"));
 assert(!strcmp(metadata.description,"First\n\nLast &amp; literal"));
 assert(SDL_RemovePath(xml));
 const char *bml_text="board\n  rom name=not-the-title\ninformation\n  title: BML Music\n  author: Composer\n  version: 3.0\n  description:\n    First line\n\n    Last line\n";
 assert(SDL_SaveFile(bml,bml_text,strlen(bml_text)));assert(AckmanModMetadataRead(msu,&metadata));
 assert(!strcmp(metadata.name,"BML Music") && !strcmp(metadata.author,"Composer") && !strcmp(metadata.version,"3.0"));
 assert(!strcmp(metadata.description,"First line\n\nLast line"));AckmanModMetadataClear(&metadata);
 AckmanMenuModsRefresh();
 int alpha=mod_index("ackman-test-alpha.snesmod"),beta=mod_index("ackman-test-beta.snesmod");
 int music=mod_index("ackman-test-tree/deep/music.msu"),nested=mod_index("ackman-test-tree/deep/demo.snesmod");
 for(int i=0;i<AckmanMenuModsCount();i++)assert(AckmanModFilenameValid(AckmanMenuModName(i)));
 assert(!strcmp(AckmanMenuModMetadata(music)->name,"BML Music"));
 AckmanMenuModSetActive(alpha,false);AckmanMenuModSetActive(beta,false);
 AckmanMenuModSelect(-1);AckmanMenuModClick(alpha,false,false);AckmanMenuModClick(music,true,false);
 assert(AckmanMenuModHighlighted(alpha) && AckmanMenuModHighlighted(music) && !AckmanMenuModHighlighted(beta));
 AckmanMenuModClick(alpha,true,false);assert(!AckmanMenuModHighlighted(alpha) && AckmanMenuModHighlighted(music));
 AckmanMenuModClick(music,false,false);assert(AckmanMenuModSelected()==-1 && !AckmanMenuModHighlighted(music));
 AckmanMenuModClick(alpha,false,false);AckmanMenuModClick(music,false,true);
 assert(AckmanMenuModHighlighted(alpha) && AckmanMenuModHighlighted(beta) && AckmanMenuModHighlighted(nested) && AckmanMenuModHighlighted(music));
 AckmanMenuModActivate(music);
 assert(AckmanMenuModActive(alpha) && AckmanMenuModActive(beta) && AckmanMenuModActive(nested) && AckmanMenuModActive(music));
 assert(AckmanMenuModSelected()==-1 && !AckmanMenuModHighlighted(alpha) && !AckmanMenuModHighlighted(music));
 assert(AckmanMenuModsSave());AckmanMenuModsInit();music=mod_index("ackman-test-tree/deep/music.msu");
 assert(AckmanMenuModActive(music));
 assert(!AckmanModFilenameValid("../music.msu") && !AckmanModFilenameValid("pack/../music.msu") && !AckmanModFilenameValid("/pack/music.msu"));
 assert(SDL_RemovePath(msu));assert(SDL_RemovePath(bml));assert(SDL_RemovePath(zip));assert(SDL_RemovePath(ignored));assert(SDL_RemovePath(folder));
 SDL_snprintf(folder,sizeof(folder),"%sackman-test-tree",AckmanMenuModsDirectory());assert(SDL_RemovePath(folder));
 assert(SDL_SaveFile(AckmanConfigPath(),original,strlen(original)));SDL_free(original);AckmanMenuModsInit();
}
static void test_shoulders(void) {
 char *original=SDL_LoadFile(AckmanConfigPath(),NULL);assert(original);
 AckmanIniValue aliases[]={
  {"Controller.Keybinds.Player1","l",NULL},{"Controller.Keybinds.Player1","r",NULL},
  {"Controller.Keybinds.Player1","q","leftshoulder"},{"Controller.Keybinds.Player1","w","rightshoulder"},
  {"Keyboard.Keybinds.Player1","l",NULL},{"Keyboard.Keybinds.Player1","r",NULL},
  {"Keyboard.Keybinds.Player1","q","C"},{"Keyboard.Keybinds.Player1","w","V"}
 };
 assert(AckmanConfigUpdate(aliases,8));AckmanControlsInit();
 char label[128];AckmanControlsLabel(true,8,label,sizeof(label));assert(!strcmp(label,"LB / L"));
 AckmanControlsLabel(true,9,label,sizeof(label));assert(!strcmp(label,"RB / R"));
 assert(keybinds_get_button(1,4)==SDL_SCANCODE_C && keybinds_get_button(1,5)==SDL_SCANCODE_V);
 char *migrated=SDL_LoadFile(AckmanConfigPath(),NULL);assert(strstr(migrated,"l = leftshoulder") && strstr(migrated,"r = rightshoulder"));
 assert(strstr(migrated,"l = C") && strstr(migrated,"r = V") && !strstr(migrated,"\nq = ") && !strstr(migrated,"\nw = "));SDL_free(migrated);
 SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_F16;e.key.scancode=SDL_SCANCODE_F16;
 AckmanControlsBeginSingle(false,8);assert(AckmanControlsEvent(&e));keybinds_init(NULL);assert(keybinds_get_button(1,4)==SDL_SCANCODE_F16);
 uint8_t physical[SDL_SCANCODE_COUNT]={0};physical[SDL_SCANCODE_F16]=1;assert(keybinds_read_player_runner(physical,1)==(1<<10));
 AckmanControlsBeginSingle(false,9);e.key.key=SDLK_ESCAPE;assert(AckmanControlsEvent(&e));assert(keybinds_get_button(1,5)==SDL_SCANCODE_V);
 assert(AckmanControlsClear(false,8));keybinds_init(NULL);assert(keybinds_get_button(1,4)==SDL_SCANCODE_UNKNOWN);
 // An explicitly cleared canonical shoulder wins over an old alias.
 AckmanIniValue old={"Keyboard.Keybinds.Player1","q","C"};assert(AckmanConfigUpdate(&old,1));AckmanControlsInit();assert(keybinds_get_button(1,4)==SDL_SCANCODE_UNKNOWN);
 AckmanControlsBeginSingle(false,9);e.key.key=SDLK_F17;e.key.scancode=SDL_SCANCODE_F17;assert(AckmanControlsEvent(&e));keybinds_init(NULL);assert(keybinds_get_button(1,5)==SDL_SCANCODE_F17);
 memset(physical,0,sizeof(physical));physical[SDL_SCANCODE_F17]=1;assert(keybinds_read_player_runner(physical,1)==(1<<11));
 assert(AckmanControlsClear(true,8) && AckmanControlsClear(true,9));AckmanControlsInit();AckmanControlsLabel(true,8,label,sizeof(label));assert(!*label);
 AckmanControlsLabel(true,9,label,sizeof(label));assert(!*label);
 SDL_IOStream *f=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(f);SDL_WriteIO(f,original,strlen(original));SDL_CloseIO(f);SDL_free(original);
 AckmanControlsInit();AckmanHotkeysInit();
}
static void test_cross_tab_conflicts(void) {
 char *original=SDL_LoadFile(AckmanConfigPath(),NULL);assert(original);
 AckmanIniValue values[]={
  {"Keyboard.Keybinds.Player1","up","U"},{"Keyboard.Keybinds.Player1","down","U"},
  {"KeyMap","ToggleWidescreen","u"},{"KeyMap","ToggleFPS","u"},
  {"KeyMap","VolumeUp","u"},{"KeyMap","SaveSlot10","u"},
  {"KeyMap","Fullscreen","Alt+u"},{"KeyMap","ScaleDown","Ctrl+u"},
  {"Controller.Keybinds.Player1","up","Axis rightx +"},
  {"Controller.Keybinds.Player1","down","Axis rightx +"},
  {"KeyMap","Pause","Gamepad Axis rightx +"},{"KeyMap","Turbo","Gamepad Axis rightx +"},
  {"KeyMap","Mute","Gamepad Axis rightx -"}
 };
 assert(AckmanConfigUpdate(values,sizeof(values)/sizeof(*values)));
 ConfigReloadKeyMap(AckmanConfigPath());AckmanControlsInit();AckmanHotkeysInit();
 char conflict[512],label[128];
 AckmanControlsConflict(false,0,conflict,sizeof(conflict));
 assert(strstr(conflict,"Down") && strstr(conflict,"Widescreen") && strstr(conflict,"Show FPS"));
 assert(strstr(conflict,"Audio volume up") && strstr(conflict,"Save slot 10"));
 assert(!strstr(conflict,"ToggleWidescreen") && !strstr(conflict,"ToggleFPS") && !strstr(conflict,"VolumeUp"));
 assert(!strstr(conflict,"Fullscreen") && !strstr(conflict,"Decrease screen size"));
 AckmanHotkeysConflict(13,conflict,sizeof(conflict));assert(strstr(conflict,"Up") && strstr(conflict,"Down") && strstr(conflict,"Show FPS"));
 AckmanConflict typed[ACK_CONFLICT_MAX];size_t typed_count=AckmanHotkeysConflicts(13,typed,ACK_CONFLICT_MAX);
 bool keyboard_origin=false,host_origin=false;
 for(size_t i=0;i<typed_count;i++) {
  if(!strcmp(typed[i].name,"Up") || !strcmp(typed[i].name,"Down")){assert(typed[i].origin==ACK_CONFLICT_KEYBOARD);keyboard_origin=true;}
  else {assert(typed[i].origin==ACK_CONFLICT_HOTKEY);host_origin=true;}
 }
 assert(keyboard_origin && host_origin);
 typed_count=AckmanControlsConflicts(false,0,typed,ACK_CONFLICT_MAX);
 for(size_t i=0;i<typed_count;i++)assert(typed[i].origin==(!strcmp(typed[i].name,"Down")?ACK_CONFLICT_KEYBOARD:ACK_CONFLICT_HOTKEY));
 assert(AckmanControlsConflicts(false,0,typed,1)==1);
 AckmanControlsConflict(true,0,conflict,sizeof(conflict));assert(strstr(conflict,"Down") && strstr(conflict,"Pause") && strstr(conflict,"Turbo") && !strstr(conflict,"Mute"));
 AckmanHotkeysConflict(7,conflict,sizeof(conflict));assert(strstr(conflict,"Up") && strstr(conflict,"Down") && strstr(conflict,"Turbo") && !strstr(conflict,"Mute"));
 typed_count=AckmanHotkeysConflicts(7,typed,ACK_CONFLICT_MAX);bool pad_origin=false;host_origin=false;
 for(size_t i=0;i<typed_count;i++) {
  if(!strcmp(typed[i].name,"Up") || !strcmp(typed[i].name,"Down")){assert(typed[i].origin==ACK_CONFLICT_GAMEPAD);pad_origin=true;}
  else {assert(typed[i].origin==ACK_CONFLICT_HOTKEY);host_origin=true;}
 }
 assert(pad_origin && host_origin);
 typed_count=AckmanControlsConflicts(true,0,typed,ACK_CONFLICT_MAX);
 for(size_t i=0;i<typed_count;i++)assert(typed[i].origin==(!strcmp(typed[i].name,"Down")?ACK_CONFLICT_GAMEPAD:ACK_CONFLICT_HOTKEY));
 AckmanHotkeysLabel(7,label,sizeof(label));assert(!strcmp(label,"Right analog RIGHT"));
 AckmanControlsLabel(true,0,conflict,sizeof(conflict));assert(!strcmp(label,conflict));
 assert(AckmanControlsClear(true,0));AckmanHotkeysConflict(7,conflict,sizeof(conflict));assert(!strstr(conflict,"Up") && strstr(conflict,"Down"));
 assert(AckmanHotkeysClear(7));AckmanControlsConflict(true,1,conflict,sizeof(conflict));assert(!strstr(conflict,"Pause") && strstr(conflict,"Turbo"));
 SDL_IOStream *f=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(f);SDL_WriteIO(f,original,strlen(original));SDL_CloseIO(f);SDL_free(original);
 ConfigReloadKeyMap(AckmanConfigPath());AckmanControlsInit();AckmanHotkeysInit();
}
static void hot_key(SDL_Keycode key,bool down,SDL_Keymod mods) {
 SDL_Event e={0};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.key=key;e.key.mod=mods;e.key.scancode=SDL_GetScancodeFromKey(key,NULL);
 assert(AckmanHotkeysCaptureEvent(&e));
}
static void test_hotkeys(void) {
 test_shoulders();test_cross_tab_conflicts();
 char *original=SDL_LoadFile(AckmanConfigPath(),NULL);assert(original);
 SDL_IOStream *file=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(file);SDL_WriteIO(file,AckmanDefaultConfig,strlen(AckmanDefaultConfig));SDL_CloseIO(file);
 ConfigReloadKeyMap(AckmanConfigPath());AckmanControlsInit();AckmanHotkeysInit();char label[128],conflict[512];
 assert(!strcmp(AckmanHotkeysName(0),"Audio volume down") && !strcmp(AckmanHotkeysName(13),"Widescreen"));
 for(int i=0;i<10;i++){assert(FindCmdForSdlKey(SDLK_F1+i,0)==kKeys_Load+i);assert(FindCmdForSdlKey(SDLK_F1+i,SDL_KMOD_SHIFT)==kKeys_Save+i);}
 // A modifier already down before capture is not a modifier-first chord.
 AckmanHotkeysBegin(0);hot_key(SDLK_F14,true,SDL_KMOD_CTRL);assert(!AckmanHotkeysActive());hot_key(SDLK_F14,false,0);
 AckmanHotkeysLabel(0,label,sizeof(label));assert(!strcmp(label,"F14"));
 AckmanHotkeysBegin(3);hot_key(SDLK_LCTRL,true,SDL_KMOD_CTRL);assert(AckmanHotkeysActive());hot_key(SDLK_LSHIFT,true,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);
 hot_key(SDLK_F14,true,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);assert(!AckmanHotkeysActive());
 hot_key(SDLK_F14,false,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);hot_key(SDLK_LSHIFT,false,SDL_KMOD_CTRL);hot_key(SDLK_LCTRL,false,0);
 assert(!AckmanHotkeysInputBlocked());AckmanHotkeysLabel(3,label,sizeof(label));assert(!strcmp(label,"Ctrl+Shift+F14"));
 assert(FindCmdForSdlKey(SDLK_F14,SDL_KMOD_CTRL|SDL_KMOD_SHIFT)==kKeys_Fullscreen && FindCmdForSdlKey(SDLK_F14,0)==kKeys_VolumeDown);
 AckmanHotkeysConflict(3,conflict,sizeof(conflict));assert(!strstr(conflict,"Audio volume down"));
 AckmanHotkeysBegin(1);hot_key(SDLK_LSHIFT,true,SDL_KMOD_SHIFT);hot_key(SDLK_LCTRL,true,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);hot_key(SDLK_F14,true,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);
 hot_key(SDLK_F14,false,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);hot_key(SDLK_LCTRL,false,SDL_KMOD_SHIFT);hot_key(SDLK_LSHIFT,false,0);
 AckmanHotkeysConflict(1,conflict,sizeof(conflict));assert(strstr(conflict,"Fullscreen") && !strstr(conflict,"Audio volume down"));
 AckmanHotkeysBegin(3);hot_key(SDLK_LALT,true,SDL_KMOD_ALT);hot_key(SDLK_RETURN,true,SDL_KMOD_ALT);hot_key(SDLK_RETURN,false,SDL_KMOD_ALT);hot_key(SDLK_LALT,false,0);
 AckmanHotkeysConflict(3,conflict,sizeof(conflict));assert(!strstr(conflict,"Start"));
 AckmanHotkeysBegin(7);hot_key(SDLK_RETURN,true,0);hot_key(SDLK_RETURN,false,0);AckmanHotkeysConflict(7,conflict,sizeof(conflict));assert(strstr(conflict,"Start") && !strstr(conflict,"Fullscreen"));
 char *before=SDL_LoadFile(AckmanConfigPath(),NULL);AckmanHotkeysBegin(3);hot_key(SDLK_LCTRL,true,SDL_KMOD_CTRL);hot_key(SDLK_ESCAPE,true,SDL_KMOD_CTRL);
 hot_key(SDLK_ESCAPE,false,SDL_KMOD_CTRL);hot_key(SDLK_LCTRL,false,0);char *after=SDL_LoadFile(AckmanConfigPath(),NULL);assert(!strcmp(before,after));SDL_free(before);SDL_free(after);
 assert(AckmanHotkeysClear(7));AckmanHotkeysLabel(7,label,sizeof(label));assert(!*label);after=SDL_LoadFile(AckmanConfigPath(),NULL);assert(strstr(after,"Pause = \n"));SDL_free(after);
 // Punctuation survives INI comment/array syntax and hot reload.
 AckmanHotkeysBegin(10);hot_key(SDLK_SEMICOLON,true,0);hot_key(SDLK_SEMICOLON,false,0);assert(FindCmdForSdlKey(SDLK_SEMICOLON,0)==kKeys_Screenshot);
 AckmanHotkeysBegin(10);hot_key(SDLK_COMMA,true,0);hot_key(SDLK_COMMA,false,0);assert(FindCmdForSdlKey(SDLK_COMMA,0)==kKeys_Screenshot);
 AckmanHotkeysBegin(12);hot_key(SDLK_LCTRL,true,SDL_KMOD_CTRL);hot_key(SDLK_F15,true,SDL_KMOD_CTRL);hot_key(SDLK_F15,false,SDL_KMOD_CTRL);hot_key(SDLK_LCTRL,false,0);
 SDL_Event turbo={0};turbo.type=SDL_EVENT_KEY_DOWN;turbo.key.key=SDLK_F15;turbo.key.scancode=SDL_SCANCODE_F15;turbo.key.mod=SDL_KMOD_CTRL;assert(AckmanHotkeysHostEvent(&turbo));
 int turbo_releases=host_hotkey_releases;turbo.type=SDL_EVENT_KEY_UP;turbo.key.mod=0;assert(AckmanHotkeysHostEvent(&turbo) && host_hotkey_releases==turbo_releases+1);
 AckmanIniValue rewind_binding={"KeyMap","Rewind","Shift+F16"};assert(AckmanConfigUpdate(&rewind_binding,1));ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
 SDL_Event rw={0};rw.type=SDL_EVENT_KEY_DOWN;rw.key.key=SDLK_F16;rw.key.scancode=SDL_SCANCODE_F16;rw.key.mod=SDL_KMOD_SHIFT;
 assert(AckmanHotkeysHostEvent(&rw) && host_hotkey_command==kKeys_Rewind);int rw_release=host_hotkey_releases;
 rw.type=SDL_EVENT_KEY_UP;rw.key.mod=0;assert(AckmanHotkeysHostEvent(&rw) && host_hotkey_releases==rw_release+1);
 rw.type=SDL_EVENT_KEY_DOWN;rw.key.mod=SDL_KMOD_SHIFT;assert(AckmanHotkeysHostEvent(&rw));rw_release=host_hotkey_releases;
 rw.type=SDL_EVENT_KEY_UP;rw.key.mod=SDL_KMOD_ALT;
 AckmanMenuEvent(&rw);assert(host_hotkey_releases==rw_release+1); /* release survives Alt quarantine */
 int rw_presses=host_hotkey_presses;rw.type=SDL_EVENT_KEY_DOWN;rw.key.mod=SDL_KMOD_SHIFT;rw.key.repeat=true;
 assert(AckmanHotkeysHostEvent(&rw) && host_hotkey_presses==rw_presses);
 // Single gamepad button and signed axis mappings dispatch real host commands.
 SDL_Event e={0};AckmanHotkeysBegin(7);e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;e.gbutton.which=123;e.gbutton.button=SDL_GAMEPAD_BUTTON_EAST;e.gbutton.down=true;assert(AckmanHotkeysCaptureEvent(&e));
 assert(!AckmanHotkeysActive());AckmanHotkeysInputBlocked();AckmanHotkeysLabel(7,label,sizeof(label));assert(!strcmp(label,"B"));
 AckmanHotkeysConflict(7,conflict,sizeof(conflict));assert(strstr(conflict,"A"));
 int presses=host_hotkey_presses;assert(AckmanHotkeysHostEvent(&e));assert(host_hotkey_command==kKeys_Pause && host_hotkey_presses==presses+1);
 assert(AckmanHotkeysHostEvent(&e) && host_hotkey_presses==presses+1);e.type=SDL_EVENT_GAMEPAD_BUTTON_UP;e.gbutton.down=false;assert(AckmanHotkeysHostEvent(&e));
 AckmanHotkeysBegin(12);e=(SDL_Event){0};e.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;e.gaxis.which=123;e.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;e.gaxis.value=24000;assert(AckmanHotkeysCaptureEvent(&e));AckmanHotkeysInputBlocked();
 AckmanHotkeysInit();assert(AckmanHotkeysHostEvent(&e) && host_hotkey_command==kKeys_Turbo);int releases=host_hotkey_releases;
 e.gaxis.value=12000;assert(AckmanHotkeysHostEvent(&e) && host_hotkey_releases==releases);e.gaxis.value=0;assert(AckmanHotkeysHostEvent(&e) && host_hotkey_releases==releases+1);
 // Primary immutable holds and remapped secondary inputs combine with OR.
 SDL_Event primary={0};primary.type=SDL_EVENT_KEY_DOWN;primary.key.key=SDLK_TAB;
 assert(AckmanHotkeysFixedEvent(&primary,true) && host_hotkey_command==kKeys_Turbo);
 releases=host_hotkey_releases;presses=host_hotkey_presses;
 e.gaxis.value=24000;assert(AckmanHotkeysHostEvent(&e));
 primary.type=SDL_EVENT_KEY_UP;assert(AckmanHotkeysFixedEvent(&primary,true));
 assert(host_hotkey_releases==releases && host_hotkey_presses==presses);
 e.gaxis.value=0;assert(AckmanHotkeysHostEvent(&e) && host_hotkey_releases==releases+1);
 primary.type=SDL_EVENT_KEY_DOWN;primary.key.key=SDLK_r;assert(AckmanHotkeysFixedEvent(&primary,true));
 releases=host_hotkey_releases;rw.key.repeat=false;rw.type=SDL_EVENT_KEY_DOWN;rw.key.mod=SDL_KMOD_SHIFT;
 assert(AckmanHotkeysHostEvent(&rw));primary.type=SDL_EVENT_KEY_UP;assert(AckmanHotkeysFixedEvent(&primary,true));
 assert(host_hotkey_releases==releases);rw.type=SDL_EVENT_KEY_UP;rw.key.mod=0;
 assert(AckmanHotkeysHostEvent(&rw) && host_hotkey_releases==releases+1);
 primary.type=SDL_EVENT_KEY_DOWN;primary.key.key=SDLK_F12;presses=host_hotkey_presses;
 assert(AckmanHotkeysFixedEvent(&primary,true) && host_hotkey_command==kKeys_Screenshot && host_hotkey_presses==presses+1);
 primary.key.repeat=true;assert(AckmanHotkeysFixedEvent(&primary,true) && host_hotkey_presses==presses+1);
 primary.type=SDL_EVENT_KEY_UP;assert(AckmanHotkeysFixedEvent(&primary,true));
 primary.type=SDL_EVENT_KEY_DOWN;primary.key.key=SDLK_TAB;primary.key.repeat=false;assert(AckmanHotkeysFixedEvent(&primary,true));
 releases=host_hotkey_releases;primary.type=SDL_EVENT_WINDOW_FOCUS_LOST;
 AckmanHotkeysReleaseRewindEvent(&primary);assert(host_hotkey_releases==releases+1);
 // Clearing a slot is explicit and survives a boot migration/reload.
 assert(AckmanHotkeysClear(33));assert(AckmanConfigMigrateHotkeys());ConfigReloadKeyMap(AckmanConfigPath());assert(!FindCmdForSdlKey(SDLK_F10,0));
 file=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(file);SDL_WriteIO(file,original,strlen(original));SDL_CloseIO(file);SDL_free(original);
 ConfigReloadKeyMap(AckmanConfigPath());AckmanControlsInit();AckmanHotkeysInit();
}
static void test_raw_controls(void) {
 char *original=SDL_LoadFile(AckmanConfigPath(),NULL);assert(original);
 SDL_VirtualJoystickDesc desc={0};SDL_INIT_INTERFACE(&desc);
 desc.type=SDL_JOYSTICK_TYPE_UNKNOWN;desc.naxes=2;desc.nbuttons=12;desc.nhats=1;desc.name="Ackman raw remap test";
 SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);assert(id && !SDL_IsGamepad(id));
 SDL_Joystick *joy=SDL_OpenJoystick(id);assert(joy);
 AckmanControlsBegin(true);
 SDL_Event e={0};e.type=SDL_EVENT_JOYSTICK_AXIS_MOTION;e.jaxis.which=id;e.jaxis.axis=0;e.jaxis.value=-24000;
 assert(AckmanControlsEvent(&e) && !strcmp(AckmanControlsPrompt(),"DOWN"));
 e.jaxis.value=-26000;assert(AckmanControlsEvent(&e) && !strcmp(AckmanControlsPrompt(),"DOWN"));
 e.jaxis.value=0;assert(AckmanControlsEvent(&e));
 e.type=SDL_EVENT_JOYSTICK_HAT_MOTION;e.jhat.which=id;e.jhat.hat=0;e.jhat.value=SDL_HAT_DOWN;
 assert(AckmanControlsEvent(&e) && !strcmp(AckmanControlsPrompt(),"LEFT"));
 e.jhat.value=SDL_HAT_CENTERED;assert(AckmanControlsEvent(&e));
 for(int i=2;i<12;i++) {
  e=(SDL_Event){0};e.type=SDL_EVENT_JOYSTICK_BUTTON_DOWN;e.jbutton.which=id;e.jbutton.button=i;e.jbutton.down=true;
  assert(AckmanControlsEvent(&e));
  if(i<11) {
   char *pending=SDL_LoadFile(AckmanConfigPath(),NULL);assert(pending && !strcmp(pending,original));SDL_free(pending);
   e.type=SDL_EVENT_JOYSTICK_BUTTON_UP;e.jbutton.down=false;assert(AckmanControlsEvent(&e));
  }
 }
 assert(!AckmanControlsActive());AckmanControlsInit();
 assert(SDL_SetJoystickVirtualAxis(joy,0,-24000));SDL_UpdateJoysticks();assert(AckmanControlsReadGamepad(id)==(1<<4));
 assert(SDL_SetJoystickVirtualAxis(joy,0,0));assert(SDL_SetJoystickVirtualHat(joy,0,SDL_HAT_DOWN));SDL_UpdateJoysticks();assert(AckmanControlsReadGamepad(id)==(1<<5));
 assert(SDL_SetJoystickVirtualHat(joy,0,SDL_HAT_CENTERED));assert(SDL_SetJoystickVirtualButton(joy,4,true));SDL_UpdateJoysticks();assert(AckmanControlsReadGamepad(id)==(1<<8));
 // A partial keyboard transaction must not mutate either map on cancellation.
 char *before=SDL_LoadFile(AckmanConfigPath(),NULL);assert(before);
 SDL_Scancode old=keybinds_get_button(1,8);
 AckmanControlsBegin(false);e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_F9;e.key.scancode=SDL_SCANCODE_F9;
 assert(AckmanControlsEvent(&e));assert(keybinds_get_button(1,8)==old);
 e.key.key=SDLK_ESCAPE;e.key.scancode=SDL_SCANCODE_ESCAPE;assert(AckmanControlsEvent(&e) && !AckmanControlsActive());
 char *after=SDL_LoadFile(AckmanConfigPath(),NULL);assert(after && !strcmp(before,after));SDL_free(after);SDL_free(before);
 SDL_CloseJoystick(joy);assert(SDL_DetachVirtualJoystick(id));
 SDL_IOStream *f=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(f);assert(SDL_WriteIO(f,original,strlen(original))==strlen(original));assert(SDL_CloseIO(f));SDL_free(original);
 AckmanControlsInit();
}
void TestCancelSaveDialog(void) {
 assert(save_dialog);const char *cancel[]={NULL};callback(callback_data,cancel,0);drain();drain();
}
static unsigned feedback_calls;
static Uint16 feedback_low,feedback_high;
static bool SDLCALL feedback_motor(void *unused,Uint16 low,Uint16 high) {
 (void)unused;feedback_calls++;feedback_low=low;feedback_high=high;return true;
}
static void test_activation_feedback(void) {
 bool original=g_config.controller_vibration;
 SDL_VirtualJoystickDesc desc={0};SDL_INIT_INTERFACE(&desc);
 desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.nbuttons=16;desc.Rumble=feedback_motor;
 SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);assert(id);
 SDL_Joystick *joy=SDL_OpenJoystick(id);assert(joy);
 SDL_Event event={0};event.type=SDL_EVENT_JOYSTICK_ADDED;event.jdevice.which=id;AckmanRumbleEvent(&event);
 RecompMenuSetControllerVibration(false);unsigned before=feedback_calls;
 RecompMenuSetControllerVibration(true);
 assert(feedback_calls==before+1 && feedback_low==0x8000 && feedback_high==0x8000);
 RecompMenuSetControllerVibration(true);assert(feedback_calls==before+1);
 AckmanRumbleSuspend(true);assert(feedback_calls==before+1);
 SDL_Delay(310);AckmanRumbleSuspend(true);assert(feedback_low==0 && feedback_high==0);
 event.type=SDL_EVENT_JOYSTICK_REMOVED;AckmanRumbleEvent(&event);
 SDL_CloseJoystick(joy);assert(SDL_DetachVirtualJoystick(id));
 g_config.controller_vibration=original;AckmanRumbleSetEnabled(original);AckmanRumbleSuspend(false);
}
int main(int argc,char **argv) {
 assert(AckmanMenuRegister());
 RecompMenuHost original=*RecompMenuServices(),invalid=original;
 invalid.version++;assert(!RecompMenuBindHost(&invalid));
 invalid=original;invalid.ConfigCommitRuntime=NULL;assert(!RecompMenuBindHost(&invalid));
 assert(RecompMenuServices()->ConfigCommitRuntime==original.ConfigCommitRuntime);
 RecompMenuHost alternate=original;alternate.window_id="Reusable SNES test menu";
 alternate.save_slot_pattern="%sOtherGame_save_%02d.sav";
 assert(RecompMenuBindHost(&alternate));
 char alternate_slot[256];RtlSaveSlotPath(0,alternate_slot,sizeof(alternate_slot));
 assert(!strcmp(alternate_slot,"saves/OtherGame_save_01.sav"));
 assert(RecompMenuBindHost(&original));
 SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
 assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
 const char *ini=argc>1?"menu-test-native.ini":"menu-test.ini";
 SDL_RemovePath(ini);ParseConfigFile(ini);
 assert(g_config.output_method==kOutputMethod_OpenGL);
 g_config.output_method=kOutputMethod_SDL; /* UI geometry fixtures use SDL. */
 // Keep legacy-array compatibility fixtures isolated from the canonical slot defaults.
 AckmanIniValue fixture[22];char slot_names[20][32];
 for(int i=0;i<20;i++){SDL_snprintf(slot_names[i],sizeof(slot_names[i]),"%sSlot%d",i<10?"Save":"Load",i%10+1);fixture[i]=(AckmanIniValue){"KeyMap",slot_names[i],NULL};}
 fixture[20]=(AckmanIniValue){"KeyMap","Save","Shift+F1"};fixture[21]=(AckmanIniValue){"KeyMap","Load","F1"};assert(AckmanConfigUpdate(fixture,22));ConfigReloadKeyMap(ini);AckmanHotkeysInit();
 mod_fixtures(true);
 SDL_Window *w=SDL_CreateWindow("Ackman menu test",640,480,SDL_WINDOW_HIDDEN);assert(w);
 AckmanMenuInit(w,NULL);
 {
  char bootpath[4096],result[4096];snprintf(bootpath,sizeof(bootpath),"%stest-rom.sfc",AckmanMenuRomDirectory());
  SDL_IOStream *file=SDL_IOFromFile(bootpath,"wb");assert(file);SDL_WriteIO(file,"valid",5);SDL_CloseIO(file);
  char *args[]={"exe","missing-deleted-rom.sfc"};boot_selection=bootpath;
  assert(AckmanResolveRom(2,args,result,sizeof(result),NULL) && !strcmp(result,bootpath));
  assert(!strcmp(cached,bootpath));
  boot_selection=NULL;assert(!AckmanResolveRom(2,args,result,sizeof(result),NULL));
  SDL_RemovePath(bootpath);
 }
 assert(g_config.pause_when_inactive && AckmanMenuRunAheadMax()==4);
 SDL_Event focus={0};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;focus.window.windowID=SDL_GetWindowID(w);
 AckmanMenuEvent(&focus);assert(AckmanMenuInactive());
 g_config.pause_when_inactive=false;assert(!AckmanMenuInactive());
 g_config.pause_when_inactive=true;assert(AckmanMenuInactive());
 focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;AckmanMenuEvent(&focus);assert(!AckmanMenuInactive());
 AckmanMenuSetRunAhead(999);assert(g_config.run_ahead==4);
 AckmanMenuSetRunAhead(-1);assert(!g_config.run_ahead);
 assert(AckmanMenuTakeSettings()&ACK_SETTINGS_RUNAHEAD);
 assert(!AckmanMenuTakeSettings());
 char p[4096];RtlSaveSlotPath(0,p,sizeof(p));assert(strstr(p,"saves/GGA1_save_01.sav"));SDL_RemovePath(p);
 AckmanMenuCommand(ACK_MENU_LOAD);assert(loads==0);
 SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_F1;
 assert(AckmanMenuEvent(&e));assert(loads==0);
 e.key.mod=SDL_KMOD_SHIFT;assert(AckmanMenuEvent(&e));assert(saves==1 && !strcmp(p,saved));
 AckmanMenuCommand(ACK_MENU_SAVE);assert(saves==2); /* silent overwrite */
 e.key.mod=0;assert(AckmanMenuEvent(&e));assert(loads==1 && !strcmp(p,loaded));
 for(int i=1;i<10;i++) { char suffix[40];sprintf(suffix,"GGA1_save_%02d.sav",i+1);
  AckmanMenuCommand(ACK_MENU_SAVE+i);assert(strstr(saved,suffix));
  AckmanMenuCommand(ACK_MENU_LOAD+i);assert(!strcmp(saved,loaded));SDL_RemovePath(saved);
 }
 SDL_RemovePath(p);
 {
  AckmanIniValue changes[]={{"KeyMap","Save","Ctrl+Shift+F2"},{"KeyMap","Load","F2"}};
  assert(AckmanConfigUpdate(changes,2));ConfigReloadKeyMap(ini);AckmanHotkeysInit();
  assert(FindCmdForSdlKey(SDLK_F1,0)==0);
  assert(FindCmdForSdlKey(SDLK_F1,SDL_KMOD_SHIFT)==0);
  char shortcut[128];AckmanMenuShortcut(true,0,shortcut,sizeof(shortcut));assert(!strcmp(shortcut,"F2"));
  AckmanMenuShortcut(false,0,shortcut,sizeof(shortcut));assert(!strcmp(shortcut,"Ctrl+Shift+F2"));
  int old_saves=saves,old_loads=loads;
  e.key.key=SDLK_F1;e.key.mod=SDL_KMOD_SHIFT;AckmanMenuEvent(&e);assert(saves==old_saves);
  e.key.key=SDLK_F2;e.key.mod=SDL_KMOD_CTRL|SDL_KMOD_SHIFT;assert(AckmanMenuEvent(&e));assert(saves==old_saves+1);
  e.key.repeat=true;assert(AckmanMenuEvent(&e));assert(saves==old_saves+1);e.key.repeat=false;
  e.type=SDL_EVENT_KEY_UP;assert(AckmanMenuEvent(&e));assert(saves==old_saves+1);
  e.type=SDL_EVENT_KEY_DOWN;e.key.mod=SDL_KMOD_CAPS;assert(AckmanMenuEvent(&e));assert(loads==old_loads+1);
  e.key.mod=SDL_KMOD_ALT;AckmanMenuEvent(&e);assert(loads==old_loads+1);
  e.type=SDL_EVENT_KEY_UP;AckmanMenuEvent(&e);e.type=SDL_EVENT_KEY_DOWN;
  SDL_RemovePath(p);
 }
 AckmanMenuCommand(ACK_MENU_ROM);assert(!strcmp(filter_pattern,"sfc;smc") && !strcmp(location,AckmanMenuRomDirectory()));
 assert(AckmanMenuDialogActive());g_config.pause_when_inactive=false;
 SDL_Event regain={0};regain.type=SDL_EVENT_WINDOW_FOCUS_GAINED;AckmanMenuEvent(&regain);
 assert(!AckmanMenuInactive() && AckmanMenuDialogActive());
 const char *cancel[]={NULL};callback(callback_data,cancel,0);
 assert(AckmanMenuDialogActive()); /* callback data is applied on the main loop */
 drain();drain();assert(!AckmanMenuDialogActive());g_config.pause_when_inactive=true;
 AckmanMenuCommand(ACK_MENU_FILE);assert(!strcmp(filter_pattern,"sav") && strstr(location,"saves/"));
 char custom[4096];snprintf(custom,sizeof(custom),"%scustom-\xc3\xa7.sav",location);
 SDL_IOStream *custom_file=SDL_IOFromFile(custom,"wb");assert(custom_file);SDL_CloseIO(custom_file);
 const char *selection[]={custom,NULL};int before=loads;
 callback(callback_data,selection,0);drain();drain();assert(loads==before+1 && !strcmp(loaded,custom));SDL_RemovePath(custom);
 {
  int old_saves=saves;
  AckmanMenuCommand(ACK_MENU_SAVE_FILE);
  assert(save_dialog && !strcmp(filter_pattern,"sav"));
  char expected_dir[4096];snprintf(expected_dir,sizeof(expected_dir),"%ssaves/",SDL_GetBasePath());
  assert(!strcmp(location,expected_dir));
  int calls=dialog_calls;AckmanMenuCommand(ACK_MENU_FILE);assert(dialog_calls==calls);
  TestCancelSaveDialog();assert(saves==old_saves);
  AckmanMenuCommand(ACK_MENU_SAVE_FILE);callback(callback_data,NULL,-1);drain();drain();assert(saves==old_saves);
  AckmanMenuCommand(ACK_MENU_SAVE_FILE);callback(callback_data,selection,0);
  assert(saves==old_saves); /* only the main loop can call the core */
  drain();drain();assert(saves==old_saves+1 && !strcmp(saved,custom));
  size_t length=0;char *state=SDL_LoadFile(custom,&length);assert(state && length==5 && !memcmp(state,"state",5));SDL_free(state);
  AckmanMenuCommand(ACK_MENU_SAVE_FILE);callback(callback_data,selection,0);drain();drain();
  assert(saves==old_saves+2);SDL_RemovePath(custom); /* overwrite */
  char bare[4096],with_ext[4096];snprintf(bare,sizeof(bare),"%scustom-no-extension",location);
  snprintf(with_ext,sizeof(with_ext),"%s.sav",bare);const char *without_extension[]={bare,NULL};
  AckmanMenuCommand(ACK_MENU_SAVE_FILE);callback(callback_data,without_extension,0);drain();drain();
  assert(saves==old_saves+3 && !strcmp(saved,with_ext));SDL_RemovePath(with_ext);
  const char *invalid[]={"invalid.txt",NULL};AckmanMenuCommand(ACK_MENU_SAVE_FILE);
  callback(callback_data,invalid,0);drain();drain();assert(saves==old_saves+3);
 }
 AckmanMenuCommand(ACK_MENU_RESET);assert(resets==0 && AckmanMenuColdBootPending());drain();
 {
  snes_osd_set_fps_visible(0);
  e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_f;
  assert(AckmanMenuEvent(&e) && snes_osd_fps_visible());
  e.key.repeat=true;assert(AckmanMenuEvent(&e) && snes_osd_fps_visible());
  e.type=SDL_EVENT_KEY_UP;e.key.repeat=false;assert(AckmanMenuEvent(&e) && snes_osd_fps_visible());
  AckmanIniValue fps={"KeyMap","ToggleFPS","F9"};assert(AckmanConfigUpdate(&fps,1));ConfigReloadKeyMap(ini);AckmanHotkeysInit();
  e.type=SDL_EVENT_KEY_DOWN;AckmanMenuEvent(&e);assert(snes_osd_fps_visible());
  e.key.key=SDLK_F9;assert(AckmanMenuEvent(&e) && !snes_osd_fps_visible());
  e.type=SDL_EVENT_KEY_UP;assert(AckmanMenuEvent(&e));
  fps.value="f";assert(AckmanConfigUpdate(&fps,1));ConfigReloadKeyMap(ini);AckmanHotkeysInit();
 }
 {
  AckmanIniValue fullscreen={"KeyMap","Fullscreen","Alt+F3"};
  assert(AckmanConfigUpdate(&fullscreen,1));ConfigReloadKeyMap(ini);AckmanHotkeysInit();
  SDL_SetModState(SDL_KMOD_LALT);
  e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_RETURN;
  assert(AckmanMenuEvent(&e) && AckmanMenuAltOwnsInput()); /* unmapped old chord */
  e.key.key=SDLK_F3;assert(!AckmanMenuEvent(&e));
  assert(FindCmdForSdlKey(e.key.key,e.key.mod)==kKeys_Fullscreen);
  SDL_SetModState(0);e.key.key=SDLK_LALT;e.type=SDL_EVENT_KEY_UP;
  assert(AckmanMenuEvent(&e) && AckmanMenuAltOwnsInput());
  e.key.key=SDLK_RETURN;assert(AckmanMenuEvent(&e));
  e.key.key=SDLK_F3;e.key.mod=0;assert(!AckmanMenuEvent(&e));
  assert(e.key.mod&SDL_KMOD_ALT); /* release reaches the original system command */
  assert(!AckmanMenuAltOwnsInput());
  fullscreen.value="Alt+Return";assert(AckmanConfigUpdate(&fullscreen,1));ConfigReloadKeyMap(ini);AckmanHotkeysInit();
  e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_LALT;assert(AckmanMenuEvent(&e));
  e.key.key=SDLK_RALT;assert(AckmanMenuEvent(&e));
  e.type=SDL_EVENT_KEY_UP;e.key.key=SDLK_LALT;assert(AckmanMenuEvent(&e));
  assert(AckmanMenuAltOwnsInput()); /* right Alt still held */
  e.key.key=SDLK_RALT;assert(AckmanMenuEvent(&e));assert(!AckmanMenuAltOwnsInput());
 }
 {
  AckmanMenuInit(w,NULL); /* A new process clears the prior explicit Reboot test latch. */
  SDL_Renderer *renderer=SDL_CreateRenderer(w,"software");assert(renderer);
  assert(AckmanMenuHeight(480)==0 && !AckmanMenuVisible());
  test_activation_feedback();
  int before_mouse=saves,before_load=loads;TestImGuiInteraction(renderer);assert(loads==before_load);
  assert(saves==before_mouse+1 && strstr(saved,"GGA1_save_10.sav"));SDL_RemovePath(saved);
  e=(SDL_Event){0};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_ESCAPE;
  assert(AckmanMenuEvent(&e) && AckmanMenuVisible());
  if(argc>1) {
   assert(SDL_SetWindowFullscreen(w,true));SDL_SyncWindow(w);drain();AckmanMenuSync();
   assert(AckmanMenuVisible() && AckmanMenuHeight(480)==0);
   assert(SDL_SetWindowFullscreen(w,false));SDL_SyncWindow(w);drain();AckmanMenuSync();
   assert(AckmanMenuVisible());
  }
  assert(AckmanMenuEvent(&e) && !AckmanMenuVisible());
  AckmanMenuRendererShutdown();SDL_DestroyRenderer(renderer);
 }
 AckmanMenuShutdown();
 {
  char *contents=SDL_LoadFile(ini,NULL);assert(contents);
  assert(strstr(contents,"DisplayAspect = 8:7"));
  assert(strstr(contents,"PauseWhenInactive = 1") && strstr(contents,"Autosave = 0"));
  assert(strstr(contents,"RunAhead = 0") && strstr(contents,"Fullscreen = 0"));
  assert(strstr(contents,"RewindDepth = 240") && strstr(contents,"RewindInterval = 4"));
  assert(strstr(contents,"VSync = 1") && strstr(contents,"OutputMethod = SDL"));
  assert(strstr(contents,"NoSpriteLimits = 0"));
  assert(strstr(contents,"Widescreen = 1") && strstr(contents,"InternalResolution = 1") && strstr(contents,"WindowSize = 1344x672"));
  assert(strstr(contents,"EnableAudio = 1") && strstr(contents,"Volume = 100") && strstr(contents,"AudioFreq = 48000"));
  assert(!strstr(contents,"FrameBlend =") && !strstr(contents,"LinearFiltering ="));
  SDL_free(contents);ParseConfigFile(ini);assert(g_config.display_aspect==1);
 }
 SDL_strlcpy(cached,"R:/example/rom/game.sfc",sizeof(cached));AckmanMenuInit(w,SDL_getenv("ACKMAN_TEST_STOCK_ROM"));
 AckmanMenuCommand(ACK_MENU_ROM);assert(!strcmp(location,AckmanMenuRomDirectory()));callback(callback_data,cancel,0);drain();drain();
 char rompath[4096];snprintf(rompath,sizeof(rompath),"%smenu-test.sfc",SDL_GetBasePath());
 SDL_IOStream *romfile=SDL_IOFromFile(rompath,"wb");assert(romfile);SDL_WriteIO(romfile,"valid",5);SDL_CloseIO(romfile);
 AckmanMenuCommand(ACK_MENU_ROM);const char *romselection[]={rompath,NULL};callback(callback_data,romselection,0);
 for(int i=0;i<2;i++)while(SDL_PollEvent(&e))AckmanMenuEvent(&e);
 assert(!strcmp(cached,rompath));SDL_RemovePath(rompath);
 AckmanMenuCommand(ACK_MENU_QUIT);bool quit=false;
 for(int i=0;i<2;i++)while(SDL_PollEvent(&e))if(e.type==SDL_EVENT_QUIT)quit=true;
 assert(quit);
 AckmanMenuSetWidescreen(false);AckmanMenuSetWindowSize(9);AckmanMenuSetInternalScale(5);
 AckmanMenuSetAudio(false);AckmanMenuSetVolume(30);AckmanMenuSetSampleRate(44100);
 test_recursive_mods();test_mods();
 g_config.enable_shaders=true;g_config.integer_scaling=true;g_config.screenshot_internal=true;
 SDL_strlcpy(g_config.selected_shader,"missing-shader.glsl",sizeof(g_config.selected_shader));
 AckmanMenuShutdown();ParseConfigFile(ini);
 assert(!g_config.ignore_aspect_ratio && g_config.window_width==4032 && g_config.window_height==2016 && g_config.window_scale==5);
 assert(!g_config.enable_audio && g_config.volume==30 && g_config.audio_freq==44100);
 assert(g_config.enable_shaders && g_config.integer_scaling && !strcmp(g_config.selected_shader,"missing-shader.glsl"));
 assert(g_config.screenshot_internal);
 AckmanMenuShadersRefresh();assert(!*g_config.selected_shader);
 char *shader_ini=SDL_LoadFile(ini,NULL);assert(shader_ini && strstr(shader_ini,"SelectedShader = \n"));SDL_free(shader_ini);
 test_individual_controls();test_raw_controls();test_hotkeys();mod_fixtures(false);
 /* The selector is pending only, and a cancelled change never checkpoints. */
 g_config.output_method=kOutputMethod_SDL;AckmanMenuInit(w,NULL);AckmanMenuOpened();
 int checkpoints=forced_checkpoints;AckmanMenuSetOutputMethod(kOutputMethod_OpenGL);
 assert(AckmanMenuActiveOutput()==kOutputMethod_SDL && !(AckmanMenuTakeSettings()&ACK_SETTINGS_OUTPUT));
 AckmanMenuSetOutputMethod(kOutputMethod_SDL);AckmanMenuClosed();
 assert(forced_checkpoints==checkpoints && !AckmanMenuColdBootPending());
 /* A real change disables shader effects only when leaving OpenGL. */
 g_config.output_method=kOutputMethod_OpenGL;AckmanMenuInit(w,NULL);AckmanMenuOpened();
 g_config.enable_shaders=true;AckmanMenuSetOutputMethod(kOutputMethod_SDLSoftware);
 assert(AckmanMenuActiveOutput()==kOutputMethod_OpenGL && g_config.enable_shaders);
 AckmanMenuClosed();assert(forced_checkpoints==checkpoints+1 && !g_config.enable_shaders && AckmanMenuColdBootPending());
 char *driver_config=SDL_LoadFile(ini,NULL);assert(driver_config && strstr(driver_config,"OutputMethod = SDL-Software") && strstr(driver_config,"EnableShaders = 0"));SDL_free(driver_config);
 drain();assert(AckmanMenuColdBootPending());
 /* Failed checkpoints retain the entry snapshot and prohibit a premature reboot. */
 g_config.output_method=kOutputMethod_OpenGL;g_config.enable_shaders=false;AckmanMenuInit(w,NULL);AckmanMenuOpened();
 char *before_failure=SDL_LoadFile(ini,NULL);assert(before_failure);AckmanMenuSetOutputMethod(kOutputMethod_SDL);
 forced_ok=false;assert(!AckmanMenuClosed() && !AckmanMenuColdBootPending());forced_ok=true;
 char *after_failure=SDL_LoadFile(ini,NULL);assert(after_failure && !strcmp(before_failure,after_failure));SDL_free(after_failure);SDL_free(before_failure);
 assert(AckmanMenuClosed() && AckmanMenuColdBootPending());drain();
 /* Shader widgets are deferred; logical changes matter even with SDL active. */
 g_config.output_method=kOutputMethod_SDL;g_config.enable_shaders=false;
 AckmanMenuInit(w,NULL);AckmanMenuOpened();checkpoints=forced_checkpoints;
 AckmanMenuSetShaders(true);assert(!AckmanMenuActiveShaders());
 AckmanMenuClosed();assert(g_config.enable_shaders && forced_checkpoints==checkpoints+1 && AckmanMenuColdBootPending());
 AckmanMenuClosed();assert(forced_checkpoints==checkpoints+1);drain();
 /* Only a dormant filename delta skips checkpoint, commit and cold boot. */
 g_config.enable_shaders=false;g_config.selected_shader[0]=0;AckmanMenuInit(w,NULL);AckmanMenuOpened();
 checkpoints=forced_checkpoints;AckmanMenuSetShaders(true);AckmanMenuSetShaders(false);
 AckmanMenuClosed();assert(forced_checkpoints==checkpoints && !AckmanMenuColdBootPending());
 SDL_strlcpy(g_config.selected_shader,"pending.glsl",sizeof(g_config.selected_shader));
 char *before_dormant=SDL_LoadFile(ini,NULL);assert(before_dormant);
 assert(!*AckmanMenuActiveShader() && AckmanMenuClosed());
 assert(forced_checkpoints==checkpoints && !AckmanMenuColdBootPending());
 assert(!strcmp(g_config.selected_shader,"pending.glsl")); /* Retain the preference in RAM. */
 char *after_dormant=SDL_LoadFile(ini,NULL);assert(after_dormant && !strcmp(before_dormant,after_dormant));
 SDL_free(before_dormant);SDL_free(after_dormant);
 /* Enabling effects alongside the filename still follows the complete restart. */
 AckmanMenuSetShaders(true);assert(AckmanMenuClosed());
 assert(forced_checkpoints==checkpoints+1 && AckmanMenuColdBootPending());drain();
 /* A filename-only edit with EnableShaders already ON must also restart. */
 g_config.selected_shader[0]=0;AckmanMenuInit(w,NULL);AckmanMenuOpened();checkpoints=forced_checkpoints;
 SDL_strlcpy(g_config.selected_shader,"enabled-selection.glsl",sizeof(g_config.selected_shader));
 assert(AckmanMenuClosed() && forced_checkpoints==checkpoints+1 && AckmanMenuColdBootPending());drain();
 /* Snapshot membership is owned independently, and refreshes every opening. */
 mod_fixtures(true);g_config.enable_shaders=false;AckmanMenuInit(w,NULL);AckmanMenuOpened();AckmanMenuModsOpened();
 int first=-1;for(int j=0;j<AckmanMenuModsCount();j++)if(!strcmp(AckmanMenuModName(j),"ackman-test-alpha.snesmod"))first=j;
 assert(first>=0);checkpoints=forced_checkpoints;AckmanMenuModSelect(first);AckmanMenuModTransfer();
 g_config.enable_shaders=false; /* Snapshot remains OFF; mod changes defeat the exception. */
 SDL_strlcpy(g_config.selected_shader,"mod-and-shader.glsl",sizeof(g_config.selected_shader));
 AckmanMenuClosed();assert(forced_checkpoints==checkpoints+1 && AckmanMenuColdBootPending());drain();
 AckmanMenuInit(w,NULL);AckmanMenuOpened();AckmanMenuModsOpened();assert(!AckmanMenuModsChanged());
 checkpoints=forced_checkpoints;AckmanMenuClosed();assert(forced_checkpoints==checkpoints && !AckmanMenuColdBootPending());
 mod_fixtures(false);
 SDL_DestroyWindow(w);SDL_RemovePath(ini);SDL_Quit();return 0;
}

