// ESP32 by Espressif Systems 3.3.12, ESP32 Dev Module.
#include <Arduino.h>
#include <esp_arduino_version.h>
#include <esp_idf_version.h>
#if ESP_ARDUINO_VERSION_MAJOR != 3
#error "Cette version de Disco utilise Arduino-ESP32 3.x ; reference de compilation : 3.3.12."
#endif
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
#error "Pilote I2C moderne requis : utiliser Arduino-ESP32 3.3.12 pour cette variante."
#endif
extern "C" void player_run(void);
static void playerTask(void *) {
  player_run();
  vTaskDelete(nullptr);
}
void setup() {
  Serial.begin(115200);
  // Aucun while(!Serial), aucun WiFi.begin avant lecture.
  if (xTaskCreatePinnedToCore(playerTask,"player",12288,nullptr,5,nullptr,1)!=pdPASS)
    Serial.println("Erreur : creation tache lecteur impossible.");
}
void loop() { delay(1000); }
