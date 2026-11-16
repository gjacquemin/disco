#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
typedef struct { uint32_t rate, bytes; long offset; } wav_info;
static uint16_t le16(const unsigned char *p) { return p[0] | (p[1]<<8); }
static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
/* PCM mono 16 bits, 44.1/48 kHz. Supporte les chunks JUNK/LIST et padding RIFF. */
static bool wav_read(FILE *f, wav_info *w) {
    unsigned char h[16]; bool fmt=false, data=false;
    if(fseek(f,0,SEEK_END)) return false;
    long size=ftell(f); rewind(f);
    if(size<12 || fread(h,1,12,f)!=12 || memcmp(h,"RIFF",4) || memcmp(h+8,"WAVE",4)) return false;
    uint64_t end=8ULL+le32(h+4);
    if(end>(uint64_t)size || end<12) return false;
    while((uint64_t)ftell(f)+8<=end) {
        if(fread(h,1,8,f)!=8) return false;
        uint32_t n=le32(h+4); long pos=ftell(f);
        uint64_t next=(uint64_t)pos+n+(n&1);
        if(next>end) return false;
        if(!memcmp(h,"fmt ",4)) {
            if(n<16 || fread(h,1,16,f)!=16) return false;
            w->rate=le32(h+4);
            if(le16(h)!=1 || le16(h+2)!=1 || le16(h+12)!=2 || le16(h+14)!=16 ||
               (w->rate!=44100 && w->rate!=48000) || le32(h+8)!=w->rate*2) return false;
            fmt=true;
        } else if(!memcmp(h,"data",4) && !data) {
            w->offset=pos; w->bytes=n; data=true;
        }
        if(fseek(f,(long)next,SEEK_SET)) return false;
    }
    return fmt && data && w->bytes>0 && !(w->bytes&1) && fseek(f,w->offset,SEEK_SET)==0;
}
