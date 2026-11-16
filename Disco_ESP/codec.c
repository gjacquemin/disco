/* AC101 register sequence adapted from AC101 by Ivo Pullens (2019), GPL-3.0-or-later.
 * ES8388 playback sequence adapted from Espressif ESP-ADF es8388.c (2018).
 * See THIRD_PARTY.md and LICENSES. Adaptation: native IDF I2C, runtime profiles,
 * checked transactions, headphone-only output, sample-rate selection.
 */
#include <stddef.h>
#include "codec.h"
#include "config.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "diagnostics.h"
static const codec_profile profiles[]={
 {1,33,32,27,26,25,0x1a,"AC101 / Audio Kit"},
 {2,33,32,27,25,26,0x10,"ES8388 / variante 5"},
 {2,18,23,5,25,26,0x10,"ES8388 / variante 7"}
};
static esp_err_t bus_open(const codec_profile *p) {
 i2c_config_t c={.mode=I2C_MODE_MASTER,.sda_io_num=p->sda,.scl_io_num=p->scl,
 .sda_pullup_en=GPIO_PULLUP_ENABLE,.scl_pullup_en=GPIO_PULLUP_ENABLE,.master.clk_speed=100000};
 esp_err_t e=i2c_param_config(I2C_NUM_0,&c);
 return e==ESP_OK?i2c_driver_install(I2C_NUM_0,I2C_MODE_MASTER,0,0,0):e;
}
static esp_err_t readreg(const codec_profile *p,uint8_t reg,uint8_t *data,size_t n) {
 return i2c_master_write_read_device(I2C_NUM_0,p->addr,&reg,1,data,n,pdMS_TO_TICKS(50));
}
static esp_err_t writeval(const codec_profile *p,uint8_t reg,uint16_t val) {
 uint8_t b[3]={reg,0,0}; size_t n;
 if(p->kind==1) { b[1]=val>>8; b[2]=val; n=3; } else { b[1]=val; n=2; }
 return i2c_master_write_to_device(I2C_NUM_0,p->addr,b,n,pdMS_TO_TICKS(100));
}
#define W(r,v) do { esp_err_t err=writeval(p,r,v); if(err!=ESP_OK) { DISCO_LOGE("CODEC","Ecriture registre 0x%02x : %s",(unsigned)(r),esp_err_to_name(err)); return err; } } while(0)
esp_err_t codec_detect(codec_profile *out) {
 unsigned hits=0;
 for(unsigned i=0;i<3;i++) {
  if(CODEC_PROFILE && CODEC_PROFILE!=(int)i+1) continue;
  codec_profile candidate=profiles[i];
  if(bus_open(&candidate)!=ESP_OK) continue;
  for(unsigned a=0;a<(candidate.kind==1?1:2);a++) {
   candidate.addr=profiles[i].addr+a;
   uint8_t b[2]={0};
   esp_err_t e=readreg(&candidate,0,b,candidate.kind==1?2:1);
   // AC101 has a readable chip signature; ES8388 has no unique chip ID.
   bool match=e==ESP_OK && (candidate.kind==2 || (b[0]==1 && b[1]==1));
   if(match) { *out=candidate; hits++; }
  }
  i2c_driver_delete(I2C_NUM_0);
  gpio_reset_pin(candidate.sda); gpio_reset_pin(candidate.scl);
 }
 if(hits!=1) { DISCO_LOGE("CODEC","Detection impossible ou ambigue : %u candidats. Aucun codec initialise.",hits); return ESP_ERR_NOT_FOUND; }
 DISCO_LOGI("CODEC","%s ; I2C SDA=%d SCL=%d adresse=0x%02x",out->name,out->sda,out->scl,out->addr);
 return bus_open(out);
}
esp_err_t codec_start(const codec_profile *p,uint32_t rate,int volume) {
 if(rate!=44100 && rate!=48000) return ESP_ERR_INVALID_ARG;
 if(volume<0) volume=0;
 if(volume>100) volume=100;
 gpio_set_direction(GPIO_NUM_21,GPIO_MODE_OUTPUT); gpio_set_level(GPIO_NUM_21,0);
 if(p->kind==1) {
  W(0x00,0x0123); vTaskDelay(pdMS_TO_TICKS(100));
  uint8_t id[2];
  if(readreg(p,0,id,2)!=ESP_OK || id[0]!=1 || id[1]!=1) return ESP_FAIL;
  W(0x58,0xe880); W(0x01,0x014f); W(0x02,0x8600);
  W(0x03,0x8b08); W(0x04,0x800c); W(0x05,0x800c);
  W(0x06,rate==44100?0x7000:0x8000);
  W(0x10,0x8850); // slave, 16 bits I2S, BCLK div8, LRCK div32
  W(0x11,0xc000); W(0x12,0xc000); W(0x13,0x2200);
  W(0x52,0xccc4); W(0x51,0x2020); W(0x40,0x8000); W(0x50,0xbbc3);
  W(0x4c,0xcc00); W(0x48,0x8000); W(0x54,0x0081); W(0x53,0xf080);
  W(0x53,0xff80); vTaskDelay(pdMS_TO_TICKS(100));
  W(0x56,(0xfbc0 & ~(63<<4)) | ((volume*63/100)<<4));
  W(0x58,0xe880); // speaker disabled; jack only
 } else {
  W(0x19,0x04); // mute
  W(0x01,0x50); W(0x02,0x00); W(0x35,0xa0); W(0x37,0xd0); W(0x39,0xd0);
  W(0x08,0x00); W(0x04,0xc0); W(0x00,0x12);
  W(0x17,0x18); W(0x18,0x02); // 16 bits I2S, MCLK/LRCK=256
  W(0x26,0x00); W(0x27,0x90); W(0x2a,0x90); W(0x2b,0x80); W(0x2d,0x00);
  W(0x1a,0x00); W(0x1b,0x00);
  W(0x2e,volume*33/100); W(0x2f,volume*33/100);
  W(0x30,volume*33/100); W(0x31,volume*33/100);
  W(0x03,0xff); // ADC off
  W(0x0d,0x02); // ADC LRCK ratio (shared clock source)
  W(0x04,0x3c); W(0x02,0xf0); W(0x02,0x00);
  W(0x19,volume==0?0x04:0x00);
 }
 return ESP_OK;
}
esp_err_t codec_mute(const codec_profile *p) {
 if(p->kind==1) { W(0x56,0x0001); W(0x58,0xe880); }
 else { W(0x19,0x04); W(0x04,0xc0); }
 return ESP_OK;
}

// Called only by the audio worker, never concurrently with codec start/mute.
esp_err_t codec_set_volume(const codec_profile *p,int volume) {
 if(volume<0 || volume>100) return ESP_ERR_INVALID_ARG;
 if(p->kind==1) {
  W(0x56,(0xfbc0 & ~(63<<4)) | ((volume*63/100)<<4));
 } else {
  W(0x2e,volume*33/100); W(0x2f,volume*33/100);
  W(0x30,volume*33/100); W(0x31,volume*33/100);
  W(0x19,volume==0?0x04:0x00);
 }
 return ESP_OK;
}
