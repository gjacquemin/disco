# Migration de Disco vers Arduino-ESP32 3.x

## Conclusion

La carte ESP32-A1S et ses codecs AC101 / ES8388 peuvent être conservés. La migration concerne les pilotes logiciels qui transportent les échantillons (I2S) et commandent le codec (I²C), pas le matériel.

Cette variante utilise Arduino IDE et le paquet **esp32 par Espressif Systems 3.3.12** comme référence. Elle reste un sketch Arduino, sans projet natif supplémentaire. Le projet 2.0.17 est conservé séparément comme solution de repli. La compilation seule ne constitue pas une validation audio sur carte.

## Adaptations

| Partie | Ancienne version | Variante 3.x |
|---|---|---|
| Sortie audio | `driver/i2s.h`, pilote historique | `driver/i2s_std.h`, canal TX standard Philips |
| Cycle I2S | installation / écriture / désinstallation | allocation, configuration, activation, écriture, désactivation, suppression |
| Horloges | APLL, MCLK = fréquence × 256 | APLL et même rapport MCLK ; GPIO0 conservé |
| DMA | 8 tampons × 256 frames | mêmes dimensions, remise à zéro automatique |
| Commande codec | `driver/i2c.h` | `driver/i2c_master.h`, bus et périphérique explicites |
| Délais d'écriture | ticks FreeRTOS | millisecondes pour les nouvelles API |
| Empreinte du catalogue | fonctions SHA-256 avec suffixe `_ret` | API mbedTLS de la nouvelle version |
| Réglages et contenu | config locale, WAV mono/stéréo | mêmes réglages et formats |

Aucun mélange de pilotes historiques et modernes I2S/I²C dans cette variante. Les registres des codecs, leurs profils de broches et les règles de lecture sont conservés. Le bus I²C et le canal I2S sont libérés après chaque lecture, y compris en cas d'erreur, pour permettre les écoutes manuelles successives.

Le nouveau canal émet les horloges avant l'initialisation du codec. Les timeouts I2S sont exprimés en millisecondes. Le seuil du Wi-Fi fast reste basé sur le nombre de frames audio transmis, pour être identique en mono et en stéréo.

## Fonctions conservées

- WAV PCM16 mono/stéréo, 44,1 ou 48 kHz, à la racine de la SD FAT32.
- Un morceau au démarrage ; aucune reprise après coupure.
- Urne persistante ou tirage indépendant.
- Wi-Fi normal ou fast, serveur Web, écoute manuelle et arrêt.
- Volume mémorisé, gestion des fichiers et uploads jusqu'à 128 Mio.
- Tous les réglages dans `config.local.h`, ignoré par Git.

## Installer et revenir en arrière

1. Conserver la version précédente et son fichier personnel de configuration.
2. Dans Arduino IDE, installer le paquet Espressif **3.3.12**.
3. Ouvrir `Disco_ESP/Disco_ESP.ino` de cette variante.
4. Copier le modèle en `config.local.h`, ou reprendre les réglages personnels précédents.
5. Utiliser les réglages de carte du README : ESP32 Dev Module, DIO, 40 MHz, flash 4 Mo, Huge APP, PSRAM désactivée.
6. Compiler et téléverser. Tester d'abord à volume modéré.

Pour revenir en arrière, réinstaller 2.0.17 et recompiler les **anciennes sources**, pas celles de cette variante. Les noms des clés NVS de l'urne et du volume n'ont pas changé ; leur conservation dépend aussi du partitionnement et de l'état de NVS. Garder l'effacement complet désactivé lors d'une mise à jour ordinaire. Ne pas considérer la conservation de l'état comme garantie lors d'un changement de SDK ou d'un retour vers une ancienne version.

Le contrôle de version accepte la famille 3.x disposant d'ESP-IDF >= 5.2 pour le pilote I²C moderne ; ce contrôle n'est pas une certification de toutes ces versions. La référence documentée est **3.3.12**. Les versions 2.x et 4.x sont refusées.

## Vérifications sur carte nécessaires

1. Détection AC101 ou ES8388 et sortie sur EARPHONES.
2. WAV mono puis stéréo à 44,1 et 48 kHz ; vérifier la vitesse et la séparation gauche/droite.
3. Fin de morceau sans troncature et arrêt manuel.
4. Plusieurs écoutes manuelles successives, avec changement de fréquence.
5. Volume en cours de lecture, silence à zéro, mémorisation après extinction.
6. Wi-Fi fast pendant un fichier long ; absence de décrochage audio.
7. Coupures pendant lecture et succession exacte des morceaux de l'urne.

Aucun exemplaire physique de la carte n'a été utilisé pour cette migration. Les résultats de compilation et les contrôles logiciels figurent dans le rapport local de validation, exclu de Git.

## Références officielles

- [Migration Arduino-ESP32 2.x vers 3.0](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html)
- [Pilote I2S standard](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/api-reference/peripherals/i2s.html)
- [Pilote I²C maître](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/api-reference/peripherals/i2c.html)
- [Versions Arduino-ESP32](https://github.com/espressif/arduino-esp32/releases)
