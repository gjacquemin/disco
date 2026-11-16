#pragma once
#include "esp_err.h"
#include "cJSON.h"
esp_err_t selection_take(char chosen[512]);
esp_err_t selection_reset(void); // Web only: Wi-Fi entropy is active, no SAR enable.
cJSON *selection_status(void);

void selection_init(void);
esp_err_t selection_set_mode(int mode);
