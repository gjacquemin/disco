#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
static int hexval(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='A'&&c<='F') return c-'A'+10;
    if(c>='a'&&c<='f') return c-'a'+10;
    return -1;
}
/* Nom encode via encodeURIComponent, jamais un chemin. */
static bool decode_name(const char *in,char out[256]) {
    size_t n=0;
    while(*in) {
        unsigned char c=(unsigned char)*in++;
        if(c=='%') {
            if(!in[0]||!in[1]) return false;
            int a=hexval(in[0]),b=hexval(in[1]);
            if(a<0||b<0) return false;
            c=(unsigned char)((a<<4)|b); in+=2;
        }
        if(n>=255 || c<32 || c==127 || strchr("/\\:*?\"<>|",c)) return false;
        out[n++]=(char)c;
    }
    out[n]=0;
    return n>4 && out[0]!='.' && out[n-1]!=' ' && !strcasecmp(out+n-4,".wav");
}
