#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_system.h"
#include "bootloader_random.h"
#include "nvs.h"
#include "mbedtls/sha256.h"
#include "config.h"
#include "wav.h"
#include "urn_core.h"
#include "selection.h"
#include "diagnostics.h"
#include "esp_timer.h"
#define URN_MAGIC 0x55524e31U
// One NVS blob contains both permutation and cursor: no split transaction.
typedef struct {
    uint32_t magic;
    uint16_t count, cursor;
    unsigned char catalog[32];
    uint16_t order[URN_MAX];
} urn_state;
static urn_state state;
static char **paths;
static unsigned count;
static uint32_t durations[URN_MAX];
uint32_t selection_duration(const char *path) {
    for(unsigned i=0;i<count;i++)if(!strcmp(paths[i],path))return durations[i];
    return 0;
}
static bool ready;
static char playlist[128];
static char directory[512]=AUDIO_DIRECTORY;
static char urn_namespace[16]="random_player";
const char *selection_directory(void) { return directory; }
const char *selection_playlist(void) { return playlist; }
static void playlist_paths(void) {
    snprintf(directory,sizeof(directory),"%s%s%s",AUDIO_DIRECTORY,playlist[0]?"/":"",playlist);
    if(!playlist[0]) { strcpy(urn_namespace,"random_player"); return; }
    unsigned char digest[32]; mbedtls_sha256_context sha; mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha,0);
    mbedtls_sha256_update_ret(&sha,(unsigned char*)playlist,strlen(playlist));
    mbedtls_sha256_finish_ret(&sha,digest); mbedtls_sha256_free(&sha);
    urn_namespace[0]='p';
    for(unsigned i=0;i<7;i++) snprintf(urn_namespace+1+2*i,3,"%02x",digest[i]);
}
static int mode=PLAYBACK_MODE;
void selection_init(void) {
    nvs_handle_t nvs; uint8_t saved;
    if(nvs_open("audio_settings",NVS_READONLY,&nvs)==ESP_OK) {
        if(nvs_get_u8(nvs,"mode",&saved)==ESP_OK && saved<=1) mode=saved;
        size_t length=sizeof(playlist);
        if(nvs_get_str(nvs,"playlist",playlist,&length)!=ESP_OK) playlist[0]=0;
        nvs_close(nvs);
    }
    playlist_paths();
}
esp_err_t selection_set_mode(int value) {
    if(value!=PLAYBACK_URN && value!=PLAYBACK_RANDOM) return ESP_ERR_INVALID_ARG;
    if(value==mode) return ESP_OK;
    nvs_handle_t nvs; esp_err_t err=nvs_open("audio_settings",NVS_READWRITE,&nvs);
    if(err!=ESP_OK) return err;
    err=nvs_set_u8(nvs,"mode",value);
    if(err==ESP_OK) err=nvs_commit(nvs);
    nvs_close(nvs); if(err==ESP_OK) mode=value;
    return err;
}
static int compare_paths(const void *a,const void *b) {
    return strcmp(*(char *const*)a,*(char *const*)b);
}
static void release_catalog(void) {
    for(unsigned i=0;i<count;i++) free(paths[i]);
    free(paths); paths=NULL; count=0; ready=false;
}
// A disposable SD index. The urn itself remains committed in NVS.
static void catalog_digest(unsigned char digest[32]) {
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts_ret(&sha,0);
    for(unsigned i=0;i<count;i++)mbedtls_sha256_update_ret(&sha,(unsigned char*)paths[i],strlen(paths[i])+1);
    mbedtls_sha256_finish_ret(&sha,digest);mbedtls_sha256_free(&sha);
}
static void cache_path(char out[512],bool temp){snprintf(out,512,"%s/%s",directory,temp?".disco-catalog.tmp":".disco-catalog.bin");}
static bool cache_read(unsigned char digest[32],bool verify) {
    char index[512];cache_path(index,false);FILE *f=fopen(index,"rb");if(!f)return false;
    uint32_t magic,n;bool ok=fread(&magic,4,1,f)==1&&magic==0x44434332&&fread(&n,4,1,f)==1&&n<=URN_MAX;
    release_catalog();
    for(unsigned i=0;ok&&i<n;i++){
        uint16_t len;uint64_t size;int64_t modified;uint32_t duration;char path[512];struct stat st;
        ok=fread(&len,2,1,f)==1&&len>0&&len<sizeof(path);
        if(!ok)break;
        ok=fread(path,1,len,f)==len&&fread(&size,8,1,f)==1&&fread(&modified,8,1,f)==1&&fread(&duration,4,1,f)==1&&duration>0;
        path[len]=0;
        size_t prefix=strlen(directory);
        ok=ok&&strlen(path)==len&&!strncmp(path,directory,prefix)&&path[prefix]=='/'&&!strchr(path+prefix+1,'/')&&!strchr(path+prefix+1,'\\')&&path[prefix+1]!='.';
        if(verify)ok=ok&&!stat(path,&st)&&S_ISREG(st.st_mode)&&(uint64_t)st.st_size==size&&(int64_t)st.st_mtime==modified;
        if(!ok)break;
        char **next=realloc(paths,(count+1)*sizeof(*paths));if(!next){ok=false;break;}paths=next;
        paths[count]=strdup(path);if(!paths[count]){ok=false;break;}durations[count]=duration;count++;
        if(count>1&&strcmp(paths[count-2],paths[count-1])>=0){ok=false;break;}
    }
    unsigned char stored[32];if(ok){catalog_digest(digest);ok=fread(stored,1,32,f)==32&&!memcmp(stored,digest,32)&&fgetc(f)==EOF;}
    fclose(f);
    // Detect files added offline without opening their WAV headers.
    if(ok&&verify){DIR *d=opendir(directory);if(!d)ok=false;else {struct dirent *e;unsigned files=0;
        while((e=readdir(d))){size_t l=strlen(e->d_name);if(e->d_name[0]=='.'||l<5||strcasecmp(e->d_name+l-4,".wav"))continue;
            char path[512];struct stat st;snprintf(path,sizeof(path),"%s/%s",directory,e->d_name);
            if(!stat(path,&st)&&S_ISREG(st.st_mode))files++;}
        closedir(d);ok=files==count;}}
    if(!ok)release_catalog();return ok;
}
static bool cache_load(unsigned char digest[32]) {return cache_read(digest,true);}
static void cache_write(const unsigned char digest[32]) {
    char temp[512],dest[512];cache_path(temp,true);cache_path(dest,false);
    FILE *f=fopen(temp,"wb");if(!f)return;
    uint32_t magic=0x44434332,n=count;bool ok=fwrite(&magic,4,1,f)==1&&fwrite(&n,4,1,f)==1;
    for(unsigned i=0;ok&&i<count;i++){struct stat st;if(stat(paths[i],&st)){ok=false;break;}
        uint16_t len=strlen(paths[i]);uint64_t size=st.st_size;int64_t modified=st.st_mtime;
        ok=fwrite(&len,2,1,f)==1&&fwrite(paths[i],1,len,f)==len&&fwrite(&size,8,1,f)==1&&fwrite(&modified,8,1,f)==1&&fwrite(&durations[i],4,1,f)==1;}
    if(ok)ok=fwrite(digest,1,32,f)==32;
    if(fflush(f))ok=false;if(fclose(f))ok=false;
    if(ok){unlink(dest);rename(temp,dest);}else unlink(temp);
}
static esp_err_t scan(unsigned char digest[32]) {
    release_catalog();
    DIR *dir=opendir(selection_directory());
    if(!dir) return ESP_FAIL;
    struct dirent *entry; esp_err_t err=ESP_OK;
    while((entry=readdir(dir))) {
        const char *name=entry->d_name; size_t len=strlen(name);
        if(name[0]=='.'||len<5||strcasecmp(name+len-4,".wav")) continue;
        char path[512]; struct stat st;
        if(snprintf(path,sizeof(path),"%s/%s",selection_directory(),name)>=(int)sizeof(path)) continue;
        if(stat(path,&st)||!S_ISREG(st.st_mode)) continue;
        FILE *f=fopen(path,"rb"); wav_info w;
        if(!f) { err=ESP_FAIL; break; }
        bool valid=wav_read(f,&w); fclose(f);
        if(!valid) continue;
        if(count==URN_MAX) { err=ESP_ERR_INVALID_SIZE; break; }
        char **next=realloc(paths,(count+1)*sizeof(*paths));
        if(!next) { err=ESP_ERR_NO_MEM; break; }
        paths=next; paths[count]=strdup(path);
        if(!paths[count]) { err=ESP_ERR_NO_MEM; break; }
        durations[count]=(uint32_t)(((uint64_t)w.bytes*1000)/(w.rate*w.channels*2));
        if(!durations[count])durations[count]=1;
        count++;
    }
    closedir(dir);
    if(err!=ESP_OK) { release_catalog(); return err; }
    // Keep durations associated with their filenames when sorting.
    for(unsigned i=1;i<count;i++){char *path=paths[i];uint32_t duration=durations[i];unsigned j=i;
        while(j&&strcmp(paths[j-1],path)>0){paths[j]=paths[j-1];durations[j]=durations[j-1];j--;}
        paths[j]=path;durations[j]=duration;}
    catalog_digest(digest);
    cache_write(digest);
    return ESP_OK;
}
static esp_err_t save(nvs_handle_t nvs) {
    esp_err_t err=nvs_set_blob(nvs,"urn_v1",&state,sizeof(state));
    if(err==ESP_OK) err=nvs_commit(nvs);
    ready=err==ESP_OK;
    return err;
}
static void refill(const unsigned char digest[32]) {
    memset(&state,0,sizeof(state)); state.magic=URN_MAGIC;
    state.count=count; memcpy(state.catalog,digest,32);
    urn_shuffle(state.order,count,esp_random);
}
static bool maintenance_pending;
esp_err_t selection_take(char chosen[512]) {
    unsigned char digest[32];int64_t start=esp_timer_get_time();
    bool cached=cache_read(digest,false);
    if(cached&&!count)cached=false; // An empty old index must not hide newly added tracks.
    esp_err_t err=cached?ESP_OK:scan(digest);
    DISCO_LOGI("BOOT","Catalogue (%s) : %lld us",cached?"cache":"reconstruction",(long long)(esp_timer_get_time()-start));
    if(err!=ESP_OK)return err;
    for(unsigned attempt=0;attempt<2;attempt++) {
        start=esp_timer_get_time();nvs_handle_t nvs;err=nvs_open(urn_namespace,NVS_READWRITE,&nvs);if(err!=ESP_OK)return err;
        bootloader_random_enable();size_t size=sizeof(state);
        err=nvs_get_blob(nvs,"urn_v1",&state,&size);
        if(err==ESP_ERR_NVS_NOT_FOUND||(err==ESP_OK&&(size!=sizeof(state)||state.magic!=URN_MAGIC||state.count!=count||memcmp(state.catalog,digest,32)||!urn_valid(state.order,state.count,state.cursor)||state.cursor==state.count))){refill(digest);err=ESP_OK;}
        if(err==ESP_OK&&count){
            if(mode==PLAYBACK_URN)strcpy(chosen,paths[state.order[state.cursor++]]);
            else {char last[512]={0};size_t len=sizeof(last);int skip=-1;nvs_get_str(nvs,"last",last,&len);
                if(AVOID_LAST_TRACK&&count>1)for(unsigned i=0;i<count;i++)if(!strcmp(last,paths[i]))skip=i;
                strcpy(chosen,paths[choose_index(count,skip,esp_random)]);}
        }
        bootloader_random_disable();
        DISCO_LOGI("BOOT","Chargement urne et tirage : %lld us",(long long)(esp_timer_get_time()-start));
        if(err!=ESP_OK||!count){nvs_close(nvs);return err!=ESP_OK?err:ESP_ERR_NOT_FOUND;}
        start=esp_timer_get_time();FILE *f=fopen(chosen,"rb");wav_info w;bool valid=f&&wav_read(f,&w);if(f)fclose(f);
        DISCO_LOGI("BOOT","Verification du WAV choisi : %lld us",(long long)(esp_timer_get_time()-start));
        if(!valid){nvs_close(nvs);if(attempt)return ESP_FAIL;
            start=esp_timer_get_time();err=scan(digest);
            DISCO_LOGI("BOOT","Reconstruction de secours : %lld us",(long long)(esp_timer_get_time()-start));
            if(err!=ESP_OK)return err;continue;}
        start=esp_timer_get_time();err=save(nvs);
        if(err==ESP_OK&&mode==PLAYBACK_RANDOM){err=nvs_set_str(nvs,"last",chosen);if(err==ESP_OK)err=nvs_commit(nvs);}
        nvs_close(nvs);ready=err==ESP_OK;maintenance_pending=err==ESP_OK;
        DISCO_LOGI("BOOT","Sauvegarde NVS avant audio : %lld us",(long long)(esp_timer_get_time()-start));
        return err;
    }
    return ESP_FAIL;
}
// Called only by the serialized HTTP worker while playback is stopped.
void selection_maintenance(void) {
    if(!maintenance_pending)return;
    maintenance_pending=false;char current[128];strcpy(current,playlist);
    int64_t start=esp_timer_get_time();esp_err_t err=selection_playlist_set(current);
    DISCO_LOGI("BOOT","Verification complete apres lecture : %lld us ; resultat=%d",(long long)(esp_timer_get_time()-start),err);
}
esp_err_t selection_reset_first(const char *first) {
    unsigned char digest[32]; esp_err_t err=scan(digest);
    if(err!=ESP_OK) return err;
    unsigned selected=0;
    if(first) {
        while(selected<count && strcmp(paths[selected],first)) selected++;
        if(selected==count) return ESP_ERR_NOT_FOUND;
    }
    // HTTP server runs after Wi-Fi connects: hardware RNG has RF entropy.
    refill(digest);
    if(first) for(unsigned i=0;i<count;i++) if(state.order[i]==selected) {
        uint16_t swap=state.order[0]; state.order[0]=state.order[i]; state.order[i]=swap; break;
    }
    nvs_handle_t nvs; err=nvs_open(urn_namespace,NVS_READWRITE,&nvs);
    if(err!=ESP_OK) return err;
    err=save(nvs); nvs_close(nvs); return err;

}
esp_err_t selection_reset(void) { return selection_reset_first(NULL); }
cJSON *selection_status(void) {
    cJSON *root=cJSON_CreateObject(); if(!root) return NULL;
    if(mode==PLAYBACK_URN) {
    cJSON_AddStringToObject(root,"mode","urn");
    cJSON_AddBoolToObject(root,"ready",ready);
    cJSON_AddNumberToObject(root,"total",count);
    cJSON *queue=cJSON_AddArrayToObject(root,"queue");
    if(!queue) { cJSON_Delete(root); return NULL; }
    if(ready) for(unsigned i=state.cursor;i<state.count;i++) {
        const char *path=paths[state.order[i]];
        cJSON *item=cJSON_CreateString(strrchr(path,'/')+1);
        if(!item || !cJSON_AddItemToArray(queue,item)) {
            cJSON_Delete(item); cJSON_Delete(root); return NULL;
        }
    }
} else { cJSON_AddStringToObject(root,"mode","random"); }
    return root;
}

esp_err_t selection_playlist_set(const char *name) {
    size_t len=strlen(name);
    if(len>=sizeof(playlist) || (len && (name[0]=='.' || !strcasecmp(name,"www")))) return ESP_ERR_INVALID_ARG;
    for(size_t i=0;i<len;i++) if((unsigned char)name[i]<32 || strchr("/\\:*?\"<>|",name[i])) return ESP_ERR_INVALID_ARG;
    char target[512]; struct stat st;
    snprintf(target,sizeof(target),"%s%s%s",AUDIO_DIRECTORY,len?"/":"",name);
    if(stat(target,&st) || !S_ISDIR(st.st_mode)) return ESP_ERR_NOT_FOUND;
    nvs_handle_t settings; esp_err_t err=nvs_open("audio_settings",NVS_READWRITE,&settings);
    if(err!=ESP_OK) return err;
    err=nvs_set_str(settings,"playlist",name);
    if(err==ESP_OK) err=nvs_commit(settings);
    nvs_close(settings); if(err!=ESP_OK) return err;
    strcpy(playlist,name); playlist_paths();
    unsigned char digest[32]; err=scan(digest); if(err!=ESP_OK) return err;
    nvs_handle_t nvs; err=nvs_open(urn_namespace,NVS_READWRITE,&nvs); if(err!=ESP_OK) return err;
    size_t size=sizeof(state); err=nvs_get_blob(nvs,"urn_v1",&state,&size);
    if(err==ESP_ERR_NVS_NOT_FOUND || (err==ESP_OK && (size!=sizeof(state) || state.magic!=URN_MAGIC || state.count!=count || memcmp(state.catalog,digest,32) || !urn_valid(state.order,state.count,state.cursor)))) {
        refill(digest); err=save(nvs);
    }
    ready=err==ESP_OK; nvs_close(nvs); return err;
}

static bool folder_name(const char *name) {
    size_t n=strlen(name);
    if(!n || n>=128 || name[0]=='.' || name[n-1]==' ' || name[n-1]=='.' || !strcasecmp(name,"www")) return false;
    for(size_t i=0;i<n;i++) if((unsigned char)name[i]<32 || (unsigned char)name[i]==127 || strchr("/\\:*?\"<>|",name[i])) return false;
    return true;
}
esp_err_t selection_manage(const char *op,const char *name,const char *file) {
    char target[512],source[512],original[128]; struct stat st;
    strcpy(original,playlist);
    if(strcmp(op,"delete") && (!folder_name(name) && !( !strcmp(op,"move") && !name[0]))) return ESP_ERR_INVALID_ARG;
    snprintf(target,sizeof(target),"%s%s%s",AUDIO_DIRECTORY,name[0]?"/":"",name);
    if(!strcmp(op,"create")) {
        if(!stat(target,&st) || mkdir(target,0775)) return ESP_FAIL;
        // Clear a possible urn from a previously deleted folder of the same name.
        strcpy(playlist,name);playlist_paths();esp_err_t err=selection_reset();
        strcpy(playlist,original);playlist_paths();
        esp_err_t restore=selection_playlist_set(original);return err!=ESP_OK?err:restore;
    }
    if(!strcmp(op,"rename")) {
        if(!playlist[0] || !stat(target,&st)) return ESP_ERR_INVALID_ARG;
        // Load the current persisted queue before relocating it.
        esp_err_t err=selection_playlist_set(original);if(err!=ESP_OK)return err;
        urn_state previous=state;strcpy(source,directory);
        if(rename(source,target))return ESP_FAIL;
        strcpy(playlist,name);playlist_paths();unsigned char digest[32];err=scan(digest);
        if(err==ESP_OK && count==previous.count) {
            state=previous;memcpy(state.catalog,digest,32);
            nvs_handle_t nvs;err=nvs_open(urn_namespace,NVS_READWRITE,&nvs);
            if(err==ESP_OK){err=save(nvs);nvs_close(nvs);}
        } else err=ESP_FAIL;
        if(err==ESP_OK)err=selection_playlist_set(name);
        if(err!=ESP_OK){rename(target,source);selection_playlist_set(original);}
        return err;
    }
    if(!strcmp(op,"delete")) {
        if(!playlist[0])return ESP_ERR_INVALID_ARG;
        strcpy(source,directory);DIR *d=opendir(source);if(!d)return ESP_FAIL;
        struct dirent *e;bool safe=true;
        while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
            if(!strcmp(e->d_name,".disco-catalog.bin")||!strcmp(e->d_name,".disco-catalog.tmp"))continue;
            size_t n=strlen(e->d_name);snprintf(target,sizeof(target),"%s/%s",source,e->d_name);
            if(stat(target,&st)||!S_ISREG(st.st_mode)||n<5||strcasecmp(e->d_name+n-4,".wav")){safe=false;break;}}
        closedir(d);if(!safe)return ESP_ERR_INVALID_ARG;
        // Persist root before deleting, so a power cut cannot select a removed folder.
        esp_err_t err=selection_playlist_set("");if(err!=ESP_OK)return err;
        d=opendir(source);if(!d)return ESP_FAIL;
        while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
            snprintf(target,sizeof(target),"%s/%s",source,e->d_name);if(unlink(target)){closedir(d);return ESP_FAIL;}}
        closedir(d);return rmdir(source)?ESP_FAIL:ESP_OK;
    }
    if(!strcmp(op,"move")) {
        if(!strcmp(name,original)||stat(target,&st)||!S_ISDIR(st.st_mode))return ESP_ERR_INVALID_ARG;
        size_t n=strlen(file);if(n<5||strcasecmp(file+n-4,".wav")||file[0]=='.'||strpbrk(file,"/\\")||n>255)return ESP_ERR_INVALID_ARG;
        snprintf(source,sizeof(source),"%s/%s",directory,file);
        snprintf(target,sizeof(target),"%s%s%s/%s",AUDIO_DIRECTORY,name[0]?"/":"",name,file);
        if(stat(source,&st)||!S_ISREG(st.st_mode)||!stat(target,&st))return ESP_ERR_INVALID_ARG;
        if(rename(source,target))return ESP_FAIL;
        esp_err_t err=selection_reset();
        strcpy(playlist,name);playlist_paths();esp_err_t other=selection_reset();
        strcpy(playlist,original);playlist_paths();esp_err_t restore=selection_playlist_set(original);
        return err!=ESP_OK?err:other!=ESP_OK?other:restore;
    }
    return ESP_ERR_INVALID_ARG;
}
