#pragma once
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
typedef struct {char ssid[33];char password[65];} disco_wifi_credentials;
static bool wifi_credentials_valid(const disco_wifi_credentials *c) {
    size_t s=strnlen(c->ssid,sizeof(c->ssid)),p=strnlen(c->password,sizeof(c->password));
    if(!s||s>32||p>64)return false;
    if(p==64){for(size_t i=0;i<p;i++)if(!isxdigit((unsigned char)c->password[i]))return false;}
    else if(p && p<8)return false;
    return true;
}
// Strict two-key UTF-8 text, LF/CRLF; values are kept verbatim (including spaces/=/#).
static bool wifi_credentials_parse(FILE *f,disco_wifi_credentials *out) {
    unsigned char data[513];size_t n=fread(data,1,sizeof(data),f);
    if(ferror(f)||n>512||memchr(data,0,n))return false;
    data[n]=0;char *line=(char *)data;
    if(n>=3 && data[0]==0xef && data[1]==0xbb && data[2]==0xbf)line+=3;
    disco_wifi_credentials c={0};bool ss=false,pw=false;
    while(*line){char *next=strchr(line,'\n');if(next)*next++=0;
        size_t len=strlen(line);if(len && line[len-1]=='\r')line[--len]=0;
        for(size_t i=0;i<len;i++)if((unsigned char)line[i]<32)return false;
        if(len && line[0]!='#'){
            if(!strncmp(line,"ssid=",5)){if(ss||len-5>32)return false;strcpy(c.ssid,line+5);ss=true;}
            else if(!strncmp(line,"password=",9)){if(pw||len-9>64)return false;strcpy(c.password,line+9);pw=true;}
            else return false;
        }
        if(!next)break;line=next;
    }
    if(!ss||!pw||!wifi_credentials_valid(&c))return false;
    *out=c;return true;
}
