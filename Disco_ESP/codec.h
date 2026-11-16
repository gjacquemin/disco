#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct { int kind,sda,scl,bclk,lrck,dout; uint8_t addr; const char *name; } codec_profile;
esp_err_t codec_detect(codec_profile *p);
esp_err_t codec_start(const codec_profile *p,uint32_t rate,int volume);
esp_err_t codec_mute(const codec_profile *p);

esp_err_t codec_set_volume(const codec_profile *p,int volume);

void codec_close(void);
