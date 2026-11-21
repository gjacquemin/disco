#pragma once
#include "esp_err.h"
#include "cJSON.h"
esp_err_t selection_take(char chosen[512]);
esp_err_t selection_reset(void); // Web only: Wi-Fi entropy is active, no SAR enable.
cJSON *selection_status(void);

void selection_init(void);
esp_err_t selection_set_mode(int mode);

esp_err_t selection_reset_first(const char *first);

const char *selection_directory(void);
const char *selection_playlist(void);
esp_err_t selection_playlist_set(const char *name);

esp_err_t selection_manage(const char *op,const char *name,const char *file);

#include <stdint.h>
uint32_t selection_duration(const char *path);

void selection_maintenance(void);
