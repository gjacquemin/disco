#pragma once
// Reglages personnels : copier ce modele dans config.local.h.
// Seul config.local.h est a modifier ; ne jamais le publier.

// Reseau Wi-Fi 2,4 GHz (vide ou TON_SSID = desactive).
#define WIFI_SSID "TON_SSID"
#define WIFI_PASSWORD "TON_MOT_DE_PASSE"
#define WIFI_TIMEOUT_MS 15000
// 0 : Wi-Fi apres le morceau ; 1 : Wi-Fi des le debut effectif de lecture.
#define WIFI_FAST 0
// Volume initial : 0 a 100. Le volume memorise depuis le Web est prioritaire.
#define AUDIO_VOLUME 55
#define AUDIO_DIRECTORY "/sdcard"
// Mode de selection au demarrage.
#define PLAYBACK_RANDOM 0
#define PLAYBACK_URN 1
#define PLAYBACK_MODE PLAYBACK_URN
// Seulement en mode RANDOM : exclure le dernier morceau.
#define AVOID_LAST_TRACK 1

#define WEB_MAX_FILE_BYTES (64U * 1024U * 1024U)

// 0 automatique ; 1 AC101 ; 2 ES8388 variante 5 ; 3 ES8388 variante 7
#define CODEC_PROFILE 0
