#include "recomp_menu_hotkeys.h"
#include "recomp_menu_host.h"
#include "recomp_menu_controls.h"
#include "recomp_menu_core.h"
#include "desktop/config.h"
#include "keybinds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { EMPTY, KEY, PAD_BUTTON, PAD_AXIS };
typedef struct {int kind,index,sign;SDL_Keycode key;SDL_Keymod mods;} Binding;
typedef struct {const char *name,*key;int command;} Hotkey;
static const Hotkey items[]={
 {"Audio volume down","VolumeDown",kKeys_VolumeDown},{"Audio volume up","VolumeUp",kKeys_VolumeUp},
 {"Decrease screen size","ScaleDown",kKeys_WindowSmaller},{"Fullscreen","Fullscreen",kKeys_Fullscreen},
 {"Increase screen size","ScaleUp",kKeys_WindowBigger},{"Menu","Menu",kKeys_Menu},{"Mute","Mute",kKeys_Mute},{"Pause","Pause",kKeys_Pause},
 {"Reset","Reset",kKeys_Reset},{"Rewind","Rewind",kKeys_Rewind},{"Screenshot","Screenshot",kKeys_Screenshot},
 {"Show FPS","ToggleFPS",kKeys_DisplayPerf},{"Turbo","Turbo",kKeys_Turbo},{"Widescreen","ToggleWidescreen",kKeys_ToggleWidescreen},
 {"Save slot 1","SaveSlot1",kKeys_Save+0},
 {"Save slot 2","SaveSlot2",kKeys_Save+1},
 {"Save slot 3","SaveSlot3",kKeys_Save+2},
 {"Save slot 4","SaveSlot4",kKeys_Save+3},
 {"Save slot 5","SaveSlot5",kKeys_Save+4},
 {"Save slot 6","SaveSlot6",kKeys_Save+5},
 {"Save slot 7","SaveSlot7",kKeys_Save+6},
 {"Save slot 8","SaveSlot8",kKeys_Save+7},
 {"Save slot 9","SaveSlot9",kKeys_Save+8},
 {"Save slot 10","SaveSlot10",kKeys_Save+9},
 {"Load slot 1","LoadSlot1",kKeys_Load+0},
 {"Load slot 2","LoadSlot2",kKeys_Load+1},
 {"Load slot 3","LoadSlot3",kKeys_Load+2},
 {"Load slot 4","LoadSlot4",kKeys_Load+3},
 {"Load slot 5","LoadSlot5",kKeys_Load+4},
 {"Load slot 6","LoadSlot6",kKeys_Load+5},
 {"Load slot 7","LoadSlot7",kKeys_Load+6},
 {"Load slot 8","LoadSlot8",kKeys_Load+7},
 {"Load slot 9","LoadSlot9",kKeys_Load+8},
 {"Load slot 10","LoadSlot10",kKeys_Load+9},

 {"Pause dimmed","PauseDimmed",kKeys_PauseDimmed},{"Toggle renderer","ToggleRenderer",kKeys_ToggleRenderer}
};
#define TOTAL ((int)(sizeof(items)/sizeof(*items)))
static Binding bindings[TOTAL],held_pad;
static bool active,first_modifier,drain_pad,pressed[TOTAL];
static bool release_keys[SDL_SCANCODE_COUNT];
static int selected;
static SDL_Keymod held_mods;
static SDL_Keycode last_modifier;
static SDL_JoystickID held_device;
static SDL_Scancode menu_key;
static bool fixed_turbo,fixed_rewind,secondary_turbo,secondary_rewind;
static bool effective_turbo,effective_rewind;
static void update_hold(int command) {
 bool value=command==kKeys_Turbo?(fixed_turbo || secondary_turbo):(fixed_rewind || secondary_rewind);
 bool *previous=command==kKeys_Turbo?&effective_turbo:&effective_rewind;
 if(value!=*previous){*previous=value;RecompMenuServices()->HostHotkey(command,value);}
}
static void secondary_command(int command,bool down) {
#ifdef _WIN32
 if(command==kKeys_Fullscreen && down)RecompMenuUiLockMouseFocus();
#endif
 if(command==kKeys_Turbo){secondary_turbo=down;update_hold(command);}
 else if(command==kKeys_Rewind){secondary_rewind=down;update_hold(command);}
 else RecompMenuServices()->HostHotkey(command,down);
}
bool RecompHotkeysFixedEvent(const SDL_Event *e,bool allow_press) {
 if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST) {
  fixed_turbo=fixed_rewind=false;update_hold(kKeys_Turbo);update_hold(kKeys_Rewind);return false;
 }
 if(e->type!=SDL_EVENT_KEY_DOWN && e->type!=SDL_EVENT_KEY_UP)return false;
 SDL_Keycode key=e->key.key;
 bool down=e->type==SDL_EVENT_KEY_DOWN;
 int command=key==SDLK_TAB?kKeys_Turbo:key==SDLK_r?kKeys_Rewind:key==SDLK_F12?kKeys_Screenshot:0;
 if(!command)return false;
 bool held=command==kKeys_Turbo?fixed_turbo:command==kKeys_Rewind?fixed_rewind:false;
 /* Chords such as Ctrl+R retain their configurable meaning. Releases of a
  * primary latch still drain even after modifiers, focus or menu changed. */
 if(!held && (e->key.mod&(SDL_KMOD_ALT|SDL_KMOD_CTRL|SDL_KMOD_SHIFT|SDL_KMOD_GUI)))return false;
 if(down && !allow_press)return held;
 if(down && e->key.repeat)return true;
 if(command==kKeys_Screenshot){if(down)RecompMenuServices()->HostHotkey(command,true);return true;}
 if(command==kKeys_Turbo)fixed_turbo=down;else fixed_rewind=down;
 update_hold(command);return true;
}
static SDL_Scancode turbo_key;
static SDL_Scancode rewind_key;
static SDL_Keymod rewind_mods;
static SDL_Keymod normalize(SDL_Keymod m) {
 return (m&SDL_KMOD_SHIFT?SDL_KMOD_SHIFT:0)|(m&SDL_KMOD_CTRL?SDL_KMOD_CTRL:0)|
        (m&SDL_KMOD_ALT?SDL_KMOD_ALT:0)|(m&SDL_KMOD_GUI?SDL_KMOD_GUI:0);
}
static SDL_Keymod modifier(SDL_Keycode key) {
 if(key==SDLK_LSHIFT || key==SDLK_RSHIFT)return SDL_KMOD_SHIFT;
 if(key==SDLK_LCTRL || key==SDLK_RCTRL)return SDL_KMOD_CTRL;
 if(key==SDLK_LALT || key==SDLK_RALT)return SDL_KMOD_ALT;
 return 0;
}
static Binding parse(const char *text) {
 Binding b={0};if(!text || !*text || !SDL_strcasecmp(text,"None"))return b;
 if(!SDL_strncasecmp(text,"Gamepad ",8)) {
  text+=8;char axis[32],sign;
  if(sscanf(text,"Axis %31s %c",axis,&sign)==2 && (sign=='+' || sign=='-')) {
   SDL_GamepadAxis index=SDL_GetGamepadAxisFromString(axis);
   if(index!=SDL_GAMEPAD_AXIS_INVALID)b=(Binding){.kind=PAD_AXIS,.index=index,.sign=sign=='+'?1:-1};
  } else {
   SDL_GamepadButton index=SDL_GetGamepadButtonFromString(text);
   if(!SDL_strcasecmp(text,"south"))index=SDL_GAMEPAD_BUTTON_SOUTH;
   else if(!SDL_strcasecmp(text,"east"))index=SDL_GAMEPAD_BUTTON_EAST;
   else if(!SDL_strcasecmp(text,"north"))index=SDL_GAMEPAD_BUTTON_NORTH;
   else if(!SDL_strcasecmp(text,"west"))index=SDL_GAMEPAD_BUTTON_WEST;
   if(index!=SDL_GAMEPAD_BUTTON_INVALID)b=(Binding){.kind=PAD_BUTTON,.index=index};
  }
  return b;
 }
 SDL_Keymod mods=0;
 while(1) {
  if(!SDL_strncasecmp(text,"Ctrl+",5)){mods|=SDL_KMOD_CTRL;text+=5;}
  else if(!SDL_strncasecmp(text,"Alt+",4)){mods|=SDL_KMOD_ALT;text+=4;}
  else if(!SDL_strncasecmp(text,"Shift+",6)){mods|=SDL_KMOD_SHIFT;text+=6;}
  else if(!SDL_strncasecmp(text,"Gui+",4)){mods|=SDL_KMOD_GUI;text+=4;}
  else break;
 }
 SDL_Keycode key=SDL_GetKeyFromName(text);
 if(key!=SDLK_UNKNOWN)b=(Binding){.kind=KEY,.key=key,.mods=normalize(mods)};
 return b;
}
static bool same(Binding a,Binding b) {
 if(a.kind==EMPTY || a.kind!=b.kind)return false;
 if(a.kind==KEY)return a.key==b.key && a.mods==b.mods;
 return a.index==b.index && a.sign==b.sign;
}
static void format(Binding b,char *out,size_t size) {
 if(b.kind==KEY)SDL_snprintf(out,size,"%s%s%s%s%s",b.mods&SDL_KMOD_CTRL?"Ctrl+":"",b.mods&SDL_KMOD_ALT?"Alt+":"",
  b.mods&SDL_KMOD_SHIFT?"Shift+":"",b.mods&SDL_KMOD_GUI?"Gui+":"",SDL_GetKeyName(b.key));
 else if(b.kind==PAD_BUTTON)SDL_snprintf(out,size,"Gamepad %s",SDL_GetGamepadStringForButton((SDL_GamepadButton)b.index));
 else if(b.kind==PAD_AXIS)SDL_snprintf(out,size,"Gamepad Axis %s %c",SDL_GetGamepadStringForAxis((SDL_GamepadAxis)b.index),b.sign>0?'+':'-');
 else SDL_strlcpy(out,"",size);
}
static int item_index(const char *key) {for(int i=0;i<TOTAL;i++)if(!strcmp(items[i].key,key))return i;return -1;}
static void read(void *context,const char *section,const char *key,const char *value) {
 (void)context;if(SDL_strcasecmp(section,"KeyMap"))return;
 for(int i=0;i<TOTAL;i++)if(!SDL_strcasecmp(key,items[i].key)){bindings[i]=parse(value);return;}
 // Compatibility only for pre-migration files; boot converts them to slot keys.
 if(!SDL_strcasecmp(key,"Save"))bindings[item_index("SaveSlot1")]=parse(value);
 if(!SDL_strcasecmp(key,"Load"))bindings[item_index("LoadSlot1")]=parse(value);
 if(!SDL_strcasecmp(key,"DisplayPerf"))bindings[item_index("ToggleFPS")]=parse(value);
 if(!SDL_strcasecmp(key,"WindowBigger"))bindings[item_index("ScaleUp")]=parse(value);
 if(!SDL_strcasecmp(key,"WindowSmaller"))bindings[item_index("ScaleDown")]=parse(value);
}
void RecompHotkeysInit(void) {if(rewind_key){secondary_command(kKeys_Rewind,false);rewind_key=SDL_SCANCODE_UNKNOWN;}if(turbo_key){secondary_command(kKeys_Turbo,false);turbo_key=SDL_SCANCODE_UNKNOWN;}for(int i=0;i<TOTAL;i++)if(pressed[i])secondary_command(items[i].command,false);memset(bindings,0,sizeof(bindings));memset(pressed,0,sizeof(pressed));bindings[item_index("Menu")]=parse("None");RecompMenuServices()->IniRead(RecompMenuServices()->ConfigPath(),read,NULL);}
const char *RecompHotkeysName(int item) {return item>=0 && item<TOTAL?items[item].name:"";}
const char *RecompHotkeysKey(int item) {return item>=0 && item<TOTAL?items[item].key:"";}
void RecompHotkeysRaw(int item,char *out,size_t size) {
 format(item>=0 && item<TOTAL?bindings[item]:(Binding){0},out,size);
}
void RecompHotkeysLabel(int item,char *out,size_t size) {
 Binding b=item>=0 && item<TOTAL?bindings[item]:(Binding){0};
 char raw[128];format(b,raw,sizeof(raw));
 if(b.kind==PAD_BUTTON || b.kind==PAD_AXIS)RecompGamepadLabel(raw,out,size);
 else SDL_strlcpy(out,raw,size);
}
static void conflict_add(char *out,size_t size,const char *name) {if(*out)SDL_strlcat(out,", ",size);SDL_strlcat(out,name,size);}
static Binding gameplay_binding(bool gamepad,int button) {
 char raw[128],pad[144];RecompControlsRaw(gamepad,button,raw,sizeof(raw));
 if(gamepad && *raw){SDL_snprintf(pad,sizeof(pad),"Gamepad %s",raw);SDL_strlcpy(raw,pad,sizeof(raw));}
 return parse(raw);
}
size_t RecompHotkeysGameplayConflicts(bool gamepad,int button,RecompConflict *out,size_t capacity) {
 size_t count=0;Binding b=gameplay_binding(gamepad,button);
 for(int i=0;i<TOTAL;i++)if(same(b,bindings[i]))RecompConflictAppend(out,capacity,&count,items[i].name,RECOMP_CONFLICT_HOTKEY);
 return count;
}
size_t RecompHotkeysConflicts(int item,RecompConflict *out,size_t capacity) {
 size_t count=0;if(item<0 || item>=TOTAL)return 0;
 Binding b=bindings[item];if(b.kind==EMPTY)return 0;
 for(int i=0;i<TOTAL;i++)if(i!=item && same(b,bindings[i]))RecompConflictAppend(out,capacity,&count,items[i].name,RECOMP_CONFLICT_HOTKEY);
 for(int i=0;i<12;i++)if(same(b,gameplay_binding(b.kind!=KEY,i)))
  RecompConflictAppend(out,capacity,&count,RecompControlsName(i),b.kind==KEY?RECOMP_CONFLICT_KEYBOARD:RECOMP_CONFLICT_GAMEPAD);
 return count;
}
void RecompHotkeysGameplayConflict(bool gamepad,int button,char *out,size_t size) {
 if(!size)return;*out=0;RecompConflict entries[RECOMP_CONFLICT_MAX];
 size_t n=RecompHotkeysGameplayConflicts(gamepad,button,entries,RECOMP_CONFLICT_MAX);
 for(size_t i=0;i<n;i++)conflict_add(out,size,entries[i].name);
}
void RecompHotkeysConflict(int item,char *out,size_t size) {
 if(!size)return;*out=0;RecompConflict entries[RECOMP_CONFLICT_MAX];
 size_t n=RecompHotkeysConflicts(item,entries,RECOMP_CONFLICT_MAX);
 for(size_t i=0;i<n;i++)conflict_add(out,size,entries[i].name);
}

bool RecompHotkeysClear(int item) {
 if(item<0 || item>=RECOMP_HOTKEY_COUNT)return false;
 RecompIniValue value={"KeyMap",items[item].key,""};
 if(!RecompMenuServices()->ConfigUpdate(&value,1))return false;
 ConfigReloadKeyMap(RecompMenuServices()->ConfigPath());RecompHotkeysInit();return true;
}
void RecompHotkeysBegin(int item) {
 if(item<0 || item>=RECOMP_HOTKEY_COUNT)return;
 selected=item;active=true;first_modifier=false;held_mods=0;last_modifier=0;
 memset(release_keys,0,sizeof(release_keys));drain_pad=false;
}
void RecompHotkeysCancel(void) {active=false;}
bool RecompHotkeysActive(void) {return active;}
bool RecompHotkeysCapturing(int item) {return active && item==selected;}
bool RecompHotkeysInputBlocked(void) {
 if(active)return true;
 for(int i=1;i<SDL_SCANCODE_COUNT;i++)if(release_keys[i])return true;
 if(drain_pad) {
  SDL_Gamepad *pad=SDL_GetGamepadFromID(held_device);
  drain_pad=pad && (held_pad.kind==PAD_BUTTON?SDL_GetGamepadButton(pad,(SDL_GamepadButton)held_pad.index):abs((int)SDL_GetGamepadAxis(pad,(SDL_GamepadAxis)held_pad.index))>8000);
 }
 return drain_pad;
}
static void commit(Binding binding) {
 char text[128],quoted[132];format(binding,text,sizeof(text));
 const char *stored=text;
 if(strchr(text,';') || strchr(text,'#')){SDL_snprintf(quoted,sizeof(quoted),"\"%s\"",text);stored=quoted;}
 RecompIniValue value={"KeyMap",items[selected].key,stored};
 if(RecompMenuServices()->ConfigUpdate(&value,1)){ConfigReloadKeyMap(RecompMenuServices()->ConfigPath());RecompHotkeysInit();}
 active=false;
}
bool RecompHotkeysCaptureEvent(const SDL_Event *e) {
 if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST) {
  active=false;memset(release_keys,0,sizeof(release_keys));drain_pad=false;return false;
 }
 if(e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) {
  SDL_Scancode sc=e->key.scancode;if(!sc)sc=SDL_GetScancodeFromKey(e->key.key,NULL);
  if(!active && sc>0 && sc<SDL_SCANCODE_COUNT && release_keys[sc]) {
   if(e->type==SDL_EVENT_KEY_UP)release_keys[sc]=false;return true;
  }
  if(active) {
   bool down=e->type==SDL_EVENT_KEY_DOWN;
   if(sc>0 && sc<SDL_SCANCODE_COUNT)release_keys[sc]=down;
   if(down && !e->key.repeat && e->key.key==SDLK_ESCAPE){active=false;return true;}
   if(down && !e->key.repeat && e->key.key==SDLK_DELETE){RecompHotkeysClear(selected);active=false;return true;}
   SDL_Keymod mod=modifier(e->key.key);
   if(mod) {
    if(down){first_modifier=true;held_mods|=mod;last_modifier=e->key.key;}
    else {
     held_mods=0;
     if(release_keys[SDL_SCANCODE_LCTRL] || release_keys[SDL_SCANCODE_RCTRL])held_mods|=SDL_KMOD_CTRL;
     if(release_keys[SDL_SCANCODE_LSHIFT] || release_keys[SDL_SCANCODE_RSHIFT])held_mods|=SDL_KMOD_SHIFT;
     if(release_keys[SDL_SCANCODE_LALT] || release_keys[SDL_SCANCODE_RALT])held_mods|=SDL_KMOD_ALT;
     if(first_modifier && !held_mods)commit((Binding){.kind=KEY,.key=last_modifier});}
   } else if(down && !e->key.repeat)commit((Binding){.kind=KEY,.key=e->key.key,.mods=first_modifier?normalize(held_mods):0});
   return true;
  }
 }
 if(!active) {
  if(drain_pad && RecompHotkeysInputBlocked() && (e->type>=SDL_EVENT_GAMEPAD_AXIS_MOTION && e->type<=SDL_EVENT_GAMEPAD_BUTTON_UP))return true;
  return false;
 }
 if(e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
  held_pad=(Binding){.kind=PAD_BUTTON,.index=e->gbutton.button};held_device=e->gbutton.which;drain_pad=true;commit(held_pad);
 } else if(e->type==SDL_EVENT_GAMEPAD_AXIS_MOTION && abs(e->gaxis.value)>16000) {
  held_pad=(Binding){.kind=PAD_AXIS,.index=e->gaxis.axis,.sign=e->gaxis.value>0?1:-1};held_device=e->gaxis.which;drain_pad=true;commit(held_pad);
 }
 return e->type==SDL_EVENT_TEXT_INPUT || (e->type>=SDL_EVENT_MOUSE_MOTION && e->type<=SDL_EVENT_MOUSE_WHEEL) ||
  (e->type>=SDL_EVENT_GAMEPAD_AXIS_MOTION && e->type<=SDL_EVENT_GAMEPAD_BUTTON_UP) ||
  (e->type>=SDL_EVENT_JOYSTICK_AXIS_MOTION && e->type<=SDL_EVENT_JOYSTICK_BUTTON_UP);
}
void RecompHotkeysReleaseRewindEvent(const SDL_Event *e) {
 if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST || RecompMenuVisible() || RecompMenuDialogActive()) {
  fixed_rewind=false;fixed_turbo=false;update_hold(kKeys_Rewind);update_hold(kKeys_Turbo);
 }
 RecompHotkeysFixedEvent(e,false);
 if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST || e->type==SDL_EVENT_GAMEPAD_REMOVED) {
  if(rewind_key){rewind_key=SDL_SCANCODE_UNKNOWN;secondary_command(kKeys_Rewind,false);}
  for(int i=0;i<TOTAL;i++)if(items[i].command==kKeys_Rewind && pressed[i]){pressed[i]=false;secondary_command(kKeys_Rewind,false);}
 }
 if(e->type==SDL_EVENT_KEY_UP && rewind_key) {
  SDL_Scancode sc=e->key.scancode;if(!sc)sc=SDL_GetScancodeFromKey(e->key.key,NULL);
  if(sc==rewind_key || normalize(e->key.mod)!=rewind_mods){rewind_key=SDL_SCANCODE_UNKNOWN;secondary_command(kKeys_Rewind,false);}
 }
 for(int i=0;i<TOTAL;i++)if(items[i].command==kKeys_Rewind && pressed[i]) {
  Binding b=bindings[i];
  bool release=(b.kind==PAD_BUTTON && e->type==SDL_EVENT_GAMEPAD_BUTTON_UP && b.index==e->gbutton.button) ||
   (b.kind==PAD_AXIS && e->type==SDL_EVENT_GAMEPAD_AXIS_MOTION && b.index==e->gaxis.axis && b.sign*(int)e->gaxis.value<=8000);
  if(release){pressed[i]=false;secondary_command(kKeys_Rewind,false);}
 }
}
bool RecompHotkeysHostEvent(const SDL_Event *e) {
 bool consumed=false,allow_press=!RecompMenuVisible();
 if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST || e->type==SDL_EVENT_GAMEPAD_REMOVED) {
  if(rewind_key){rewind_key=SDL_SCANCODE_UNKNOWN;secondary_command(kKeys_Rewind,false);}
  for(int i=0;i<TOTAL;i++)if(pressed[i]){secondary_command(items[i].command,false);pressed[i]=false;}
 }
 if(e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) {
  SDL_Scancode sc=e->key.scancode;if(!sc)sc=SDL_GetScancodeFromKey(e->key.key,NULL);
  if(e->type==SDL_EVENT_KEY_UP && menu_key==sc && menu_key) {menu_key=SDL_SCANCODE_UNKNOWN;return true;}
  Binding menu=bindings[item_index("Menu")];
  if(menu.kind==KEY && e->key.key==menu.key && normalize(e->key.mod)==menu.mods) {
   if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat){menu_key=sc;RecompMenuUiToggle();}
   return true;
  }
  if(e->type==SDL_EVENT_KEY_UP && rewind_key &&
     (rewind_key==sc || normalize(e->key.mod)!=rewind_mods)) {
   rewind_key=SDL_SCANCODE_UNKNOWN;secondary_command(kKeys_Rewind,false);return true;
  }
  if(allow_press && e->type==SDL_EVENT_KEY_DOWN && FindCmdForSdlKey(e->key.key,e->key.mod)==kKeys_Rewind) {
   if(!e->key.repeat){rewind_key=sc;rewind_mods=normalize(e->key.mod);secondary_command(kKeys_Rewind,true);}
   return true; /* repeats cannot reactivate a chord cancelled by focus/Alt */
  }
  if(e->type==SDL_EVENT_KEY_UP && turbo_key && turbo_key==sc){turbo_key=SDL_SCANCODE_UNKNOWN;secondary_command(kKeys_Turbo,false);return true;}
  if(allow_press && e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat && FindCmdForSdlKey(e->key.key,e->key.mod)==kKeys_Turbo) {
   turbo_key=sc;secondary_command(kKeys_Turbo,true);return true;
  }
 }
 for(int i=0;i<TOTAL;i++) {
  Binding b=bindings[i];bool matches=false,down=false;
  if(b.kind==PAD_BUTTON && (e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type==SDL_EVENT_GAMEPAD_BUTTON_UP)) {
   matches=b.index==e->gbutton.button;down=e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  } else if(b.kind==PAD_AXIS && e->type==SDL_EVENT_GAMEPAD_AXIS_MOTION) {
   matches=b.index==e->gaxis.axis;down=b.sign*(int)e->gaxis.value>(pressed[i]?8000:16000);
  }
  if(matches) {
   if(!allow_press && !pressed[i] && items[i].command!=kKeys_Menu)continue;
   if(down!=pressed[i]) {if(items[i].command==kKeys_Menu){if(down)RecompMenuUiToggle();}else secondary_command(items[i].command,down);}
   pressed[i]=down;consumed=true;
  }
 }
 return consumed;
}
