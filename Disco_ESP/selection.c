#include <stdio.h>
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
static bool ready;
static int compare_paths(const void *a,const void *b) {
    return strcmp(*(char *const*)a,*(char *const*)b);
}
static void release_catalog(void) {
    for(unsigned i=0;i<count;i++) free(paths[i]);
    free(paths); paths=NULL; count=0; ready=false;
}
static esp_err_t scan(unsigned char digest[32]) {
    release_catalog();
    DIR *dir=opendir(AUDIO_DIRECTORY);
    if(!dir) return ESP_FAIL;
    struct dirent *entry; esp_err_t err=ESP_OK;
    while((entry=readdir(dir))) {
        const char *name=entry->d_name; size_t len=strlen(name);
        if(name[0]=='.'||len<5||strcasecmp(name+len-4,".wav")) continue;
        char path[512]; struct stat st;
        if(snprintf(path,sizeof(path),"%s/%s",AUDIO_DIRECTORY,name)>=(int)sizeof(path)) continue;
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
        count++;
    }
    closedir(dir);
    if(err!=ESP_OK) { release_catalog(); return err; }
    if(count) qsort(paths,count,sizeof(*paths),compare_paths);
    mbedtls_sha256_context sha; mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha,0);
    for(unsigned i=0;i<count;i++) mbedtls_sha256_update_ret(&sha,(unsigned char*)paths[i],strlen(paths[i])+1);
    mbedtls_sha256_finish_ret(&sha,digest); mbedtls_sha256_free(&sha);
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
esp_err_t selection_take(char chosen[512]) {
    unsigned char digest[32]; esp_err_t err=scan(digest);
    if(err!=ESP_OK) return err;
    nvs_handle_t nvs; err=nvs_open("random_player",NVS_READWRITE,&nvs);
    if(err!=ESP_OK) return err;
    bootloader_random_enable(); // Only called BEFORE audio or Wi-Fi.
#if PLAYBACK_MODE == PLAYBACK_URN
    size_t size=sizeof(state);
    err=nvs_get_blob(nvs,"urn_v1",&state,&size);
    if(err==ESP_ERR_NVS_NOT_FOUND || (err==ESP_OK && (size!=sizeof(state)||
       state.magic!=URN_MAGIC||state.count!=count||memcmp(state.catalog,digest,32)||
       !urn_valid(state.order,state.count,state.cursor)||state.cursor==state.count))) {
        refill(digest); err=ESP_OK;
    }
    if(err==ESP_OK) {
        if(count) strcpy(chosen,paths[state.order[state.cursor++]]);
        err=save(nvs); // Removal durable BEFORE the first audio sample.
    }
#else
    char last[512]={0}; size_t size=sizeof(last); int skip=-1;
    nvs_get_str(nvs,"last",last,&size);
    if(AVOID_LAST_TRACK && count>1) for(unsigned i=0;i<count;i++) if(!strcmp(last,paths[i])) skip=i;
    if(count) {
        strcpy(chosen,paths[choose_index(count,skip,esp_random)]);
        err=nvs_set_str(nvs,"last",chosen);
        if(err==ESP_OK) err=nvs_commit(nvs);
    }
    ready=err==ESP_OK;
#endif
    bootloader_random_disable(); nvs_close(nvs);
    return err!=ESP_OK?err:(count?ESP_OK:ESP_ERR_NOT_FOUND);
}
esp_err_t selection_reset(void) {
#if PLAYBACK_MODE == PLAYBACK_URN
    unsigned char digest[32]; esp_err_t err=scan(digest);
    if(err!=ESP_OK) return err;
    // HTTP server runs after Wi-Fi connects: hardware RNG has RF entropy.
    refill(digest);
    nvs_handle_t nvs; err=nvs_open("random_player",NVS_READWRITE,&nvs);
    if(err!=ESP_OK) return err;
    err=save(nvs); nvs_close(nvs); return err;
#else
    return ESP_OK;
#endif
}
cJSON *selection_status(void) {
    cJSON *root=cJSON_CreateObject(); if(!root) return NULL;
#if PLAYBACK_MODE == PLAYBACK_URN
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
#else
    cJSON_AddStringToObject(root,"mode","random");
#endif
    return root;
}
