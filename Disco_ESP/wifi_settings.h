#pragma once
#include "wifi_credentials.h"
#include "nvs.h"
#include "config.h"
#include "diagnostics.h"
static disco_wifi_credentials wifi_active;
static void wifi_settings_load(void) {
    memset(&wifi_active,0,sizeof(wifi_active));
    if(strlen(WIFI_SSID)<=32 && strlen(WIFI_PASSWORD)<=64){strcpy(wifi_active.ssid,WIFI_SSID);strcpy(wifi_active.password,WIFI_PASSWORD);}
    nvs_handle_t h;disco_wifi_credentials saved={0};size_t len=sizeof(saved);
    if(nvs_open("disco_wifi",NVS_READONLY,&h)==ESP_OK){
        if(nvs_get_blob(h,"credentials",&saved,&len)==ESP_OK && len==sizeof(saved) && wifi_credentials_valid(&saved))wifi_active=saved;
        nvs_close(h);
    }
}
static void wifi_settings_import(void) {
    FILE *f=fopen(WIFI_SD_FILE,"rb");if(!f)return;
    disco_wifi_credentials next={0};bool valid=wifi_credentials_parse(f,&next);fclose(f);
    if(!valid){DISCO_LOGW("WIFI","wifi.txt invalide : identifiants precedents conserves");return;}
    if(!memcmp(&next,&wifi_active,sizeof(next))){
        // Even compiled credentials must be stored on the first explicit import.
        nvs_handle_t h;disco_wifi_credentials saved={0};size_t len=sizeof(saved);bool stored=false;
        if(nvs_open("disco_wifi",NVS_READONLY,&h)==ESP_OK){stored=nvs_get_blob(h,"credentials",&saved,&len)==ESP_OK && len==sizeof(saved) && !memcmp(&saved,&next,sizeof(next));nvs_close(h);}
        if(stored)return;
    }
    nvs_handle_t h;esp_err_t err=nvs_open("disco_wifi",NVS_READWRITE,&h);
    if(err==ESP_OK){err=nvs_set_blob(h,"credentials",&next,sizeof(next));if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);}
    if(err!=ESP_OK){DISCO_LOGW("WIFI","Import non memorise : identifiants precedents conserves");return;}
    wifi_active=next;DISCO_LOGI("WIFI","Identifiants importes depuis la SD et memorises");
}
