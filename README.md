# Disco — Lecteur audio aléatoire

Lecteur pour **ESP32-A1S Audio Kit V2.2**, développé pour **Arduino IDE**. À chaque mise sous tension, il joue un seul WAV depuis la carte microSD, puis s'arrête. Une interface Web locale permet de gérer les fichiers, consulter l'urne, écouter un titre et régler le volume.

## Fonctionnalités

- Lecture dès le démarrage, sans attendre le réseau.
- Deux modes de sélection à la compilation : urne persistante ou tirage aléatoire indépendant.
- Aucune reprise après coupure : chaque morceau commence au début.
- Wi-Fi après le morceau par défaut ; option **fast** pour le lancer pendant la lecture.
- Interface Web sans Internet ni dépendance à un CDN : envoi, suppression, renommage, écoute manuelle, arrêt, volume mémorisé.
- Sortie mono dupliquée sur les deux canaux du jack EARPHONES.

## Matériel et branchements

- ESP32-A1S Audio Kit V2.2. Ce firmware vise cette carte, pas une LyraT-Mini.
- microSD FAT32, de préférence 8 à 32 Go.
- Alimentation 5 V / 2 A sur le port POWER ou UART/POWER selon la carte.
- Enceintes amplifiées branchées sur **EARPHONES**. LINEIN est une entrée ; LOUT/ROUT ne sont pas la sortie mini-jack.
- Pour programmer : port **UART/POWER** et câble micro-USB de données.

Ne pas brancher de batterie si la mise sous tension secteur doit déclencher la lecture. L'USB d'un ordinateur peut également maintenir la carte allumée.

### Interrupteurs DIP

Pour la carte portant les liaisons suivantes, lire le repère ON imprimé sur le boîtier :

| Interrupteur | Liaison | Position |
|---|---|---|
| 1 | IO13 / KEY2 | OFF |
| 2 | IO13 / DATA3 | ON |
| 3 | IO15 / CMD | ON |
| 4 | IO13 / MTCK | OFF |
| 5 | IO15 / MTDO | OFF |

La SD utilise le mode 1 bit : CLK14, CMD15, D0=2. Aucun pilotage d'alimentation sur GPIO13. Si la sérigraphie diffère, vérifier le schéma de la révision avant de reprendre ces positions.

## Préparer la carte SD

1. Formater en **FAT32**, une partition principale ; MBR recommandé. Pas d'exFAT et aucun formatage automatique par le programme.
2. Copier les fichiers WAV **directement à la racine** de la carte, sans dossier.
3. Insérer la carte avant d'allumer le lecteur. Pas de remplacement à chaud.

Exemples : `Bonjour.wav`, `Ambiance foret.wav`, `Musique_03.wav`. Aucun numéro ni préfixe n'est imposé. Les anciens fichiers placés dans un sous-dossier doivent être déplacés à la racine : les sous-dossiers ne sont pas parcourus.

Format accepté : **WAV PCM entier signé 16 bits, mono, 44 100 ou 48 000 Hz**. MP3, stéréo, 24 bits, float et WAV extensible ne sont pas acceptés. Les fichiers invalides sont exclus de l'urne.

Noms courts conseillés ; espaces et accents acceptés. Éviter les noms commençant par un point et les caractères `/ \ : * ? " < > |`. Limite : 1024 WAV compatibles ; conserver moins de fichiers réduit le temps de scan et la mémoire utilisée. Envoi Web limité par défaut à 64 Mio par fichier.

Conversion avec FFmpeg :
```sh
ffmpeg -i entree.wav -ac 1 -ar 48000 -c:a pcm_s16le sortie.wav
```

## Installer et configurer Arduino IDE

1. Installer Arduino IDE 2.x.
2. Ajouter cette URL dans Préférences → URL de gestionnaire de cartes supplémentaires :
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
3. Dans le gestionnaire de cartes, installer **esp32 par Espressif Systems, version 2.0.17 exactement**. Les versions 3.x ne conviennent pas au pilote I2S utilisé ici.
4. Copier `Disco_ESP/config.example.h` en `Disco_ESP/config.local.h` s'il n'existe pas déjà.
5. Ouvrir `Disco_ESP/Disco_ESP.ino`, avec tous ses fichiers voisins. Aucune bibliothèque supplémentaire à installer.
6. Renseigner les réglages dans **config.local.h**.

### Tous les réglages de l'IDE

| Option | Valeur |
|---|---|
| Carte | ESP32 Arduino → **ESP32 Dev Module** |
| Upload Speed | **115200** |
| CPU Frequency | **240 MHz (WiFi/BT)** |
| Flash Frequency | **40 MHz** |
| Flash Mode | **DIO** |
| Flash Size | **4MB (32Mb)** |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Core Debug Level | **Info** |
| PSRAM | **Disabled** |
| Arduino Runs On | **Core 1** |
| Events Run On | **Core 1** |
| Erase All Flash Before Sketch Upload | **Disabled** |
| Port | Port série de la carte |
| Moniteur série | **115200 bauds** |

Laisser les options absentes de l'IDE à leur valeur par défaut. Si le téléversement ne démarre pas : maintenir BOOT, appuyer brièvement sur RST, relâcher BOOT quand le transfert commence.

Ne pas effacer toute la flash lors des mises à jour ordinaires : l'urne et le volume y sont conservés. Un changement de partitionnement peut nécessiter un effacement, qui supprime ces réglages.

## Réglages du lecteur

Un seul fichier personnel à modifier : **Disco_ESP/config.local.h**. `config.h` sert uniquement de relais au code.

| Réglage | Valeur initiale | Rôle |
|---|---|---|
| `WIFI_SSID` | `"TON_SSID"` | Nom du réseau ; vide ou valeur initiale = Wi-Fi désactivé |
| `WIFI_PASSWORD` | `"TON_MOT_DE_PASSE"` | Mot de passe Wi-Fi |
| `WIFI_TIMEOUT_MS` | `15000` | Durée maximale de tentative de connexion |
| `WIFI_FAST` | `0` | `0` après le son ; `1` pendant le son, une fois la lecture commencée |
| `AUDIO_VOLUME` | `55` | Volume initial de 0 à 100, utilisé en l'absence de valeur mémorisée |
| `AUDIO_DIRECTORY` | `"/sdcard"` | Point de montage de la racine de la SD, à conserver pour cet usage |
| `PLAYBACK_MODE` | `PLAYBACK_URN` | Urne ; choisir `PLAYBACK_RANDOM` pour le tirage indépendant |
| `AVOID_LAST_TRACK` | `1` | Exclure le dernier titre, seulement en mode aléatoire |
| `WEB_MAX_FILE_BYTES` | `64U * 1024U * 1024U` | Taille maximale d'un envoi Web |
| `CODEC_PROFILE` | `0` | Détection automatique ; voir les profils ci-dessous |

Recompiler après modification. Le volume réglé depuis le Web est prioritaire sur `AUDIO_VOLUME`, y compris après une mise à jour du firmware sans effacement de flash.

### Wi-Fi normal et fast

```c
#define WIFI_FAST 0  // connexion seulement après la fin du morceau
// ou
#define WIFI_FAST 1  // connexion une fois le morceau en cours de lecture
```

Le mode fast commence la connexion après avoir envoyé plus que la capacité des tampons DMA à I2S : le son a commencé à sortir. Un fichier très court peut se terminer avant ce seuil ; la connexion commence alors à sa fin. La page devient accessible après obtention d'une adresse IP, pas instantanément au premier échantillon.

Le mode normal minimise l'activité réseau pendant le morceau de démarrage. Le mode fast et les écoutes manuelles font fonctionner réseau et audio ensemble : leur stabilité dépend de la carte, de l'alimentation et de la SD. Le délai de démarrage audio n'est pas garanti ; il inclut notamment le scan, la sauvegarde NVS et l'initialisation du codec.

Réseau **2,4 GHz**, personnel WPA/WPA2 ou ouvert ; pas de portail captif ni de Wi-Fi entreprise. Une tentative par démarrage, sans boucle de reconnexion. Si aucune lecture n'est possible, le lecteur tente tout de même le réseau pour permettre le diagnostic.

## Urne et tirage aléatoire

### Urne — mode par défaut

Tous les WAV compatibles sont mélangés une fois. Chaque allumage retire le premier morceau restant. Après épuisement, un nouvel ordre complet est créé au démarrage suivant.

La page affiche les **prochains allumages dans l'ordre réel**, sans le titre déjà tiré. Consulter la page ne consomme rien. Chaque fichier apparaît une fois par cycle ; une répétition entre la fin d'un cycle et le début du suivant reste possible.

Le bouton **Réinitialiser l'urne** remet tous les morceaux dans un nouveau mélange sans lancer de lecture. Un ajout, une suppression ou un renommage réussi fait automatiquement la même chose. Les changements hors ligne sont détectés au prochain démarrage par une empreinte du catalogue trié. Remplacer le contenu d'un WAV compatible sans changer son nom conserve sa place ; utiliser le bouton pour forcer un mélange.

### Aléatoire indépendant

```c
#define PLAYBACK_MODE PLAYBACK_RANDOM
```

Un nouveau choix est effectué à chaque démarrage. Avec `AVOID_LAST_TRACK=1`, le dernier titre est exclu s'il existe au moins deux WAV. Ce mode ne possède pas d'ordre futur affichable. Un passage ultérieur au mode urne retrouve l'état conservé si le catalogue est inchangé.

Le mélange et les tirages utilisent le générateur matériel avec sa source d'entropie active. La source SAR utilisée avant le démarrage est arrêtée avant I2S et Wi-Fi ; les mélanges demandés depuis le site utilisent le Wi-Fi déjà actif.

## Interface Web

Ouvrir **http://ADRESSE_IP/** depuis un appareil du même réseau. L'IP est affichée dans le moniteur série et dans la liste DHCP du routeur.

- **Carte SD · racine** : liste, envoi multiple, suppression et renommage sans écraser un fichier existant.
- **Prochains allumages** : ordre restant dans l'urne et remise à zéro.
- **Écouter** : joue le titre choisi **sur les enceintes raccordées au jack**, depuis le début, une seule fois. Le navigateur ne reçoit pas de flux audio.
- **Arrêter la lecture** : interrompt le morceau de démarrage ou une écoute manuelle. Attendre l'état « À l'arrêt » avant d'en lancer une autre.
- **Volume** : déplacer le curseur, puis cliquer **Appliquer et mémoriser**. La valeur s'applique à la lecture en cours et aux suivantes, et reste mémorisée après extinction. À 0, le signal audio envoyé est silencieux.

Les écoutes manuelles **ne consomment pas l'urne et ne changent pas la mémoire du dernier tirage aléatoire**. Elles ne démarrent pas une nouvelle séquence automatique.

Pendant une lecture, les envois, suppressions, renommages et remises à zéro de l'urne sont désactivés et refusés par le serveur. Le volume et l'arrêt restent disponibles. Une seule lecture à la fois ; aucune file de titres utilisateur.

Le volume est écrit en mémoire interne une fois par validation, pas à chaque mouvement du curseur. La sauvegarde flash et l'activité réseau peuvent occasionner une perturbation audible sur certaines cartes ; vérifier ce comportement avec le matériel utilisé. Si la sauvegarde échoue, la page l'indique et le nouveau volume n'est pas accepté.

Serveur HTTP local sans mot de passe : les appareils du même réseau peuvent gérer le lecteur. Ne pas exposer le port 80 sur Internet. Des noms très longs ou fortement encodés peuvent dépasser la limite des en-têtes HTTP ; les raccourcir en cas d'erreur 431.

## Coupures de courant

Le retrait de l'urne ou le dernier tirage est sauvegardé **avant le premier son**. Un morceau coupé compte donc comme tiré ; aucune position n'est conservée et aucune reprise n'a lieu au démarrage suivant. Une coupure après sauvegarde mais avant sortie audio peut consommer un titre sans qu'il ait été entendu.

En cas d'échec de sauvegarde du tirage, aucune lecture automatique n'est lancée. Un reset, y compris après téléversement, consomme également un titre. Un seul fichier compatible sera nécessairement rejoué à chaque allumage.

La SD est lue pendant la lecture. Les modifications de fichiers sont réservées aux périodes d'arrêt. **Attendre la confirmation des opérations Web avant de couper l'alimentation** : FAT32 n'est pas journalisé. Les envois utilisent un fichier temporaire `.upload.part`, ignoré par le lecteur et nettoyé au démarrage ; cela ne garantit pas l'intégrité de la SD lors d'une coupure pendant une écriture. NVS gère l'usure de la flash mais ne remplace pas une alimentation stable.

## Profils audio

| Profil | Codec | SDA / SCL | BCLK / LRCK / sortie | MCLK |
|---|---|---|---|---|
| 1 | AC101, adresse 0x1A | 33 / 32 | 27 / 26 / 25 | 0 |
| 2 | ES8388 variante 5, adresse 0x10 ou 0x11 | 33 / 32 | 27 / 25 / 26 | 0 |
| 3 | ES8388 variante 7, adresse 0x10 ou 0x11 | 18 / 23 | 5 / 25 / 26 | 0 |

`CODEC_PROFILE=0` essaie ces profils. Un seul candidat doit répondre. AC101 est vérifié par sa signature ; ES8388 par une lecture à l'adresse attendue, sans identifiant unique. Il ne s'agit pas d'une reconnaissance universelle des cartes. Aucun autre périphérique I2C ne doit être ajouté sur ces broches.

En cas de détection impossible ou ambiguë, consulter le moniteur série. Fixer `CODEC_PROFILE` à 1, 2 ou 3 uniquement lorsque la révision est connue. Après une erreur de lecture, l'état du lecteur apparaît aussi sur la page.

## Organisation et publication

| Chemin | Contenu |
|---|---|
| `Disco_ESP/` | Sketch Arduino et sources |
| `Disco_ESP/config.example.h` | Modèle public des réglages |
| `Disco_ESP/config.local.h` | Réglages personnels, ignorés par Git |
| `Disco_ESP/data/index.html` | Source de l'interface Web |
| `Disco_ESP/page.cpp` | Page embarquée, générée et versionnée |
| `tools/embed_web.py` | Régénération de la page avec Python 3 |
| `LICENSE`, `LICENSES/`, `THIRD_PARTY.md` | Licence et notices des dépendances |

Après modification du HTML :
```sh
python3 tools/embed_web.py
```
Puis recompiler dans Arduino IDE. La page est embarquée dans le firmware ; rien à copier sur la SD en dehors des WAV.

Le `.gitignore` exclut les réglages personnels, les tests locaux, le rapport local de validation et les fichiers de compilation. Garder le modèle sans identifiants réels. Vérifier avant publication :
```sh
git check-ignore Disco_ESP/config.local.h tests/
git ls-files '*config.local.h' 'tests/*'
```
La seconde commande ne doit rien afficher. Si ces fichiers étaient déjà suivis, les retirer de l'index sans les supprimer du disque :
```sh
git rm --cached --ignore-unmatch Disco_ESP/config.local.h
git rm -r --cached --ignore-unmatch tests
```

Un fichier ignoré déjà publié demeure dans l'historique. Si des identifiants réels ont été exposés dans une ancienne configuration, changer le mot de passe et nettoyer l'historique. Ne pas publier de firmware compilé avec des identifiants privés.

## Vérification sur matériel

Le projet doit être validé sur la révision de carte utilisée : sortie jack, mode fast, volume pendant la lecture et coupures d'alimentation. Vérifier notamment :

1. Trois WAV à la racine : un seul morceau au démarrage, puis silence.
2. Coupure pendant un morceau : démarrage suivant depuis le début du prochain titre de l'urne.
3. Mode fast : apparition de la page pendant un morceau suffisamment long.
4. Écoute manuelle puis arrêt : urne inchangée.
5. Volume modifié et mémorisé : même niveau après remise sous tension.
6. Modification de fichiers refusée pendant une lecture, autorisée après arrêt.

Aucun essai sur carte physique n'a été effectué pour cette version. Les compilations et contrôles logiciels ne remplacent pas ces vérifications.

## Licence

GPL-3.0-or-later. Voir [LICENSE](LICENSE) et [THIRD_PARTY.md](THIRD_PARTY.md) pour les pilotes adaptés et les notices à conserver.
