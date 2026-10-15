#include "ackman_config.h"
#include "desktop/sdl_compat.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Single selected file for the host, ROM picker and controller editor.
 * --config is an explicit alternative for isolated runs, never an overlay. */
static char config_path[4096] = "config.ini";
const char AckmanDefaultConfig[] =
    "[General]\nRom = \"\"\nAutosave = 0\nPauseWhenInactive = 1\nControllerVibration = 1\nDisableFrameDelay = 0\nRunAhead = 0\nRewindDepth = 240\nRewindInterval = 4\n\n"
    "[Graphics]\nVSync = 1\nOutputMethod = OpenGL\nWindowSize = 1344x672\nInternalResolution = 1\n"
    "Fullscreen = 0\nNewRenderer = 1\nDisplayAspect = 4:3\nWidescreen = 1\nNoSpriteLimits = 0\nIntegerScaling = 0\nScreenshotResolution = WindowSize\nEnableShaders = 0\nSelectedShader = \n\n"
    "[Sound]\nEnableAudio = 1\nVolume = 100\nAudioFreq = 48000\nAudioChannels = 2\nAudioSamples = 512\n\n"
    "[KeyMap]\nMenu = None\n"
    "Pause = p\n"
    "SaveSlot1 = Shift+F1\n"
    "SaveSlot2 = Shift+F2\n"
    "SaveSlot3 = Shift+F3\n"
    "SaveSlot4 = Shift+F4\n"
    "SaveSlot5 = Shift+F5\n"
    "SaveSlot6 = Shift+F6\n"
    "SaveSlot7 = Shift+F7\n"
    "SaveSlot8 = Shift+F8\n"
    "SaveSlot9 = Shift+F9\n"
    "SaveSlot10 = Shift+F10\n"
    "LoadSlot1 = F1\n"
    "LoadSlot2 = F2\n"
    "LoadSlot3 = F3\n"
    "LoadSlot4 = F4\n"
    "LoadSlot5 = F5\n"
    "LoadSlot6 = F6\n"
    "LoadSlot7 = F7\n"
    "LoadSlot8 = F8\n"
    "LoadSlot9 = F9\n"
    "LoadSlot10 = F10\n"
    "Fullscreen = Alt+Return\n"
    "ToggleFPS = f\n"
    "Turbo = Tab\n"
    "Mute = m\n"
    "ScaleUp = o\n"
    "ScaleDown = i\n"
    "Reset = Ctrl+r\n"
    "PauseDimmed = Shift+p\n"
    "VolumeUp = Keypad +\n"
    "VolumeDown = Keypad -\n"
    "ToggleRenderer = r\n"
    "ToggleWidescreen = u\n"
    "Rewind = 8\n"
    "Screenshot = None\n"
    "\n"
    "[Controller.Keybinds.Player1]\nup = dpup\ndown = dpdown\nleft = dpleft\nright = dpright\n"
    "a = east\nb = south\nx = north\ny = west\nl = leftshoulder\nr = rightshoulder\nselect = back\nstart = start\n\n"
    "[Keyboard.Keybinds.Player1]\nup = Up\ndown = Down\nleft = Left\nright = Right\n"
    "a = X\nb = Z\nx = S\ny = A\nl = C\nr = V\nselect = Right Shift\nstart = Return\n\n[ActiveMods]\n";

const char *AckmanConfigPath(void) { return config_path; }
bool AckmanConfigSelect(const char *path) {
    if (!path) path = "config.ini";
    if (strlen(path) >= sizeof(config_path)) return false;
    SDL_strlcpy(config_path, path, sizeof(config_path));
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path, &info)) return info.type == SDL_PATHTYPE_FILE;
    SDL_IOStream *f = SDL_IOFromFile(path, "wb");
    if (!f) return false;
    bool ok = SDL_WriteIO(f, AckmanDefaultConfig, strlen(AckmanDefaultConfig)) == strlen(AckmanDefaultConfig);
    return SDL_CloseIO(f) && ok;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) ++s;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = 0;
    return s;
}
/* Quotes protect #/; in ROM paths; backslashes are literal Windows paths. */
static char *parse(char *line, char *section, size_t capacity, char **value) {
    char *s = trim(line);
    if (!strncmp(s, "\xef\xbb\xbf", 3)) s += 3;
    if (!*s || *s == '#' || *s == ';') return NULL;
    bool quoted = false;
    for (char *p = s; *p; ++p) {
        if (*p == '"') quoted = !quoted;
        if (!quoted && (*p == '#' || *p == ';')) { *p = 0; break; }
    }
    if (*s == '[') {
        char *end = strchr(s, ']');
        if (end) { *end = 0; SDL_strlcpy(section, trim(s+1), capacity); }
        else section[0] = 0;
        return NULL;
    }
    char *eq = strchr(s, '=');
    if (!eq) {
        if(!SDL_strcasecmp(section,"ActiveMods") && *s){*value="";return trim(s);}
        return NULL;
    }
    *eq = 0; *value = trim(eq+1);
    size_t n = strlen(*value);
    if (n >= 2 && (*value)[0] == '"' && (*value)[n-1] == '"') {
        (*value)[n-1] = 0; ++*value;
    }
    return trim(s);
}
bool AckmanIniRead(const char *path, AckmanIniHandler handler, void *context) {
    char *data = SDL_LoadFile(path, NULL);
    if (!data) return false;
    char section[128] = "";
    for (char *s = data; *s;) {
        char *end = strchr(s, '\n');
        if (end) *end = 0;
        char *value, *key = parse(s, section, sizeof(section), &value);
        if (key) handler(context, section, key, value);
        if (!end) break;
        s = end+1;
    }
    SDL_free(data); return true;
}

static bool emit(SDL_IOStream *f, const char *s) {
    return SDL_WriteIO(f, s, strlen(s)) == strlen(s);
}
static bool flush_section(SDL_IOStream *f, const char *section,
                          const AckmanIniValue *values, bool *done, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!done[i] && !SDL_strcasecmp(section, values[i].section)) {
            if (!values[i].value) {done[i]=true;continue;}
            if (!emit(f, values[i].key) || !emit(f, " = ") ||
                !emit(f, values[i].value) || !emit(f, "\n")) return false;
            done[i] = true;
        }
    }
    return true;
}
/* Preserve unrelated sections/comments. Commit only after the complete file
 * has been written successfully; failed writes leave the original intact. */
static bool update_file(const char *path,const AckmanIniValue *values,size_t count,
                               const char *const *sections,size_t section_count) {
    char *data = SDL_LoadFile(path, NULL);
    if (!data) return false;
    bool *done = SDL_calloc(count ? count : 1, sizeof(bool));
    char temporary[4112]; SDL_snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    SDL_IOStream *f = done ? SDL_IOFromFile(temporary, "wb") : NULL;
    if (!f) { SDL_free(data); SDL_free(done); return false; }
    bool ok = true;
    char section[128] = "";
    for (char *s = data; *s && ok;) {
        char *end = strchr(s, '\n'); if (end) *end = 0;
        char *copy = SDL_strdup(s);
        if (!copy) { ok = false; break; }
        char next[128]; SDL_strlcpy(next, section, sizeof(next));
        char *value, *key = parse(copy, next, sizeof(next), &value);
        if (SDL_strcasecmp(next, section)) {
            ok = flush_section(f, section, values, done, count);
            SDL_strlcpy(section, next, sizeof(section));
        }
        bool matched = false;
        for (size_t i = 0; key && i < count; ++i) {
            if (!SDL_strcasecmp(section, values[i].section) && !SDL_strcasecmp(key, values[i].key)) {
                /* Collapse duplicate definitions of an updated key. */
                if (!done[i] && values[i].value) ok = ok && emit(f, values[i].key) && emit(f, " = ") &&
                    emit(f, values[i].value) && emit(f, "\n");
                done[i] = true; matched = true; break;
            }
        }
        bool removed=false;
        for(size_t i=0;i<section_count;i++)if(!SDL_strcasecmp(section,sections[i]))removed=true;
        if (!matched && !removed) ok = ok && emit(f, s) && emit(f, "\n");
        SDL_free(copy);
        if (!end) break;
        s = end+1;
    }
    ok = ok && flush_section(f, section, values, done, count);
    for (size_t i = 0; i < count && ok; ++i) if (!done[i]) {
        ok = emit(f, "\n[") && emit(f, values[i].section) && emit(f, "]\n") &&
            flush_section(f, values[i].section, values, done, count);
    }
    if (!SDL_CloseIO(f)) ok = false;
    if (ok) ok = SDL_RenamePath(temporary, path);
    if (!ok) SDL_RemovePath(temporary);
    SDL_free(data); SDL_free(done); return ok;
}
bool AckmanConfigUpdateRemoving(const AckmanIniValue *values,size_t count,
                               const char *const *sections,size_t section_count) {
    return update_file(config_path,values,count,sections,section_count);
}
bool AckmanModFilenameValid(const char *name) {
    size_t length=name?strlen(name):0;
    if(length<5 || length>1023 || name[0]=='/' ||
       (SDL_strcasecmp(name+length-4,".msu") && (length<9 || SDL_strcasecmp(name+length-8,".snesmod"))))return false;
    for(const unsigned char *s=(const unsigned char *)name;*s;s++)
        if(*s<32 || strchr("\\:=#;\"[]",*s))return false;
    const char *part=name;
    for(const char *p=name;;p++)if(*p=='/' || !*p) {
        size_t count=(size_t)(p-part);
        if(!count || count>255 || part[0]=='.' || part[count-1]==' ' || part[count-1]=='.')return false;
        if(!*p)break;part=p+1;
    }
    return true;
}
static bool emit_active_mods(SDL_IOStream *f,const char *const *names,size_t count) {
    if(!emit(f,"\n[ActiveMods]\n"))return false;
    for(size_t i=0;i<count;i++)if(!emit(f,names[i]) || !emit(f,"\n"))return false;
    return true;
}
typedef struct { const char *const *names;size_t count,index;bool equal; } ActiveComparison;
static void compare_active(void *context,const char *section,const char *name,const char *value) {
    (void)value;ActiveComparison *comparison=context;
    if(SDL_strcasecmp(section,"ActiveMods"))return;
    if(comparison->index>=comparison->count || strcmp(name,comparison->names[comparison->index]))comparison->equal=false;
    comparison->index++;
}
static bool write_mods_file(const char *path,const char *const *names,size_t count) {
    for(size_t i=0;i<count;i++)if(!AckmanModFilenameValid(names[i]))return false;
    char *data=SDL_LoadFile(path,NULL);if(!data)return false;
    ActiveComparison comparison={names,count,0,true};
    const char *keyboard=strstr(data,"[Keyboard.Keybinds.Player1]"),*active=strstr(data,"[ActiveMods]");
    const char *next=keyboard?strstr(keyboard+1,"\n["):NULL;
    if(active && (!keyboard || (next && next+1==active)) &&
       AckmanIniRead(path,compare_active,&comparison) && comparison.equal && comparison.index==count) {
        SDL_free(data);return true;
    }
    char temporary[4112];SDL_snprintf(temporary,sizeof(temporary),"%s.tmp",path);
    SDL_IOStream *f=SDL_IOFromFile(temporary,"wb");if(!f){SDL_free(data);return false;}
    char section[128]="";bool inserted=false,ok=true;
    for(char *line=data;*line && ok;) {
        char *end=strchr(line,'\n');if(end)*end=0;
        char *copy=SDL_strdup(line);if(!copy){ok=false;break;}
        char next[128];SDL_strlcpy(next,section,sizeof(next));char *value;
        parse(copy,next,sizeof(next),&value);
        if(SDL_strcasecmp(next,section)) {
            if(!inserted && !SDL_strcasecmp(section,"Keyboard.Keybinds.Player1")) {
                ok=emit_active_mods(f,names,count);inserted=true;
            }
            SDL_strlcpy(section,next,sizeof(section));
        }
        if(SDL_strcasecmp(section,"ActiveMods"))ok=ok && emit(f,line) && emit(f,"\n");
        SDL_free(copy);if(!end)break;line=end+1;
    }
    if(ok && !inserted)ok=emit_active_mods(f,names,count);
    if(!SDL_CloseIO(f))ok=false;
    if(ok)ok=SDL_RenamePath(temporary,path);
    if(!ok)SDL_RemovePath(temporary);
    SDL_free(data);return ok;
}
bool AckmanConfigWriteActiveMods(const char *const *names,size_t count) {
    return write_mods_file(config_path,names,count);
}
bool AckmanConfigCommitRuntime(const AckmanIniValue *values,size_t count,
                              const char *const *mods,size_t mod_count) {
    char stage[4112];SDL_snprintf(stage,sizeof(stage),"%s.pending",config_path);
    if(!SDL_CopyFile(config_path,stage))return false;
    bool ok=update_file(stage,values,count,NULL,0) && write_mods_file(stage,mods,mod_count);
    if(ok)ok=SDL_RenamePath(stage,config_path);
    if(!ok)SDL_RemovePath(stage);
    return ok;
}
bool AckmanConfigUpdate(const AckmanIniValue *values,size_t count) {
    return AckmanConfigUpdateRemoving(values,count,NULL,0);
}

/* Upgrade the legacy slot arrays without touching explicit canonical entries. */
typedef struct { AckmanIniValue values[128];char names[128][64],text[128][128];size_t count;bool slots[2][10],relocations[128],legacy_schema,menu_defined;char legacy[2][1024]; } HotkeyMigration;
static void migration_add(HotkeyMigration *m,const char *key,const char *value) {
 if(m->count>=128)return;size_t i=m->count++;
 SDL_strlcpy(m->names[i],key,sizeof(m->names[i]));if(value)SDL_strlcpy(m->text[i],value,sizeof(m->text[i]));
 m->values[i]=(AckmanIniValue){"KeyMap",m->names[i],value?m->text[i]:NULL};
}
static void migrate_hotkey(void *context,const char *section,const char *key,const char *value) {
 HotkeyMigration *m=context;if(SDL_strcasecmp(section,"KeyMap"))return;
 if(!SDL_strcasecmp(key,"Menu"))m->menu_defined=true;
 if(!SDL_strcasecmp(key,"SaveStateMenu")){m->legacy_schema=true;migration_add(m,key,NULL);return;}
 for(int save=0;save<2;save++) {
  const char *name=save?"Save":"Load";
  if(!SDL_strcasecmp(key,name)){m->legacy_schema=true;SDL_strlcpy(m->legacy[save],value,sizeof(m->legacy[save]));migration_add(m,key,NULL);return;}
  for(int i=0;i<10;i++){char slot[32];SDL_snprintf(slot,sizeof(slot),"%sSlot%d",name,i+1);
   if(!SDL_strcasecmp(key,slot)){m->slots[save][i]=true;return;}}
 }
 // Relocate only exact conflicts with the default plain/Shift F1..F10 slots.
 const char *base=value;int mods=0;
 while(1) {
  if(!SDL_strncasecmp(base,"Shift+",6)){mods|=1;base+=6;}
  else if(!SDL_strncasecmp(base,"Ctrl+",5)){mods|=2;base+=5;}
  else if(!SDL_strncasecmp(base,"Alt+",4)){mods|=4;base+=4;}
  else break;
 }
 SDL_Keycode code=SDL_GetKeyFromName(base);
 if((mods==0 || mods==1) && code>=SDLK_F1 && code<=SDLK_F10) {
  char moved[32];SDL_snprintf(moved,sizeof(moved),"%s%d",mods?"Shift+":"",(int)(code-SDLK_F1+1)%10);
  size_t before=m->count;migration_add(m,key,moved);if(m->count>before)m->relocations[before]=true;
 }
}
bool AckmanConfigMigrateHotkeys(void) {
 HotkeyMigration m={0};if(!AckmanIniRead(AckmanConfigPath(),migrate_hotkey,&m))return false;
 if(!m.menu_defined)migration_add(&m,"Menu","None");
 for(int save=0;save<2;save++)for(int i=0;i<10;i++)if(!m.slots[save][i])m.legacy_schema=true;
 // Relocations apply only to the schema upgrade, never undo a deliberate UI
 // remap on later boots of an already canonical file.
 if(!m.legacy_schema) {
  size_t kept=0;for(size_t i=0;i<m.count;i++)if(!m.relocations[i])m.values[kept++]=m.values[i];m.count=kept;
 }
 for(int save=0;save<2;save++) {
  char *legacy=m.legacy[save];
  for(int i=0;i<10;i++) {
   char *next=strchr(legacy,',');if(next)*next=0;
   if(!m.slots[save][i]) {
    char key[32],fallback[32];SDL_snprintf(key,sizeof(key),"%sSlot%d",save?"Save":"Load",i+1);
    SDL_snprintf(fallback,sizeof(fallback),"%sF%d",save?"Shift+":"",i+1);
    migration_add(&m,key,*legacy?legacy:fallback);
   }
   legacy=next?next+1:legacy+strlen(legacy);
  }
 }
 return !m.count || AckmanConfigUpdate(m.values,m.count);
}
