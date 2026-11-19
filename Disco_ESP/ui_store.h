#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
// A basic HTML sanity check, not a JavaScript correctness/security validator.
static bool ui_html_valid(const char *path,size_t limit) {
    struct stat st;
    if(stat(path,&st) || !S_ISREG(st.st_mode) || st.st_size<=0 || (size_t)st.st_size>limit) return false;
    FILE *f=fopen(path,"rb"); if(!f) return false;
    char head[4097]; size_t n=fread(head,1,sizeof(head)-1,f); head[n]=0;
    bool valid=!ferror(f) && !memchr(head,0,n);
    for(size_t i=0;i<n;i++) head[i]=(char)tolower((unsigned char)head[i]);
    valid=valid && (strstr(head,"<html") || strstr(head,"<!doctype html"));
    char buf[1024];
    while(valid && (n=fread(buf,1,sizeof(buf),f))) if(memchr(buf,0,n)) valid=false;
    if(ferror(f)) valid=false;
    fclose(f); return valid;
}
// FAT rename does not replace an existing target. Keep it until new bytes are validated.
static bool ui_replace(const char *part,const char *target,const char *backup) {
    struct stat st; bool old=stat(target,&st)==0;
    if(old) {
        if(!S_ISREG(st.st_mode)) return false;
        if(unlink(backup) && access(backup,F_OK)==0) return false;
        if(rename(target,backup)) return false;
    }
    if(rename(part,target)) { if(old) rename(backup,target); return false; }
    unlink(backup); return true;
}

static bool ui_install(const char *part,const char *target,const char *backup,size_t limit) {
 return ui_html_valid(part,limit) && ui_replace(part,target,backup);
}
