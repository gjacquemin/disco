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
#include "ui_store.h"
#include "web_assets.h"
#define UI_DIR AUDIO_DIRECTORY "/www"
#define UI_PATH UI_DIR "/index.html"
#define UI_PART UI_DIR "/.upload.part"
#define UI_BACKUP UI_DIR "/.upload.bak"
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
static esp_err_t recovery_get(httpd_req_t *r) {
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,page_html,HTTPD_RESP_USE_STRLEN);
}
static const char *ui_mime(const char *name) {
    const char *ext=strrchr(name,'.'); if(!ext) return NULL;
    const char *extensions[]={".html",".js",".css",".png",".jpg",".jpeg",".gif",".webp",".svg",".ico",".json",".woff2"};
    const char *types[]={"text/html; charset=utf-8","text/javascript; charset=utf-8","text/css; charset=utf-8","image/png","image/jpeg","image/jpeg","image/gif","image/webp","image/svg+xml","image/x-icon","application/json","font/woff2"};
    for(unsigned i=0;i<sizeof(extensions)/sizeof(extensions[0]);i++) if(!strcasecmp(ext,extensions[i])) return types[i];
    return NULL;
}
// Flat directory only. Decode once; reject traversal, hidden/temp files and separators.
static bool ui_name(const char *encoded,char name[256]) {
    size_t n=0;
    while(*encoded) {
        unsigned char c=*encoded++;
        if(c=='%') { if(!encoded[0]||!encoded[1]) return false; int a=hexval(encoded[0]),b=hexval(encoded[1]); if(a<0||b<0) return false; c=(a<<4)|b; encoded+=2; }
        if(n>=255||c<32||c==127||strchr("/\\:*?\"<>|%",c)) return false;
        name[n++]=c;
    }
    name[n]=0;
    return n>0 && name[0]!='.' && name[n-1]!=' ' && ui_mime(name);
}
static bool ui_header(httpd_req_t *r,char name[256]) {
    char encoded[768]; return httpd_req_get_hdr_value_str(r,"X-File-Name",encoded,sizeof(encoded))==ESP_OK && ui_name(encoded,name);
}
static esp_err_t ui_serve(httpd_req_t *r,const char *name,bool fallback) {
    char path[512]; snprintf(path,sizeof(path),"%s/%s",UI_DIR,name);
    struct stat st;
    if(stat(path,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||(size_t)st.st_size>WEB_MAX_HTML_BYTES)
        return fallback?recovery_get(r):httpd_resp_send_404(r);
    FILE *f=fopen(path,"rb"); if(!f) return fallback?recovery_get(r):httpd_resp_send_404(r);
    httpd_resp_set_type(r,ui_mime(name)); httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    char buf[2048]; size_t n; esp_err_t err=ESP_OK;
    while((n=fread(buf,1,sizeof(buf),f))) { err=httpd_resp_send_chunk(r,buf,n); if(err!=ESP_OK) break; }
    if(ferror(f)) err=ESP_FAIL;
    fclose(f); return err==ESP_OK?httpd_resp_send_chunk(r,NULL,0):err;
}
static esp_err_t index_get(httpd_req_t *r) {
    struct stat st;
    if(stat(UI_PATH,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||(size_t)st.st_size>WEB_MAX_HTML_BYTES) return recovery_get(r);
    httpd_resp_set_status(r,"302 Found"); httpd_resp_set_hdr(r,"Location","/www/index.html");
    httpd_resp_set_hdr(r,"Cache-Control","no-store"); return httpd_resp_send(r,"",0);
}
static esp_err_t ui_asset_get(httpd_req_t *r) {
    char encoded[768],name[256]; const char *start=r->uri+5; size_t n=strcspn(start,"?#");
    if(n>=sizeof(encoded)) return httpd_resp_send_404(r);
    memcpy(encoded,start,n); encoded[n]=0;
    if(!n) strcpy(encoded,"index.html");
    if(!ui_name(encoded,name)) return httpd_resp_send_404(r);
    return ui_serve(r,name,!strcasecmp(name,"index.html"));
}
static esp_err_t ui_files_get(httpd_req_t *r) {
    cJSON *root=cJSON_CreateObject(),*files=cJSON_CreateArray();
    if(!root||!files) {cJSON_Delete(root);cJSON_Delete(files);return reply(r,"500 Internal Server Error","Memoire insuffisante.");}
    cJSON_AddItemToObject(root,"files",files); cJSON_AddNumberToObject(root,"maxBytes",WEB_MAX_HTML_BYTES);
    DIR *dir=opendir(UI_DIR); struct dirent *e; unsigned count=0;
    if(dir) {while((e=readdir(dir)) && count<256) {
        if(e->d_name[0]=='.'||!ui_mime(e->d_name)) continue;
        char path[512];snprintf(path,sizeof(path),"%s/%s",UI_DIR,e->d_name);struct stat st;
        if(stat(path,&st)||!S_ISREG(st.st_mode))continue;
        cJSON *item=cJSON_CreateObject();if(!item)break;
        cJSON_AddStringToObject(item,"name",e->d_name);cJSON_AddNumberToObject(item,"size",st.st_size);cJSON_AddItemToArray(files,item);count++;
    }closedir(dir);}
    cJSON_AddBoolToObject(root,"truncated",count>=256);
    char *body=cJSON_PrintUnformatted(root);cJSON_Delete(root);if(!body)return reply(r,"500 Internal Server Error","Memoire insuffisante.");
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");esp_err_t err=httpd_resp_send(r,body,HTTPD_RESP_USE_STRLEN);free(body);return err;
}
static esp_err_t ui_delete_post(httpd_req_t *r) {
    if(!authorized_request(r))return reject(r,"403 Forbidden","Requete refusee.");
    if(player_busy())return reject(r,"409 Conflict","Appuyez sur STOP.");
    char name[256],path[512];if(!ui_header(r,name))return reject(r,"400 Bad Request","Nom invalide.");
    snprintf(path,sizeof(path),"%s/%s",UI_DIR,name);
    if(unlink(path))return reply(r,"500 Internal Server Error","Suppression impossible.");
    return reply(r,"200 OK","Fichier Web supprime.");
}
static esp_err_t ping_get(httpd_req_t *r) {
    char body[128];
    snprintf(body,sizeof(body),"{\"disco\":true,\"maxHtmlBytes\":%lu}",(unsigned long)WEB_MAX_HTML_BYTES);
    httpd_resp_set_type(r,"application/json"); httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,body,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t ui_upload_post(httpd_req_t *r) {
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    if(player_busy()) return reject(r,"409 Conflict","Appuyez sur STOP avant de mettre a jour la page.");
    if(!r->content_len || r->content_len>WEB_MAX_HTML_BYTES)
        return reject(r,"413 Payload Too Large","Fichier Web vide ou trop volumineux.");
    char name[256],target[512];
    if(!ui_header(r,name)) return reject(r,"400 Bad Request","Nom de fichier Web invalide.");
    snprintf(target,sizeof(target),"%s/%s",UI_DIR,name);
    if(mkdir(UI_DIR,0775) && errno!=EEXIST) return reject(r,"507 Insufficient Storage","Creation de /www impossible.");
    FILE *f=fopen(UI_PART,"wb");
    if(!f) return reject(r,"507 Insufficient Storage","Impossible d'ecrire la page sur la SD.");
    char buf[2048]; size_t remaining=r->content_len; unsigned timeouts=0;
    while(remaining) {
        int n=httpd_req_recv(r,buf,remaining<sizeof(buf)?remaining:sizeof(buf));
        if(n==HTTPD_SOCK_ERR_TIMEOUT && ++timeouts<=3) continue;
        if(n<=0) { fclose(f); unlink(UI_PART); return reject(r,"408 Request Timeout","Transfert HTML interrompu."); }
        timeouts=0;
        if(fwrite(buf,1,n,f)!=(size_t)n) { fclose(f); unlink(UI_PART); return reject(r,"507 Insufficient Storage","Erreur d'ecriture HTML."); }
        remaining-=n;
    }
    bool ok=fflush(f)==0;
    if(ok) ok=fsync(fileno(f))==0;
    if(fclose(f)) ok=false;
    if(!ok) { unlink(UI_PART); return reply(r,"507 Insufficient Storage","Echec de sauvegarde HTML."); }
    if(!strcasecmp(strrchr(name,'.'),".html") && !ui_html_valid(UI_PART,WEB_MAX_HTML_BYTES)) { unlink(UI_PART); return reply(r,"415 Unsupported Media Type","Document HTML requis (balise html ou doctype au debut), sans octets nuls."); }
    if(!ui_replace(UI_PART,target,UI_BACKUP)) {
        unlink(UI_PART); return reply(r,"500 Internal Server Error","Echec du remplacement. Page embarquee disponible sur /recovery.");
    }
    return reply(r,"200 OK","Fichier Web mis a jour. Rechargez la page principale.");
}
static esp_err_t ui_reset_post(httpd_req_t *r) {
    if(!authorized_request(r)) return reject(r,"403 Forbidden","Requete refusee.");
    if(player_busy()) return reject(r,"409 Conflict","Appuyez sur STOP avant de restaurer la page.");
    if(unlink(UI_PATH) && errno!=ENOENT) return reply(r,"500 Internal Server Error","Suppression de index.html impossible.");
    unlink(UI_PART); unlink(UI_BACKUP);
    return reply(r,"200 OK","Page embarquee restauree.");
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
static esp_err_t app_asset_get(httpd_req_t *r) {
    for(unsigned i=0;i<sizeof(web_assets)/sizeof(web_assets[0]);i++) {
        if(!strcmp(r->uri,web_assets[i].path)) {
            httpd_resp_set_type(r,web_assets[i].type);
            httpd_resp_set_hdr(r,"Cache-Control","no-cache");
            return httpd_resp_send(r,(const char *)web_assets[i].data,web_assets[i].size);
        }
    }
    return httpd_resp_send_404(r);
}
void web_start(void) {
    if(server) return;
    // May run during playback in FAST mode; never mutate SD here.
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG(); cfg.stack_size=12288; cfg.max_uri_handlers=24; cfg.uri_match_fn=httpd_uri_match_wildcard;
    cfg.recv_wait_timeout=10; cfg.send_wait_timeout=10; cfg.lru_purge_enable=true;
    if(httpd_start(&server,&cfg)!=ESP_OK) { server=NULL; DISCO_LOGE("WEB","Echec demarrage"); return; }
    const httpd_uri_t routes[]={
        {.uri="/manifest.webmanifest",.method=HTTP_GET,.handler=app_asset_get},
        {.uri="/apple-touch-icon.png",.method=HTTP_GET,.handler=app_asset_get},
        {.uri="/icon-192.png",.method=HTTP_GET,.handler=app_asset_get},
        {.uri="/icon-512.png",.method=HTTP_GET,.handler=app_asset_get},
        {.uri="/",.method=HTTP_GET,.handler=index_get},
        {.uri="/index.html",.method=HTTP_GET,.handler=index_get},
        {.uri="/recovery",.method=HTTP_GET,.handler=recovery_get},
        {.uri="/api/ping",.method=HTTP_GET,.handler=ping_get},
        {.uri="/api/ui/upload",.method=HTTP_POST,.handler=ui_upload_post},
        {.uri="/api/ui/reset",.method=HTTP_POST,.handler=ui_reset_post},
        {.uri="/api/ui/files",.method=HTTP_GET,.handler=ui_files_get},
        {.uri="/api/ui/delete",.method=HTTP_POST,.handler=ui_delete_post},
        {.uri="/www/*",.method=HTTP_GET,.handler=ui_asset_get},
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
