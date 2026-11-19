#pragma once
// Interface partagee par les sources : aucun reglage personnel ici.
#if __has_include("config.local.h")
#include "config.local.h"
#if PLAYBACK_MODE != PLAYBACK_RANDOM && PLAYBACK_MODE != PLAYBACK_URN
#error "PLAYBACK_MODE invalide dans config.local.h"
#endif
#else
#error "Copiez config.example.h en config.local.h dans ce dossier, puis renseignez vos reglages."
#endif

// Compatibilite avec les configurations personnelles plus anciennes.
#ifndef WEB_MAX_HTML_BYTES
#define WEB_MAX_HTML_BYTES (1024U * 1024U)
#endif

#ifndef WIFI_SD_FILE
#define WIFI_SD_FILE "/sdcard/wifi.txt"
#endif
