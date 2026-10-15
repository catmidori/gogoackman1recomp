#include "recomp_menu_snes.h"
#include "recomp_menu_controls.h"
#include "recomp_menu_devices.h"
#include "recomp_menu_host.h"
#include "recomp_menu_hotkeys.h"
#include "keybinds.h"
#include "desktop/config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum { PAD_BUTTON, PAD_AXIS, JOY_BUTTON, JOY_AXIS, JOY_HAT, UNBOUND };
typedef struct { int kind,index,direction; } PadBinding;
#define names RecompSnesButtonNames
#define keys RecompSnesButtonKeys
#define keyboard_indices RecompSnesKeyboardIndices
#define runner_bits RecompSnesRunnerBits
static const int defaults[]={SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_GAMEPAD_BUTTON_DPAD_DOWN,
 SDL_GAMEPAD_BUTTON_DPAD_LEFT,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,SDL_GAMEPAD_BUTTON_EAST,
 SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_WEST,
 SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
 SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_START};
static PadBinding bindings[12],pending_pad[12];
static SDL_Scancode pending_keys[12];
static bool active,pad_mode,waiting_release,single,draining;
static int step;
static SDL_JoystickID capture_device;
static PadBinding held;
static SDL_Scancode held_key;

static void format_binding(PadBinding b,char *out,size_t size) {
 if(b.kind==PAD_BUTTON)SDL_strlcpy(out,SDL_GetGamepadStringForButton((SDL_GamepadButton)b.index),size);
 else if(b.kind==PAD_AXIS)SDL_snprintf(out,size,"Axis %s %c",SDL_GetGamepadStringForAxis((SDL_GamepadAxis)b.index),b.direction>0?'+':'-');
 else if(b.kind==JOY_BUTTON)SDL_snprintf(out,size,"Joystick Button %d",b.index);
 else if(b.kind==JOY_AXIS)SDL_snprintf(out,size,"Joystick Axis %d %c",b.index,b.direction>0?'+':'-');
 else if(b.kind==JOY_HAT)SDL_snprintf(out,size,"Joystick Hat %d %d",b.index,b.direction);
 else SDL_strlcpy(out,"",size);
}
static bool parse_binding(const char *value,PadBinding *out) {
 PadBinding b={UNBOUND,0,0};char axis[32],sign;int index,hat;
 if(!*value || !SDL_strcasecmp(value,"None")){*out=b;return true;}
 // SDL's serialized names use a/b/x/y, while older host files use positions.
 SDL_GamepadButton button=SDL_GetGamepadButtonFromString(value);
 if(!SDL_strcasecmp(value,"south"))button=SDL_GAMEPAD_BUTTON_SOUTH;
 else if(!SDL_strcasecmp(value,"east"))button=SDL_GAMEPAD_BUTTON_EAST;
 else if(!SDL_strcasecmp(value,"west"))button=SDL_GAMEPAD_BUTTON_WEST;
 else if(!SDL_strcasecmp(value,"north"))button=SDL_GAMEPAD_BUTTON_NORTH;
 if(button!=SDL_GAMEPAD_BUTTON_INVALID)b=(PadBinding){PAD_BUTTON,button,0};
 else if(sscanf(value,"Axis %31s %c",axis,&sign)==2 && (sign=='+' || sign=='-')) {
  SDL_GamepadAxis a=SDL_GetGamepadAxisFromString(axis);if(a==SDL_GAMEPAD_AXIS_INVALID)return false;
  b=(PadBinding){PAD_AXIS,a,sign=='+'?1:-1};
 } else if(sscanf(value,"Joystick Button %d",&index)==1 && index>=0 && index<256)b=(PadBinding){JOY_BUTTON,index,0};
 else if(sscanf(value,"Joystick Axis %d %c",&index,&sign)==2 && index>=0 && index<256 && (sign=='+' || sign=='-'))b=(PadBinding){JOY_AXIS,index,sign=='+'?1:-1};
 else if(sscanf(value,"Joystick Hat %d %d",&index,&hat)==2 && index>=0 && index<256 && (hat==SDL_HAT_UP || hat==SDL_HAT_DOWN || hat==SDL_HAT_LEFT || hat==SDL_HAT_RIGHT))b=(PadBinding){JOY_HAT,index,hat};
 else return false;
 *out=b;return true;
}
static void read_pad(void *context,const char *section,const char *key,const char *value) {
 bool *found=context;if(SDL_strcasecmp(section,"Controller.Keybinds.Player1"))return;
 *found=true;for(int i=0;i<12;i++)if(!SDL_strcasecmp(key,keys[i]))parse_binding(value,&bindings[i]);
}
static bool save_pad(const PadBinding *map) {
 char text[12][64];RecompIniValue values[12];
 for(int i=0;i<12;i++){format_binding(map[i],text[i],sizeof(text[i]));values[i]=(RecompIniValue){"Controller.Keybinds.Player1",keys[i],text[i]};}
 return RecompMenuServices()->ConfigUpdate(values,12);
}
typedef struct {
 bool canonical[2][2],legacy[2][2];char values[2][2][128];
} ShoulderMigration;
static void read_shoulders(void *context,const char *section,const char *key,const char *value) {
 ShoulderMigration *m=context;int device=-1,button=-1;
 if(!SDL_strcasecmp(section,"Controller.Keybinds.Player1"))device=0;
 if(!SDL_strcasecmp(section,"Keyboard.Keybinds.Player1"))device=1;
 if(device<0)return;
 if(!SDL_strcasecmp(key,"l"))button=0;
 if(!SDL_strcasecmp(key,"r"))button=1;
 if(button>=0){m->canonical[device][button]=true;return;}
 if(!SDL_strcasecmp(key,"q"))button=0;
 if(!SDL_strcasecmp(key,"w"))button=1;
 if(button>=0){m->legacy[device][button]=true;SDL_strlcpy(m->values[device][button],value,128);}
}
static void migrate_shoulders(void) {
 ShoulderMigration m={0};RecompMenuServices()->IniRead(RecompMenuServices()->ConfigPath(),read_shoulders,&m);
 RecompIniValue values[8];int count=0;
 const char *sections[]={"Controller.Keybinds.Player1","Keyboard.Keybinds.Player1"};
 for(int device=0;device<2;device++)for(int button=0;button<2;button++)if(m.legacy[device][button]) {
  if(!m.canonical[device][button])values[count++]=(RecompIniValue){sections[device],button?"r":"l",m.values[device][button]};
  values[count++]=(RecompIniValue){sections[device],button?"w":"q",NULL};
 }
 if(count)RecompMenuServices()->ConfigUpdate(values,count);
}
void RecompControlsInit(void) {
 active=false;draining=false;step=0;migrate_shoulders();keybinds_init(NULL);
 for(int i=0;i<12;i++)bindings[i]=(PadBinding){PAD_BUTTON,defaults[i],0};
 bool found=false;RecompMenuServices()->IniRead(RecompMenuServices()->ConfigPath(),read_pad,&found);
 if(!found)save_pad(bindings);
}
bool RecompControlsActive(void) { return active; }
bool RecompControlsInputBlocked(void) {
 if(active)return true;
 if(!draining)return false;
 SDL_Gamepad *pad=SDL_GetGamepadFromID(capture_device);
 SDL_Joystick *joy=SDL_GetJoystickFromID(capture_device);
 bool pressed=false;
 if(held.kind==PAD_BUTTON && pad)pressed=SDL_GetGamepadButton(pad,(SDL_GamepadButton)held.index);
 else if(held.kind==PAD_AXIS && pad)pressed=abs((int)SDL_GetGamepadAxis(pad,(SDL_GamepadAxis)held.index))>8000;
 else if(held.kind==JOY_BUTTON && joy)pressed=SDL_GetJoystickButton(joy,held.index);
 else if(held.kind==JOY_AXIS && joy)pressed=abs((int)SDL_GetJoystickAxis(joy,held.index))>8000;
 else if(held.kind==JOY_HAT && joy)pressed=SDL_GetJoystickHat(joy,held.index)!=SDL_HAT_CENTERED;
 draining=pressed;return pressed;
}
bool RecompControlsWizard(void) { return active && !single; }
bool RecompControlsCapturing(bool gamepad,int button) { return active && single && pad_mode==gamepad && step==button; }
const char *RecompControlsPrompt(void) { return names[step<12?step:0]; }
void RecompControlsBegin(bool gamepad) {
 active=true;draining=false;single=false;pad_mode=gamepad;step=0;waiting_release=false;capture_device=0;
 memset(pending_keys,0,sizeof(pending_keys));memset(pending_pad,0,sizeof(pending_pad));
}
void RecompControlsCancel(void) {
 if(active && pad_mode && waiting_release)draining=true;
 active=false;step=0;waiting_release=false;
}
void RecompControlsBeginSingle(bool gamepad,int button) {
 if(button<0 || button>=12)return;
 RecompControlsBegin(gamepad);single=true;step=button;
}
static bool save_single(bool gamepad,int button,PadBinding pad,SDL_Scancode key) {
 char text[64];
 if(gamepad)format_binding(pad,text,sizeof(text));
 else SDL_strlcpy(text,key==SDL_SCANCODE_UNKNOWN?"":SDL_GetScancodeName(key),sizeof(text));
 RecompIniValue value={gamepad?"Controller.Keybinds.Player1":"Keyboard.Keybinds.Player1",keys[button],text};
 if(!RecompMenuServices()->ConfigUpdate(&value,1))return false;
 if(gamepad)bindings[button]=pad;else keybinds_set_button(1,keyboard_indices[button],key);
 return true;
}
bool RecompControlsClear(bool gamepad,int button) {
 if(button<0 || button>=12)return false;
 return save_single(gamepad,button,(PadBinding){UNBOUND,0,0},SDL_SCANCODE_UNKNOWN);
}
static void advance(void) {
 // A captured cancel/navigation button must be released before ImGui polls it.
 if(pad_mode && (single || step==11))draining=true;
 if(single) {
  if(!save_single(pad_mode,step,pending_pad[step],pending_keys[step]))
   SDL_Log("[controls] Mapping not applied: unable to persist config.ini");
  RecompControlsCancel();return;
 }
 if(++step!=12)return;
 bool saved;
 if(pad_mode) {
  saved=save_pad(pending_pad);if(saved)memcpy(bindings,pending_pad,sizeof(bindings));
 } else {
  RecompIniValue values[12];
  for(int i=0;i<12;i++)values[i]=(RecompIniValue){"Keyboard.Keybinds.Player1",keys[i],SDL_GetScancodeName(pending_keys[i])};
  saved=RecompMenuServices()->ConfigUpdate(values,12);
  if(saved)for(int i=0;i<12;i++)keybinds_set_button(1,keyboard_indices[i],pending_keys[i]);
 }
 if(!saved)SDL_Log("[controls] Mapping not applied: unable to persist config.ini");
 RecompControlsCancel();
}
/* Wait for neutral/release before the next step: held keys, axis jitter and
 * the paired GAMEPAD/JOYSTICK events must never fill multiple SNES buttons. */
bool RecompControlsEvent(const SDL_Event *e) {
 if(!active) {
  if(!RecompControlsInputBlocked())return false;
  return e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP || e->type==SDL_EVENT_TEXT_INPUT ||
   (e->type>=SDL_EVENT_MOUSE_MOTION && e->type<=SDL_EVENT_MOUSE_WHEEL) ||
   (e->type>=SDL_EVENT_JOYSTICK_AXIS_MOTION && e->type<=SDL_EVENT_JOYSTICK_BUTTON_UP) ||
   (e->type>=SDL_EVENT_GAMEPAD_AXIS_MOTION && e->type<=SDL_EVENT_GAMEPAD_BUTTON_UP);
 }
 if(e->type==SDL_EVENT_KEY_DOWN && e->key.key==SDLK_ESCAPE && !e->key.repeat){RecompControlsCancel();return true;}
 if(single && e->type==SDL_EVENT_KEY_DOWN && e->key.key==SDLK_DELETE && !e->key.repeat) {
  RecompControlsClear(pad_mode,step);RecompControlsCancel();return true;
 }
 if(!pad_mode) {
  if(e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) {
   SDL_Scancode sc=e->key.scancode;
   if(!sc)sc=SDL_GetScancodeFromKey(e->key.key,NULL);
   if(waiting_release) {if(e->type==SDL_EVENT_KEY_UP && sc==held_key)waiting_release=false;}
   else if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat && sc>0 && sc<SDL_SCANCODE_COUNT) {
    pending_keys[step]=sc;held_key=sc;waiting_release=true;advance();
   }
  }
 } else {
  PadBinding candidate={UNBOUND,0,0};SDL_JoystickID id=0;bool pressed=false,released=false;
  switch(e->type) {
   case SDL_EVENT_GAMEPAD_BUTTON_DOWN:case SDL_EVENT_GAMEPAD_BUTTON_UP:
    id=e->gbutton.which;candidate=(PadBinding){PAD_BUTTON,e->gbutton.button,0};pressed=e->gbutton.down;released=!pressed;break;
   case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    id=e->gaxis.which;candidate=(PadBinding){PAD_AXIS,e->gaxis.axis,e->gaxis.value>=0?1:-1};pressed=abs(e->gaxis.value)>16000;released=abs(e->gaxis.value)<8000;break;
   case SDL_EVENT_JOYSTICK_BUTTON_DOWN:case SDL_EVENT_JOYSTICK_BUTTON_UP:
    id=e->jbutton.which;if(SDL_IsGamepad(id))break;
    candidate=(PadBinding){JOY_BUTTON,e->jbutton.button,0};pressed=e->jbutton.down;released=!pressed;break;
   case SDL_EVENT_JOYSTICK_AXIS_MOTION:
    id=e->jaxis.which;if(SDL_IsGamepad(id))break;
    candidate=(PadBinding){JOY_AXIS,e->jaxis.axis,e->jaxis.value>=0?1:-1};pressed=abs(e->jaxis.value)>16000;released=abs(e->jaxis.value)<8000;break;
   case SDL_EVENT_JOYSTICK_HAT_MOTION:
    id=e->jhat.which;if(SDL_IsGamepad(id))break;
    candidate=(PadBinding){JOY_HAT,e->jhat.hat,e->jhat.value};pressed=e->jhat.value==SDL_HAT_UP || e->jhat.value==SDL_HAT_DOWN || e->jhat.value==SDL_HAT_LEFT || e->jhat.value==SDL_HAT_RIGHT;released=e->jhat.value==SDL_HAT_CENTERED;break;
  }
  if(candidate.kind!=UNBOUND && (!capture_device || capture_device==id)) {
   if(waiting_release) {if(released && candidate.kind==held.kind && candidate.index==held.index)waiting_release=false;}
   else if(pressed) {capture_device=id;held=candidate;pending_pad[step]=candidate;waiting_release=true;advance();}
  }
 }
 // Host window/device lifecycle events still reach the runner; all interaction
 // is consumed, including the wrong device, mouse and system hotkeys.
 return e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP || e->type==SDL_EVENT_TEXT_INPUT ||
  (e->type>=SDL_EVENT_MOUSE_MOTION && e->type<=SDL_EVENT_MOUSE_WHEEL) ||
  (e->type>=SDL_EVENT_JOYSTICK_AXIS_MOTION && e->type<=SDL_EVENT_JOYSTICK_BUTTON_UP) ||
  (e->type>=SDL_EVENT_GAMEPAD_AXIS_MOTION && e->type<=SDL_EVENT_GAMEPAD_BUTTON_UP);
}
uint16_t RecompControlsReadDevices(void) {
 SDL_JoystickID ids[32];int count=RecompDevicesJoysticks(ids,32);uint16_t result=0;
 for(int i=0;i<count;i++)result|=RecompControlsReadGamepad(ids[i]);
 return result;
}
uint16_t RecompControlsReadGamepad(SDL_JoystickID device) {
 if(!g_config.enable_gamepad[0] || device==(SDL_JoystickID)-1)return 0;
 SDL_Gamepad *pad=SDL_GetGamepadFromID(device);SDL_Joystick *joy=SDL_GetJoystickFromID(device);
 uint16_t result=0;
 for(int i=0;i<12;i++) {
  PadBinding b=bindings[i];bool pressed=false;
  if(b.kind==PAD_BUTTON && pad) {
   pressed=SDL_GetGamepadButton(pad,(SDL_GamepadButton)b.index);
   // Preserve the standard D-pad/left-stick hybrid for directional bindings.
   int axis=b.index==SDL_GAMEPAD_BUTTON_DPAD_LEFT || b.index==SDL_GAMEPAD_BUTTON_DPAD_RIGHT?SDL_GAMEPAD_AXIS_LEFTX:SDL_GAMEPAD_AXIS_LEFTY;
   int sign=b.index==SDL_GAMEPAD_BUTTON_DPAD_UP || b.index==SDL_GAMEPAD_BUTTON_DPAD_LEFT?-1:1;
   if(b.index>=SDL_GAMEPAD_BUTTON_DPAD_UP && b.index<=SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
    pressed|=sign*(int)SDL_GetGamepadAxis(pad,(SDL_GamepadAxis)axis)>g_config.gamepad_deadzone;
  } else if(b.kind==PAD_AXIS && pad)pressed=b.direction*(int)SDL_GetGamepadAxis(pad,(SDL_GamepadAxis)b.index)>16000;
  else if(joy && b.kind==JOY_BUTTON && b.index<SDL_GetNumJoystickButtons(joy))pressed=SDL_GetJoystickButton(joy,b.index);
  else if(joy && b.kind==JOY_AXIS && b.index<SDL_GetNumJoystickAxes(joy))pressed=b.direction*(int)SDL_GetJoystickAxis(joy,b.index)>16000;
  else if(joy && b.kind==JOY_HAT && b.index<SDL_GetNumJoystickHats(joy))pressed=(SDL_GetJoystickHat(joy,b.index)&b.direction)!=0;
  if(pressed)result|=1u<<runner_bits[i];
 }
 return result;
}

void RecompControlsLabel(bool gamepad,int button,char *out,size_t capacity) {
 if(!capacity)return;*out=0;if(button<0 || button>=12)return;
 if(!gamepad) {
  SDL_Scancode key=keybinds_get_button(1,keyboard_indices[button]);
  if(key!=SDL_SCANCODE_UNKNOWN)SDL_strlcpy(out,SDL_GetScancodeName(key),capacity);
  return;
 }
 char raw[128];format_binding(bindings[button],raw,sizeof(raw));
 RecompGamepadLabel(raw,out,capacity);
}
const char *RecompControlsName(int button) {
 static const char *labels[]={"Up","Down","Left","Right","A","B","X","Y","L","R","Select","Start"};
 return button>=0 && button<12?labels[button]:"";
}
void RecompGamepadLabel(const char *binding,char *out,size_t capacity) {
 if(!capacity)return;*out=0;if(!binding)return;
 if(!SDL_strncasecmp(binding,"Gamepad ",8))binding+=8;
 PadBinding b;const char *label=NULL;
 if(!parse_binding(binding,&b)){SDL_strlcpy(out,binding,capacity);return;}
 if(b.kind==PAD_BUTTON) {
  switch(b.index) {
   case SDL_GAMEPAD_BUTTON_DPAD_UP:label="Dpad UP";break;
   case SDL_GAMEPAD_BUTTON_DPAD_DOWN:label="Dpad DOWN";break;
   case SDL_GAMEPAD_BUTTON_DPAD_LEFT:label="Dpad LEFT";break;
   case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:label="Dpad RIGHT";break;
   case SDL_GAMEPAD_BUTTON_SOUTH:label="A";break;
   case SDL_GAMEPAD_BUTTON_EAST:label="B";break;
   case SDL_GAMEPAD_BUTTON_WEST:label="X";break;
   case SDL_GAMEPAD_BUTTON_NORTH:label="Y";break;
   case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:label="LB / L";break;
   case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:label="RB / R";break;
   case SDL_GAMEPAD_BUTTON_BACK:label="Back / Select";break;
   case SDL_GAMEPAD_BUTTON_START:label="Start";break;
   case SDL_GAMEPAD_BUTTON_LEFT_STICK:label="LS / L3";break;
   case SDL_GAMEPAD_BUTTON_RIGHT_STICK:label="RS / R3";break;
  }
 } else if(b.kind==PAD_AXIS) {
  if(b.index==SDL_GAMEPAD_AXIS_LEFT_TRIGGER && b.direction>0)label="LT / L2";
  else if(b.index==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER && b.direction>0)label="RT / R2";
  else if(b.index==SDL_GAMEPAD_AXIS_LEFTX)label=b.direction<0?"Left analog LEFT":"Left analog RIGHT";
  else if(b.index==SDL_GAMEPAD_AXIS_LEFTY)label=b.direction<0?"Left analog UP":"Left analog DOWN";
  else if(b.index==SDL_GAMEPAD_AXIS_RIGHTX)label=b.direction<0?"Right analog LEFT":"Right analog RIGHT";
  else if(b.index==SDL_GAMEPAD_AXIS_RIGHTY)label=b.direction<0?"Right analog UP":"Right analog DOWN";
 }
 if(label)SDL_strlcpy(out,label,capacity);else format_binding(b,out,capacity);
}
static void add_conflict(char *out,size_t capacity,const char *name) {
 if(!name || !*name)return;
 if(*out)SDL_strlcat(out,", ",capacity);SDL_strlcat(out,name,capacity);
}
size_t RecompControlsConflicts(bool gamepad,int button,RecompConflict *out,size_t capacity) {
 size_t count=0;if(button<0 || button>=12)return 0;
 SDL_Scancode key=keybinds_get_button(1,keyboard_indices[button]);PadBinding b=bindings[button];
 if((gamepad && b.kind==UNBOUND) || (!gamepad && key==SDL_SCANCODE_UNKNOWN))return 0;
 for(int i=0;i<12;i++)if(i!=button) {
  bool shared=gamepad?b.kind==bindings[i].kind && b.index==bindings[i].index && b.direction==bindings[i].direction:
    key==keybinds_get_button(1,keyboard_indices[i]);
  // D-pad bindings accept the left stick too; warn about that physical overlap.
  if(gamepad && !shared) {
   PadBinding first=b,second=bindings[i];if(second.kind==PAD_BUTTON){first=bindings[i];second=b;}
   if(first.kind==PAD_BUTTON && second.kind==PAD_AXIS) {
    int axis=first.index==SDL_GAMEPAD_BUTTON_DPAD_LEFT || first.index==SDL_GAMEPAD_BUTTON_DPAD_RIGHT?SDL_GAMEPAD_AXIS_LEFTX:SDL_GAMEPAD_AXIS_LEFTY;
    int sign=first.index==SDL_GAMEPAD_BUTTON_DPAD_UP || first.index==SDL_GAMEPAD_BUTTON_DPAD_LEFT?-1:1;
    shared=first.index>=SDL_GAMEPAD_BUTTON_DPAD_UP && first.index<=SDL_GAMEPAD_BUTTON_DPAD_RIGHT && second.index==axis && second.direction==sign;
   }
  }
  if(shared)RecompConflictAppend(out,capacity,&count,RecompControlsName(i),gamepad?RECOMP_CONFLICT_GAMEPAD:RECOMP_CONFLICT_KEYBOARD);
 }
 // The same exact tuple comparator and friendly dictionary serve both tabs.
 RecompConflict hotkeys[RECOMP_CONFLICT_MAX];size_t n=RecompHotkeysGameplayConflicts(gamepad,button,hotkeys,RECOMP_CONFLICT_MAX);
 for(size_t i=0;i<n;i++)RecompConflictAppend(out,capacity,&count,hotkeys[i].name,hotkeys[i].origin);
 return count;
}

void RecompControlsConflict(bool gamepad,int button,char *out,size_t capacity) {
 if(!capacity)return;*out=0;RecompConflict entries[RECOMP_CONFLICT_MAX];
 size_t n=RecompControlsConflicts(gamepad,button,entries,RECOMP_CONFLICT_MAX);
 for(size_t i=0;i<n;i++)add_conflict(out,capacity,entries[i].name);
}

void RecompControlsRaw(bool gamepad,int button,char *out,size_t capacity) {
 if(!capacity)return;*out=0;if(button<0 || button>=12)return;
 if(gamepad)format_binding(bindings[button],out,capacity);
 else {SDL_Scancode key=keybinds_get_button(1,keyboard_indices[button]);if(key!=SDL_SCANCODE_UNKNOWN)SDL_strlcpy(out,SDL_GetScancodeName(key),capacity);}
}
