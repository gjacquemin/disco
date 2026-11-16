#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "diagnostics.h"
#include "nvs.h"
#include "cJSON.h"
#include "config.h"
#include "wav.h"
#include "file_name.h"
#include "web.h"
#include "selection.h"
#include "player.h"
static httpd_handle_t server;
extern const char page_html[];
static esp_err_t reply(httpd_req_t *r,const char *status,const char *text) {
    httpd_resp_set_status(r,status);
    httpd_resp_set_type(r,"text/plain; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,text,HTTPD_RESP_USE_STRLEN);
}
static bool name_header(httpd_req_t *r,const char *header,char path[512]) {
    char encoded[768],name[256];
    if(httpd_req_get_hdr_value_str(r,header,encoded,sizeof(encoded))!=ESP_OK || !decode_name(encoded,name)) return false;
    return snprintf(path,512,"%s/%s",AUDIO_DIRECTORY,name)<512;
}
static bool authorized_request(httpd_req_t *r) {
    char value[8];
    // Entete personnalise : refuse les formulaires cross-origin. Aucun CORS autorise.
    return httpd_req_get_hdr_value_str(r,"X-Manage",value,sizeof(value))==ESP_OK && !strcmp(value,"1");
}
static esp_err_t reject(httpd_req_t *r,const char *status,const char *text) {
    httpd_resp_set_hdr(r,"Connection","close");
    reply(r,status,text);
    return ESP_FAIL; // ferme la connexion, notamment si le corps n'a pas ete consomme
}
static esp_err_t index_get(httpd_req_t *r) {
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,page_html,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t files_get(httpd_req_t *r) {
    DIR *d=opendir(AUDIO_DIRECTORY);
    if(!d) return reply(r,"503 Service Unavailable","Carte SD ou dossier sons indisponible.");
    cJSON *root=cJSON_CreateObject(),*files=cJSON_CreateArray();
    if(!root||!files) { closedir(d); cJSON_Delete(root); cJSON_Delete(files); return reply(r,"500 Internal Server Error","Memoire insuffisante."); }
    cJSON_AddItemToObject(root,"files",files);
    cJSON_AddNumberToObject(root,"maxBytes",WEB_MAX_FILE_BYTES);
    struct dirent *e; unsigned count=0;
    while((e=readdir(d))) {
        size_t n=strlen(e->d_name);
        if(e->d_name[0]=='.'||n<5||strcasecmp(e->d_name+n-4,".wav")) continue;
        char path[512]; struct stat st;
        if(snprintf(path,sizeof(path),"%s/%s",AUDIO_DIRECTORY,e->d_name)>=(int)sizeof(path)) continue;
        if(stat(path,&st)||!S_ISREG(st.st_mode)) continue;
        if(count++>=1024) { cJSON_AddBoolToObject(root,"truncated",true); break; }
        cJSON *item=cJSON_CreateObject();
        if(!item) break;
        cJSON_AddStringToObject(item,"name",e->d_name);
        cJSON_AddNumberToObject(item,"size",st.st_size);
        cJSON_AddItemToArray(files,item);
    }
    closedir(d);
    char *json=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if(!json) return reply(r,"500 Internal Server Error","Memoire insuffisante.");
    httpd_resp_set_type(r,"application/json"); httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_send(r,json,HTTPD_RESP_USE_STRLEN); free(json); return result;
}
static esp_err_t urn_get(httpd_req_t *r) {
    cJSON *root=selection_status();
    char *json=root?cJSON_PrintUnformatted(root):NULL; cJSON_Delete(root);
    if(!json) return reply(r,"500 Internal Server Error","Memoire insuffisante.");
    httpd_resp_set_type(r,"application/json"); httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_send(r,json,HTTPD_RESP_USE_STRLEN); free(json); return err;
}
static esp_err_t urn_reset_post(httpd_req_t *r) {
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    if(player_busy()) return reject(r,"409 Conflict","Arretez la lecture avant de modifier les fichiers ou l'urne.");
    if(selection_reset()!=ESP_OK) return reply(r,"500 Internal Server Error","Impossible de reconstruire ou sauvegarder l'urne.");
    return reply(r,"200 OK","Urne reinitialisee et melangee. Aucun son lance.");
}
static esp_err_t changed(httpd_req_t *r,const char *status,const char *message) {
    if(selection_reset()!=ESP_OK) return reply(r,"500 Internal Server Error",
        "Fichier modifie, mais echec de reinitialisation de l'urne. Utilisez Reinitialiser l'urne et verifiez la SD/NVS.");
    return reply(r,status,message);
}
static esp_err_t upload_post(httpd_req_t *r) {
    char target[512]; struct stat st;
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    if(player_busy()) return reject(r,"409 Conflict","Arretez la lecture avant de modifier les fichiers ou l'urne.");
    if(!name_header(r,"X-File-Name",target)) return reject(r,"400 Bad Request","Nom WAV invalide.");
    if(r->content_len==0||r->content_len>WEB_MAX_FILE_BYTES) return reject(r,"413 Payload Too Large","Taille du fichier hors limite.");
    if(!stat(target,&st)) return reject(r,"409 Conflict","Ce nom existe deja. Renommez ou supprimez le fichier existant.");
    const char *temp=AUDIO_DIRECTORY "/.upload.part";
    FILE *f=fopen(temp,"wb");
    if(!f) return reject(r,"507 Insufficient Storage","Impossible d'ecrire sur la SD.");
    char buf[4096]; size_t remaining=r->content_len; unsigned timeouts=0;
    while(remaining) {
        int n=httpd_req_recv(r,buf,remaining<sizeof(buf)?remaining:sizeof(buf));
        if(n==HTTPD_SOCK_ERR_TIMEOUT && ++timeouts<=3) continue;
        if(n<=0) { fclose(f); unlink(temp); return reject(r,"408 Request Timeout","Transfert interrompu."); }
        timeouts=0;
        if(fwrite(buf,1,n,f)!=(size_t)n) { fclose(f); unlink(temp); return reject(r,"507 Insufficient Storage","SD pleine ou erreur d'ecriture."); }
        remaining-=n;
    }
    bool ok=fflush(f)==0;
    if(ok) ok=fsync(fileno(f))==0;
    if(fclose(f)) ok=false;
    if(!ok) { unlink(temp); return reply(r,"507 Insufficient Storage","Echec de sauvegarde."); }
    f=fopen(temp,"rb"); wav_info w;
    ok=f && wav_read(f,&w); if(f) fclose(f);
    if(!ok) { unlink(temp); return reply(r,"415 Unsupported Media Type","WAV requis : PCM mono ou stereo 16 bits, 44100 ou 48000 Hz."); }
    // Serveur mono-tache, operations seriees ; aucun lecteur actif a ce stade.
    if(rename(temp,target)) { unlink(temp); return reply(r,"500 Internal Server Error","Impossible de finaliser le fichier."); }
    return changed(r,"201 Created","Fichier ajoute.");
}
static esp_err_t delete_post(httpd_req_t *r) {
    char path[512];
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    if(player_busy()) return reject(r,"409 Conflict","Arretez la lecture avant de modifier les fichiers ou l'urne.");
    if(!name_header(r,"X-File-Name",path)) return reject(r,"400 Bad Request","Nom invalide.");
    if(unlink(path)) return reply(r,"404 Not Found","Fichier absent ou suppression impossible.");
    return changed(r,"200 OK","Fichier supprime.");
}
static esp_err_t rename_post(httpd_req_t *r) {
    char old[512],dest[512],last[512]; struct stat st;
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    if(player_busy()) return reject(r,"409 Conflict","Arretez la lecture avant de modifier les fichiers ou l'urne.");
    if(!name_header(r,"X-File-Name",old)||!name_header(r,"X-New-Name",dest)) return reject(r,"400 Bad Request","Nom WAV invalide.");
    if(!strcmp(old,dest)) return reply(r,"200 OK","Nom inchange.");
    if(!stat(dest,&st)) return reply(r,"409 Conflict","Ce nom existe deja (y compris avec une autre casse).");
    if(stat(old,&st)||!S_ISREG(st.st_mode)) return reply(r,"404 Not Found","Fichier absent.");
    if(rename(old,dest)) return reply(r,"500 Internal Server Error","Renommage impossible.");
    if(selection_reset()!=ESP_OK) return reply(r,"500 Internal Server Error","Fichier renomme, mais echec de reinitialisation de l'urne.");
    // Mettre a jour le dernier morceau pour conserver l'exclusion apres renommage.
    nvs_handle_t nvs; esp_err_t err=nvs_open("random_player",NVS_READWRITE,&nvs);
    if(err==ESP_OK) {
        size_t n=sizeof(last);
        if(nvs_get_str(nvs,"last",last,&n)==ESP_OK && !strcasecmp(last,old)) {
            err=nvs_set_str(nvs,"last",dest);
            if(err==ESP_OK) err=nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    if(err!=ESP_OK) return reply(r,"200 OK","Fichier renomme, mais memoire du dernier morceau non mise a jour.");
    return reply(r,"200 OK","Fichier renomme.");
}
static esp_err_t player_get(httpd_req_t *r) {
    player_status st; player_get_status(&st);
    cJSON *root=cJSON_CreateObject();
    if(!root) return reply(r,"500 Internal Server Error","Memoire insuffisante.");
    cJSON_AddBoolToObject(root,"playing",st.playing);
    cJSON_AddNumberToObject(root,"volume",st.volume);
    cJSON_AddNumberToObject(root,"elapsedMs",st.elapsed_ms);
    cJSON_AddNumberToObject(root,"durationMs",st.duration_ms);
    cJSON_AddBoolToObject(root,"fast",WIFI_FAST);
    const char *name=strrchr(st.path,'/');
    cJSON_AddStringToObject(root,"name",name?name+1:st.path);
    cJSON_AddStringToObject(root,"error",st.error);
    char *json=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if(!json) return reply(r,"500 Internal Server Error","Memoire insuffisante.");
    httpd_resp_set_type(r,"application/json"); httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_send(r,json,HTTPD_RESP_USE_STRLEN); free(json); return err;
}
static esp_err_t play_post(httpd_req_t *r) {
    char path[512];
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    if(!name_header(r,"X-File-Name",path)) return reject(r,"400 Bad Request","Nom WAV invalide.");
    FILE *f=fopen(path,"rb"); wav_info w;
    bool valid=f && wav_read(f,&w); if(f) fclose(f);
    if(!valid) return reply(r,"415 Unsupported Media Type","WAV absent ou incompatible.");
    if(player_request(path)!=ESP_OK) return reply(r,"503 Service Unavailable","Lecteur indisponible.");
    return reply(r,"202 Accepted","Ecoute demandee sur la sortie jack ; urne inchangee.");
}
static esp_err_t stop_post(httpd_req_t *r) {
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    player_stop(); return reply(r,"200 OK","Arret demande.");
}
static esp_err_t volume_post(httpd_req_t *r) {
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    char value[8];
    if(httpd_req_get_hdr_value_str(r,"X-Volume",value,sizeof(value))!=ESP_OK || !value[0])
        return reject(r,"400 Bad Request","Volume entier attendu entre 0 et 100.");
    for(unsigned i=0;value[i];i++) if(value[i]<'0'||value[i]>'9')
        return reject(r,"400 Bad Request","Volume invalide.");
    long v=strtol(value,NULL,10);
    if(v>100) return reject(r,"400 Bad Request","Volume hors limite.");
    if(player_set_volume((int)v)!=ESP_OK) return reply(r,"500 Internal Server Error","Volume non memorise : erreur NVS.");
    return reply(r,"200 OK","Volume memorise et demande au lecteur.");
}
static esp_err_t mode_post(httpd_req_t *r) {
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    char value[12];
    if(httpd_req_get_hdr_value_str(r,"X-Mode",value,sizeof(value))!=ESP_OK)
        return reject(r,"400 Bad Request","Mode attendu : random ou urn.");
    int mode=!strcmp(value,"urn")?PLAYBACK_URN:!strcmp(value,"random")?PLAYBACK_RANDOM:-1;
    if(mode<0) return reject(r,"400 Bad Request","Mode invalide.");
    if(selection_set_mode(mode)!=ESP_OK) return reply(r,"500 Internal Server Error","Mode non memorise.");
    return reply(r,"200 OK","Mode memorise pour le prochain demarrage. Lecture actuelle inchangee.");
}
void web_start(void) {
    if(server) return;
    // May run during playback in FAST mode; never mutate SD here.
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG(); cfg.stack_size=8192; cfg.max_uri_handlers=12;
    cfg.recv_wait_timeout=10; cfg.send_wait_timeout=10; cfg.lru_purge_enable=true;
    if(httpd_start(&server,&cfg)!=ESP_OK) { server=NULL; DISCO_LOGE("WEB","Echec demarrage"); return; }
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=index_get},
        {.uri="/api/mode",.method=HTTP_POST,.handler=mode_post},
        {.uri="/api/urn",.method=HTTP_GET,.handler=urn_get},
        {.uri="/api/urn/reset",.method=HTTP_POST,.handler=urn_reset_post},
        {.uri="/api/player",.method=HTTP_GET,.handler=player_get},
        {.uri="/api/play",.method=HTTP_POST,.handler=play_post},
        {.uri="/api/stop",.method=HTTP_POST,.handler=stop_post},
        {.uri="/api/volume",.method=HTTP_POST,.handler=volume_post},
        {.uri="/api/files",.method=HTTP_GET,.handler=files_get},
        {.uri="/api/upload",.method=HTTP_POST,.handler=upload_post},
        {.uri="/api/delete",.method=HTTP_POST,.handler=delete_post},
        {.uri="/api/rename",.method=HTTP_POST,.handler=rename_post}
    };
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);i++) {
        if(httpd_register_uri_handler(server,&routes[i])!=ESP_OK) { httpd_stop(server); server=NULL; DISCO_LOGE("WEB","Echec routes"); return; }
    }
    DISCO_LOGI("WEB","Serveur disponible sur http://ADRESSE_IP/ (port 80)");
}
