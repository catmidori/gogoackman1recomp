#include "ackman_ips.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
bool AckmanIpsApply(const unsigned char *ips,size_t size,unsigned char *rom,size_t rom_size) {
    if(!ips || !rom || size<8 || memcmp(ips,"PATCH",5))return false;
    size_t p=5;
    while(p<=size && size-p>=3) {
        if(!memcmp(ips+p,"EOF",3))return p+3==size;
        size_t address=((size_t)ips[p]<<16)|((size_t)ips[p+1]<<8)|ips[p+2];p+=3;
        if(size-p<2)return false;
        size_t length=((size_t)ips[p]<<8)|ips[p+1];p+=2;
        bool rle=!length;
        if(rle) {
            if(size-p<3)return false;
            length=((size_t)ips[p]<<8)|ips[p+1];p+=2;
        }
        if(!length || address>rom_size || length>rom_size-address || size-p<(rle?1:length))return false;
        if(rle){memset(rom+address,ips[p],length);p++;}
        else {memcpy(rom+address,ips+p,length);p+=length;}
    }
    return false;
}
static int nibble(char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}
bool AckmanTranslationApplyEnglish(unsigned char *rom,size_t size,const char *table) {
    const char *p=table;
    while((p=strstr(p,"[[rom_patch]]"))) {
        const char *address=strstr(p,"address = "),*source=strstr(p,"source_hex = \""),*target=strstr(p,"en_hex = \"");
        if(!address || !source || !target)return false;
        size_t offset=strtoul(address+10,NULL,0);source+=14;target+=10;
        const char *se=strchr(source,'"'),*te=strchr(target,'"');
        if(!se || !te || se-source!=te-target || (se-source)%2)return false;
        size_t count=(size_t)(se-source)/2;
        if(offset>size || count>size-offset)return false;
        for(size_t i=0;i<count;i++) {
            int a=nibble(source[i*2]),b=nibble(source[i*2+1]),c=nibble(target[i*2]),d=nibble(target[i*2+1]);
            if(a<0 || b<0 || c<0 || d<0 || rom[offset+i]!=(unsigned char)((a<<4)|b))return false;
            rom[offset+i]=(unsigned char)((c<<4)|d);
        }
        p=te+1;
    }
    return true;
}
char *AckmanTranslationBuildTable(const unsigned char *source,const unsigned char *target,size_t size) {
    /* Worst case: one short header per byte plus two hex payloads. Budget the
     * actual runs first, avoiding quadratic reallocation on hostile patches. */
    size_t capacity=64;
    for(size_t i=0;i<size;) {
        if(source[i]==target[i]){i++;continue;}
        size_t end=i+1;while(end<size && source[end]!=target[end])end++;
        capacity+=128+4*(end-i);i=end;
    }
    char *text=SDL_malloc(capacity);if(!text)return NULL;
    size_t pos=(size_t)SDL_snprintf(text,capacity,"schema = 1\ndefault_lang = \"en\"\n\n");
    static const char hex[]="0123456789abcdef";
    for(size_t i=0;i<size;) {
        if(source[i]==target[i]){i++;continue;}
        size_t end=i+1;while(end<size && source[end]!=target[end])end++;
        pos+=(size_t)SDL_snprintf(text+pos,capacity-pos,"[[rom_patch]]\naddress = 0x%zx\nsource_hex = \"",i);
        for(size_t j=i;j<end;j++){text[pos++]=hex[source[j]>>4];text[pos++]=hex[source[j]&15];}
        memcpy(text+pos,"\"\nen_hex = \"",12);pos+=12;
        for(size_t j=i;j<end;j++){text[pos++]=hex[target[j]>>4];text[pos++]=hex[target[j]&15];}
        memcpy(text+pos,"\"\n\n",3);pos+=3;i=end;
    }
    text[pos]=0;return text;
}
