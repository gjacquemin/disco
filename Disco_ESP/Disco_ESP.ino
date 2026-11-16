// ESP32 by Espressif Systems 2.0.17, ESP32 Dev Module.
#include <Arduino.h>
#include "diagnostics.h"
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR != 2 || ESP_ARDUINO_VERSION_MINOR != 0 || ESP_ARDUINO_VERSION_PATCH != 17
#error "Installer esp32 by Espressif Systems version 2.0.17 dans le gestionnaire de cartes."
#endif
extern "C" void player_run(void);
static void playerTask(void *) {
  player_run();
  vTaskDelete(nullptr);
}
void setup() {
  Serial.begin(115200);
  DISCO_LOGI("DISCO","Demarrage ; Arduino %d.%d.%d ; diagnostic audio=%d",
             ESP_ARDUINO_VERSION_MAJOR,ESP_ARDUINO_VERSION_MINOR,ESP_ARDUINO_VERSION_PATCH,AUDIO_DIAGNOSTIC);
  // Aucun while(!Serial), aucun WiFi.begin avant lecture.
  if (xTaskCreatePinnedToCore(playerTask,"player",12288,nullptr,5,nullptr,1)!=pdPASS)
    Serial.println("Erreur : creation tache lecteur impossible.");
}
void loop() { delay(1000); }
