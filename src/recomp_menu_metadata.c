#include "recomp_menu_metadata.h"
#include "crc32.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <string.h>

static uint16_t u16(const unsigned char *p) { return p[0]|((uint16_t)p[1]<<8); }
static uint32_t u32(const unsigned char *p) { return u16(p)|((uint32_t)u16(p+2)<<16); }
void RecompModMetadataClear(RecompModMetadata *m) {
    SDL_free(m->name);SDL_free(m->author);SDL_free(m->version);SDL_free(m->description);
    *m=(RecompModMetadata){0};
}
static bool space(char c) { return c==' ' || c=='\t' || c=='\r'; }
static char **metadata_field(RecompModMetadata *out,const char *key,size_t length) {
    if((length==4 && !SDL_strncasecmp(key,"name",4)) || (length==5 && !SDL_strncasecmp(key,"title",5)))return &out->name;
    if((length==6 && !SDL_strncasecmp(key,"author",6)) || (length==7 && !SDL_strncasecmp(key,"authors",7)))return &out->author;
    if(length==7 && !SDL_strncasecmp(key,"version",7))return &out->version;
    if((length==11 && !SDL_strncasecmp(key,"description",11)) || (length==4 && !SDL_strncasecmp(key,"note",4)))return &out->description;
    return NULL;
}
static char *copy_text(const char *start,const char *end) {
    while(start<end && (*start=='\n' || space(*start)))start++;
    while(end>start && (end[-1]=='\n' || space(end[-1])))end--;
    char *text=SDL_malloc((size_t)(end-start)+1);if(!text)return NULL;
    memcpy(text,start,(size_t)(end-start));text[end-start]=0;return text;
}
/* Display-only XML: no DTD/entity expansion, external resources or execution. */
static void parse_xml(char *text,RecompModMetadata *out) {
    const char *cursor=text;
    while((cursor=strchr(cursor,'<'))) {
        const char *key=++cursor;while(SDL_isalnum(*cursor) || *cursor=='_')cursor++;
        size_t key_size=(size_t)(cursor-key);char **field=metadata_field(out,key,key_size);
        const char *open=strchr(cursor,'>');if(!open)break;
        cursor=open+1;if(!field || *field || key_size>32)continue;
        char closing[40];SDL_snprintf(closing,sizeof(closing),"</%.*s>",(int)key_size,key);
        const char *end=SDL_strstr(cursor,closing);if(!end)continue;
        const char *value=cursor,*stop=end;
        bool literal=stop-value>=12 && !memcmp(value,"<![CDATA[",9) && !memcmp(stop-3,"]]>",3);
        if(literal){value+=9;stop-=3;}
        char *decoded=copy_text(value,stop);if(!decoded)continue;
        if(literal){*field=decoded;cursor=end+strlen(closing);continue;}
        char *read=decoded,*write=decoded;
        while(*read) {
            const char *entities[]={"&amp;","&lt;","&gt;","&quot;","&apos;"};const char values[]={'&','<','>','"','\''};
            bool matched=false;
            for(int i=0;i<5;i++)if(!strncmp(read,entities[i],strlen(entities[i]))) {
                *write++=values[i];read+=strlen(entities[i]);matched=true;break;
            }
            if(!matched && read[0]=='&' && read[1]=='#') {
                char *finish=NULL;bool hex=read[2]=='x' || read[2]=='X';
                unsigned long code=SDL_strtoul(read+(hex?3:2),&finish,hex?16:10);
                if(finish>read+(hex?3:2) && *finish==';' && code && code<=0x10ffff && !(code>=0xd800 && code<=0xdfff)) {
                    if(code<0x80)*write++=(char)code;
                    else if(code<0x800){*write++=(char)(0xc0|(code>>6));*write++=(char)(0x80|(code&63));}
                    else if(code<0x10000){*write++=(char)(0xe0|(code>>12));*write++=(char)(0x80|((code>>6)&63));*write++=(char)(0x80|(code&63));}
                    else {*write++=(char)(0xf0|(code>>18));*write++=(char)(0x80|((code>>12)&63));*write++=(char)(0x80|((code>>6)&63));*write++=(char)(0x80|(code&63));}
                    read=finish+1;matched=true;
                }
            }
            if(!matched)*write++=*read++;
        }
        *write=0;*field=decoded;cursor=end+strlen(closing);
    }
}
static void parse_bml(char *text,RecompModMetadata *out) {
    const char *cursor=text;int group=-1,description_indent=-1;
    while(*cursor) {
        const char *line=cursor;while(*cursor && *cursor!='\n')cursor++;
        const char *end=cursor;if(*cursor)cursor++;
        int indent=0;while(line<end && space(*line)){line++;indent++;}
        if(line==end) {
            if(description_indent>=0) {
                size_t n=strlen(out->description);char *next=SDL_realloc(out->description,n+2);
                if(next){out->description=next;if(n){next[n]='\n';next[n+1]=0;}}
            }
            continue;
        }
        if(description_indent>=0 && indent>description_indent) {
            char *piece=copy_text(line,end);
            if(piece) {
                size_t a=strlen(out->description),b=strlen(piece);char *next=SDL_realloc(out->description,a+b+2);
                if(next){out->description=next;if(a)next[a]='\n';memcpy(next+a+(a?1:0),piece,b+1);}SDL_free(piece);
            }
            continue;
        }
        description_indent=-1;
        if(group>=0 && indent<=group)group=-1;
        if((end-line==11 && !memcmp(line,"information",11)) || (end-line==8 && !memcmp(line,"metadata",8))) {group=indent;continue;}
        if(indent && group<0)continue; /* Never use rom/track name attributes. */
        const char *colon=memchr(line,':',(size_t)(end-line));if(!colon)continue;
        const char *key_end=colon;while(key_end>line && space(key_end[-1]))key_end--;
        char **field=metadata_field(out,line,(size_t)(key_end-line));if(!field || *field)continue;
        *field=copy_text(colon+1,end);
        if(*field && field==&out->description)description_indent=indent;
    }
}
static bool read_sidecar(const char *path,RecompModMetadata *out) {
    SDL_PathInfo info;if(!SDL_GetPathInfo(path,&info) || info.type!=SDL_PATHTYPE_FILE || !info.size || info.size>1024*1024)return false;
    size_t size=0;char *text=SDL_LoadFile(path,&size);if(!text)return false;
    if(memchr(text,0,size)){SDL_free(text);return false;}
    size_t length=strlen(path);
    if(length>4 && !SDL_strcasecmp(path+length-4,".xml"))parse_xml(text,out);else parse_bml(text,out);
    SDL_free(text);return out->name || out->author || out->version || out->description;
}
static bool read_msu(const char *path,RecompModMetadata *out) {
    char folder[4096],candidate[4096];size_t length=strlen(path);if(length>=sizeof(folder))return false;
    SDL_strlcpy(folder,path,sizeof(folder));char *separator=strrchr(folder,'/'),*back=strrchr(folder,'\\');
    if(back && (!separator || back>separator))separator=back;
    if(separator)separator[1]=0;else SDL_strlcpy(folder,"./",sizeof(folder));
    const char *extensions[]={"xml","bml"};
    for(int i=0;i<2;i++) {
        SDL_snprintf(candidate,sizeof(candidate),"%.*s.%s",(int)(length-4),path,extensions[i]);
        if(read_sidecar(candidate,out))return true;
        SDL_snprintf(candidate,sizeof(candidate),"%smanifest.%s",folder,extensions[i]);
        if(read_sidecar(candidate,out))return true;
    }
    int count=0;char **files=SDL_GlobDirectory(folder,"*",0,&count);
    if(files)for(int i=0;i<count;i++) {
        size_t n=strlen(files[i]);if(n<5 || (SDL_strcasecmp(files[i]+n-4,".xml") && SDL_strcasecmp(files[i]+n-4,".bml")))continue;
        SDL_snprintf(candidate,sizeof(candidate),"%s%s",folder,files[i]);
        if(read_sidecar(candidate,out)){SDL_free(files);return true;}
    }
    SDL_free(files);return false;
}
static bool parse_manifest(const unsigned char *bytes,size_t size,RecompModMetadata *out) {
    const char *cursor=(const char *)bytes,*end=cursor+size;
    while(cursor<end) {
        const char *line=cursor;while(cursor<end && *cursor!='\n')cursor++;
        const char *limit=cursor;if(cursor<end)cursor++;
        while(line<limit && space(*line))line++;
        if(line==limit || *line=='#')continue;
        if(*line=='[')break; /* Only package metadata, never feature/target names. */
        const char *equals=memchr(line,'=',(size_t)(limit-line));if(!equals)continue;
        const char *key_end=equals;while(key_end>line && space(key_end[-1]))key_end--;
        char **field=NULL;size_t key_size=(size_t)(key_end-line);
        if(key_size==4 && !memcmp(line,"name",4))field=&out->name;
        else if(key_size==6 && !memcmp(line,"author",6))field=&out->author;
        else if(key_size==7 && !memcmp(line,"version",7))field=&out->version;
        else if(key_size==11 && !memcmp(line,"description",11))field=&out->description;
        if(!field)continue;
        if(*field)return false;
        const char *value=equals+1;while(value<limit && space(*value))value++;
        if(value==limit || *value++!='"')return false;
        char *text=SDL_malloc((size_t)(limit-value)+1),*write=text;
        if(!text)return false;
        bool closed=false;
        while(value<limit) {
            char c=*value++;
            if(c=='"'){closed=true;break;}
            if(c=='\\') {
                if(value==limit)break;
                c=*value++;
                if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';
                else if(c!='"' && c!='\\'){SDL_free(text);return false;}
            }
            if(!c){SDL_free(text);return false;}
            *write++=c;
        }
        *write=0;
        while(value<limit && space(*value))value++;
        if(!closed || (value<limit && *value!='#')){SDL_free(text);return false;}
        *field=text;
    }
    return out->name || out->author || out->version || out->description;
}
bool RecompModMetadataRead(const char *path,RecompModMetadata *out) {
    RecompModMetadataClear(out);
    size_t path_size=path?strlen(path):0;
    if(path_size>4 && !SDL_strcasecmp(path+path_size-4,".msu"))return read_msu(path,out);
    SDL_PathInfo info;
    if(!SDL_GetPathInfo(path,&info) || info.type!=SDL_PATHTYPE_FILE || info.size<22 || info.size>16*1024*1024)return false;
    size_t size=0;unsigned char *data=SDL_LoadFile(path,&size);bool ok=false;
    if(!data || size<22)goto done;
    size_t eocd=size-22,minimum=size>65557?size-65557:0;
    for(;;) {
        if(u32(data+eocd)==0x06054b50 && u16(data+eocd+20)==size-eocd-22)break;
        if(eocd==minimum)goto done;eocd--;
    }
    const unsigned char *tail=data+eocd;
    size_t count=u16(tail+10),central=u32(tail+16),length=u32(tail+12);
    if(u16(tail+4) || u16(tail+6) || u16(tail+8)!=count || central>eocd || length!=eocd-central)goto done;
    size_t cursor=central;const unsigned char *manifest=NULL;size_t manifest_size=0;
    for(size_t i=0;i<count;i++) {
        if(cursor>eocd || eocd-cursor<46 || u32(data+cursor)!=0x02014b50)goto done;
        const unsigned char *entry=data+cursor;
        size_t name_size=u16(entry+28),extra=u16(entry+30),comment=u16(entry+32);
        if(name_size+extra+comment>eocd-cursor-46)goto done;
        if(name_size==13 && !memcmp(entry+46,"manifest.toml",13)) {
            size_t offset=u32(entry+42),stored=u32(entry+20),plain=u32(entry+24);
            if(manifest || u16(entry+8)!=0 || u16(entry+10)!=0 || u16(entry+34)!=0 ||
               !plain || plain>65536 || stored!=plain || offset>central || central-offset<30)goto done;
            const unsigned char *local=data+offset;
            if(u32(local)!=0x04034b50 || u16(local+6)!=0 || u16(local+8)!=0 ||
               u16(local+26)!=13 || u32(local+14)!=u32(entry+16) ||
               u32(local+18)!=stored || u32(local+22)!=plain)goto done;
            size_t header=30+13+u16(local+28);
            if(header>central-offset || plain>central-offset-header || memcmp(local+30,"manifest.toml",13))goto done;
            manifest=local+header;manifest_size=plain;
            if(crc32_compute(manifest,plain)!=u32(entry+16))goto done;
        }
        cursor+=46+name_size+extra+comment;
    }
    if(cursor==eocd && manifest)ok=parse_manifest(manifest,manifest_size,out);
done:
    SDL_free(data);if(!ok)RecompModMetadataClear(out);return ok;
}
