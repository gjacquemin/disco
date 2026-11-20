#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/i2c.h"
#include "player.h"
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "diagnostics.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "bootloader_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "codec.h"
#include "selection.h"
#include "config.h"
#include "wav.h"
#include "pcm.h"
#include "diagnostic_tone.h"
#include "web.h"
#include "wifi_settings.h"
static const char *TAG="DISCO";
enum { AUDIO_DMA_COUNT=16, AUDIO_DMA_FRAMES=256 };
static EventGroupHandle_t wifi_events;
static bool wifi_started;
static bool wifi_attempted;
static disco_wifi_credentials wifi_connect_credentials;
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START) esp_wifi_connect();
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) xEventGroupSetBits(wifi_events,2);
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e=data;
        DISCO_LOGI(TAG,"Wi-Fi connecte : " IPSTR,IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(wifi_events,1);
    }
}
static void wifi_task(void *arg) {
    wifi_events=xEventGroupCreate(); configASSERT(wifi_events);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL));
    wifi_config_t cfg={0};
    size_t sl=strlen(wifi_connect_credentials.ssid), pl=strlen(wifi_connect_credentials.password);
    if(!wifi_credentials_valid(&wifi_connect_credentials)) { DISCO_LOGE(TAG,"Identifiants Wi-Fi invalides"); vTaskDelete(NULL); return; }
    memcpy(cfg.sta.ssid,wifi_connect_credentials.ssid,sl); memcpy(cfg.sta.password,wifi_connect_credentials.password,pl);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&cfg));
    DISCO_LOGI(TAG,"Tentative Wi-Fi (FAST=%d)",WIFI_FAST);
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t b=xEventGroupWaitBits(wifi_events,3,pdFALSE,pdFALSE,pdMS_TO_TICKS(WIFI_TIMEOUT_MS));
    if(!(b&1)) { DISCO_LOGW(TAG,"Wi-Fi indisponible : aucune incidence sur la lecture"); esp_wifi_stop(); }
    if(b&1) web_start();
    vTaskDelete(NULL);
}
static void start_wifi(void) {
    if(wifi_started) return;
    if(!strcmp(wifi_active.ssid,"TON_SSID") || !strlen(wifi_active.ssid)) {
        wifi_started=true; DISCO_LOGI(TAG,"Wi-Fi desactive : SSID non configure"); return;
    }
    wifi_started=true;
    wifi_connect_credentials=wifi_active; // Immutable snapshot for the Wi-Fi task.
    wifi_attempted=xTaskCreate(wifi_task,"wifi",4096,NULL,3,NULL)==pdPASS;
    if(!wifi_attempted) DISCO_LOGE(TAG,"Creation tache Wi-Fi impossible");
}
static bool mount_sd(void) {
    sdmmc_host_t host=SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT(); slot.width=1;
    esp_vfs_fat_sdmmc_mount_config_t cfg={.format_if_mount_failed=false,.max_files=3,.allocation_unit_size=16384};
    sdmmc_card_t *card=NULL;
    esp_err_t err=esp_vfs_fat_sdmmc_mount("/sdcard",&host,&slot,&cfg,&card);
    if(err!=ESP_OK) DISCO_LOGE(TAG,"Carte SD : %s",esp_err_to_name(err));
    if(err==ESP_OK) DISCO_LOGI(TAG,"Carte SD montee : bus 1 bit");
    return err==ESP_OK;
}
// The HTTP server serializes all commands. Only this worker owns the codec/I2S.
static portMUX_TYPE player_lock=portMUX_INITIALIZER_UNLOCKED;
static player_status status={.playing=true,.volume=AUDIO_VOLUME};
static bool stop_requested;
static QueueHandle_t play_queue;
typedef struct { char path[512]; } play_command;
bool player_busy(void) {
    portENTER_CRITICAL(&player_lock); bool value=status.playing; portEXIT_CRITICAL(&player_lock); return value;
}
void player_get_status(player_status *out) {
    portENTER_CRITICAL(&player_lock); *out=status; portEXIT_CRITICAL(&player_lock);
}
static int current_volume(void) {
    portENTER_CRITICAL(&player_lock); int v=status.volume; portEXIT_CRITICAL(&player_lock); return v;
}
static bool stopped(void) {
    portENTER_CRITICAL(&player_lock); bool v=stop_requested; portEXIT_CRITICAL(&player_lock); return v;
}
void player_stop(void) {
    portENTER_CRITICAL(&player_lock); if(status.playing) stop_requested=true; portEXIT_CRITICAL(&player_lock);
}
static void finish(const char *error) {
    portENTER_CRITICAL(&player_lock);
    status.playing=false; stop_requested=false;
    snprintf(status.error,sizeof(status.error),"%s",error?error:"");
    portEXIT_CRITICAL(&player_lock);
}
esp_err_t player_request(const char *path) {
    if(!play_queue || strlen(path)>=sizeof(((play_command*)0)->path)) return ESP_ERR_INVALID_STATE;
    // HTTP commands are serialized. Wait for codec/I2S cleanup before replacing.
    player_stop();
    for(unsigned i=0;player_busy() && i<300;i++) vTaskDelay(pdMS_TO_TICKS(10));
    if(player_busy()) return ESP_ERR_TIMEOUT;
    play_command cmd; strcpy(cmd.path,path);
    portENTER_CRITICAL(&player_lock);
    if(status.playing) { portEXIT_CRITICAL(&player_lock); return ESP_ERR_INVALID_STATE; }
    status.playing=true; status.elapsed_ms=0; status.duration_ms=0; status.error[0]=0; stop_requested=false; strcpy(status.path,path);
    portEXIT_CRITICAL(&player_lock);
    if(xQueueSend(play_queue,&cmd,0)!=pdTRUE) { finish("File de lecture indisponible."); return ESP_FAIL; }
    return ESP_OK;
}
esp_err_t player_set_volume(int volume) {
    if(volume<0 || volume>100) return ESP_ERR_INVALID_ARG;
    if(volume==current_volume()) return ESP_OK;
    nvs_handle_t nvs; esp_err_t err=nvs_open("audio_settings",NVS_READWRITE,&nvs);
    if(err!=ESP_OK) return err;
    err=nvs_set_u8(nvs,"volume",volume);
    if(err==ESP_OK) err=nvs_commit(nvs);
    nvs_close(nvs);
    if(err==ESP_OK) {
        portENTER_CRITICAL(&player_lock); status.volume=volume; portEXIT_CRITICAL(&player_lock);
    }
    return err;
}
static void play_file(const char *path,bool boot) {
    bool diagnostic=boot && AUDIO_DIAGNOSTIC;
    FILE *f=diagnostic?NULL:fopen(path,"rb"); wav_info w={0};
    uint32_t max_read_us=0,slow_reads=0;
    const char *error=NULL; bool installed=false,bus=false;
    codec_profile profile;
    if(diagnostic) { w.rate=44100; w.channels=1; w.bytes=7U*44100U*2U; }
    else if(!f || !wav_read(f,&w)) { error="WAV absent ou incompatible."; goto cleanup; }
    portENTER_CRITICAL(&player_lock);
    status.duration_ms=(uint32_t)((uint64_t)w.bytes*1000/(w.rate*w.channels*2));
    status.elapsed_ms=0;
    portEXIT_CRITICAL(&player_lock);
    DISCO_LOGI(TAG,"Lecture %s : %lu Hz, PCM16, %u canal(aux), volume=%d, WIFI_FAST=%d",
               diagnostic?"TEST 1 kHz 5 s + silence 2 s":path,(unsigned long)w.rate,w.channels,
               diagnostic?35:current_volume(),WIFI_FAST);
    if(codec_detect(&profile)!=ESP_OK) { error="Codec non reconnu."; goto cleanup; }
    bus=true;
    DISCO_LOGI(TAG,"I2S Philips 16 bits stereo ; MCLK=0 BCLK=%d WS=%d DOUT=%d ; DMA=%d x %d",
               profile.bclk,profile.lrck,profile.dout,AUDIO_DMA_COUNT,AUDIO_DMA_FRAMES);
    i2s_config_t i2s={.mode=I2S_MODE_MASTER|I2S_MODE_TX,.sample_rate=w.rate,
        .bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT,.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format=I2S_COMM_FORMAT_STAND_I2S,.intr_alloc_flags=0,
        .dma_buf_count=AUDIO_DMA_COUNT,.dma_buf_len=AUDIO_DMA_FRAMES,.use_apll=true,.tx_desc_auto_clear=true,.fixed_mclk=w.rate*256};
    if(i2s_driver_install(I2S_NUM_0,&i2s,0,NULL)!=ESP_OK) { error="Initialisation I2S impossible."; goto cleanup; }
    installed=true;
    i2s_pin_config_t pins={.mck_io_num=0,.bck_io_num=profile.bclk,.ws_io_num=profile.lrck,.data_out_num=profile.dout,.data_in_num=I2S_PIN_NO_CHANGE};
    if(i2s_set_pin(I2S_NUM_0,&pins)!=ESP_OK) { error="Broches I2S invalides."; goto cleanup; }
    int applied_volume=diagnostic?35:current_volume();
    if(codec_start(&profile,w.rate,applied_volume)!=ESP_OK) { error="Initialisation codec impossible."; goto cleanup; }
    static int16_t input[2048],stereo[4096]; uint32_t remaining=w.bytes,frames=0;
    while(remaining && !stopped()) {
        int desired=diagnostic?35:current_volume();
        if(desired!=applied_volume) {
            if(codec_set_volume(&profile,desired)!=ESP_OK) { error="Reglage du volume impossible sur le codec."; break; }
            applied_volume=desired;
        }
        size_t want=remaining<sizeof(input)?remaining:sizeof(input);
        int64_t read_start=esp_timer_get_time();
        size_t got=want;
        if(diagnostic) diagnostic_samples(input,want/sizeof(int16_t),frames);
        else got=fread(input,1,want,f);
        uint32_t read_us=(uint32_t)(esp_timer_get_time()-read_start);
        if(!diagnostic) {
            if(read_us>max_read_us) max_read_us=read_us;
            if(read_us>20000) slow_reads++;
        }
        if(got!=want) { error="Erreur lecture SD."; break; }
        size_t block_frames=got/(w.channels*sizeof(int16_t));
        size_t output_bytes=pcm_to_stereo(input,stereo,block_frames,w.channels,applied_volume==0);
        size_t sent=0;
        while(sent<output_bytes) {
            size_t wrote=0;
            if(i2s_write(I2S_NUM_0,(uint8_t*)stereo+sent,output_bytes-sent,&wrote,pdMS_TO_TICKS(1000))!=ESP_OK || !wrote) {
                error="Erreur sortie I2S."; goto cleanup;
            }
            sent+=wrote;
        }
        frames+=block_frames; remaining-=got;
        // Subtract the maximum queued DMA frames: conservative audible position.
        uint32_t audible=frames>AUDIO_DMA_COUNT*AUDIO_DMA_FRAMES?frames-AUDIO_DMA_COUNT*AUDIO_DMA_FRAMES:0;
        portENTER_CRITICAL(&player_lock);
        status.elapsed_ms=(uint32_t)((uint64_t)audible*1000/w.rate);
        portEXIT_CRITICAL(&player_lock);
        // More than the complete DMA capacity has been queued: playback has started.
        // Do not enable SAR entropy here; selection finished before audio/Wi-Fi.
        if(boot && !diagnostic && WIFI_FAST && frames>AUDIO_DMA_COUNT*AUDIO_DMA_FRAMES) start_wifi();
    }
    memset(stereo,0,sizeof(stereo));
    if(!stopped() && !error) for(int i=0;i<6;i++) {
        size_t wrote=0;
        if(i2s_write(I2S_NUM_0,stereo,sizeof(stereo),&wrote,pdMS_TO_TICKS(1000))!=ESP_OK || wrote!=sizeof(stereo)) {
            error="Erreur fin de lecture."; break;
        }
    }
    if(!stopped() && !error) {
        portENTER_CRITICAL(&player_lock); status.elapsed_ms=status.duration_ms; portEXIT_CRITICAL(&player_lock);
    }
cleanup:
    if(f) fclose(f);
    if(bus) codec_mute(&profile);
    if(installed) { i2s_stop(I2S_NUM_0); i2s_driver_uninstall(I2S_NUM_0); }
    if(bus) i2c_driver_delete(I2C_NUM_0);
    DISCO_LOGI(TAG,"Lecture terminee (%s) ; lecture SD max=%lu us, blocs >20 ms=%lu (pas un compteur de sous-alimentations DMA)",
               error?"erreur":stopped()?"stop":"fin",(unsigned long)max_read_us,(unsigned long)slow_reads);
    if(error) DISCO_LOGE(TAG,"%s",error);
    finish(error);
    // Startup SD configuration is handled by player_run, after audio cleanup.
}
void player_run(void) {
    esp_err_t err=nvs_flash_init();
    if(err!=ESP_OK) { DISCO_LOGE(TAG,"NVS : %s. Arret.",esp_err_to_name(err)); return; }
    nvs_handle_t nvs;
    if(nvs_open("audio_settings",NVS_READONLY,&nvs)==ESP_OK) {
        uint8_t v;
        if(nvs_get_u8(nvs,"volume",&v)==ESP_OK && v<=100) status.volume=v;
        nvs_close(nvs);
    }
    wifi_settings_load();
    selection_init();
    play_queue=xQueueCreate(1,sizeof(play_command));
    if(!play_queue) { finish("Memoire insuffisante."); start_wifi(); return; }
    DISCO_LOGI(TAG,"Volume memorise/initial=%d ; mode de selection charge depuis les reglages",current_volume());
    bool mounted=mount_sd();
    if(mounted) unlink(AUDIO_DIRECTORY "/.upload.part"); // Cleanup BEFORE all audio/HTTP.
    char chosen[512];
    if(AUDIO_DIAGNOSTIC) { strcpy(chosen,"test interne"); err=ESP_OK; }
    else err=mounted?selection_take(chosen):ESP_FAIL;
    if(err!=ESP_OK) DISCO_LOGE(TAG,"Selection impossible : %s",esp_err_to_name(err));
    if(err==ESP_OK) {
        portENTER_CRITICAL(&player_lock); strcpy(status.path,chosen); portEXIT_CRITICAL(&player_lock);
        play_file(chosen,true);
    } else { finish("Carte SD ou selection indisponible."); }
    // No wifi.txt lookup or credential commit before/during startup playback.
    if(mounted) wifi_settings_import();
    // FAST keeps its current connection; imported credentials apply next boot.
    // If no attempt was launched (including an unconfigured SSID), use them now.
    if(!wifi_attempted) { wifi_started=false; start_wifi(); }
    play_command cmd;
    for(;;) if(xQueueReceive(play_queue,&cmd,portMAX_DELAY)==pdTRUE) play_file(cmd.path,false);
}
