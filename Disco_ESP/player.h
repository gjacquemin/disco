#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct { bool playing; int volume; uint32_t elapsed_ms, duration_ms; char path[512]; char error[128]; } player_status;
bool player_busy(void);
void player_get_status(player_status *out);
esp_err_t player_request(const char *path);
void player_stop(void);
esp_err_t player_set_volume(int volume);
