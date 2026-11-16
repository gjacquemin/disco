# Licences et provenance
Ce projet est distribue sous GPL-3.0-or-later (LICENSES/GPL-3.0.txt).
codec.c adapte les sequences AC101 de Ivo Pullens, Emmission (2019), https://github.com/Yveaux/AC101 (GPL-3.0-or-later), et la sequence ES8388 du pilote Espressif ESP-ADF v2.7 (2018), avec notice conservee dans LICENSES/Espressif-MIT.txt.
Les profils de broches sont documentes dans https://github.com/pschatzmann/arduino-audiokit/tree/main/src/audio_board (ai_thinker_ac101.h, ai_thinker_es8388_5.h, ai_thinker_es8388_7.h).
Modifications : portage I2C ESP-IDF, detection des profils a l'execution, lecture seule via jack, verification des retours I2C. Les SDK Espressif / Arduino s'installent separement selon leurs propres licences.

AC101 : d9071ea8280a900cb3452b35f22fbf8abfb5e42c

Profils AudioKit : f8c4139f67bba94623226bab23f315210eed2c6b

ESP-ADF : 9cf556de500019bb79f3bb84c821fda37668c052
