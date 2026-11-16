# Disco — Lecteur audio aléatoire

Lecteur pour **ESP32-A1S Audio Kit V2.2**, développé pour **Arduino IDE**. À chaque mise sous tension, il joue un seul WAV depuis la carte microSD, puis s'arrête. Une interface Web locale permet de gérer les fichiers, consulter l'urne, écouter un titre et régler le volume.

## Fonctionnalités

- Lecture dès le démarrage, sans attendre le réseau.
- Deux modes sélectionnables depuis le Web et mémorisés : Urne persistante ou Random.
- Aucune reprise après coupure : chaque morceau commence au début.
- **Wi-Fi fast activé par défaut** : connexion Wi-Fi dès que la lecture a commencé.
- Interface Web sans Internet ni dépendance à un CDN : envoi, suppression, renommage, écoute manuelle, arrêt, volume mémorisé.
- WAV mono dupliqués sur les deux sorties ; WAV stéréo avec canaux gauche et droit conservés sur le jack EARPHONES.

## Documentation

- [HARDWARE.md](HARDWARE.md) : matériel, câblage, alimentation, interrupteurs DIP et profils de codec.
- [PLAYLIST.md](PLAYLIST.md) : sélection musicale recommandée et préparation des extraits.
- [Process_WAV_for_SD_macOS.command](Process_WAV_for_SD_macOS.command) : conversion et copie des WAV depuis macOS.

## Préparer la carte SD

1. Formater en **FAT32**, une partition principale ; MBR recommandé. Pas d'exFAT et aucun formatage automatique par le programme. Une carte de 64 Go est utilisable si elle est bien formatée en FAT32 ; sauvegarder son contenu avant tout formatage.
2. Copier les fichiers WAV **directement à la racine** de la carte, sans dossier.
3. Insérer la carte avant d'allumer le lecteur. Pas de remplacement à chaud.

Exemples : `Bonjour.wav`, `Ambiance foret.wav`, `Musique_03.wav`. Aucun numéro ni préfixe n'est imposé. Les anciens fichiers placés dans un sous-dossier doivent être déplacés à la racine : les sous-dossiers ne sont pas parcourus.

Format accepté : **WAV PCM entier signé 16 bits, mono ou stéréo, 44 100 ou 48 000 Hz**. MP3, 24 bits, float et WAV extensible ne sont pas acceptés. Les fichiers invalides sont exclus de l'urne.

Noms courts conseillés ; espaces et accents acceptés. Éviter les noms commençant par un point et les caractères `/ \ : * ? " < > |`. Limite : 1024 WAV compatibles ; conserver moins de fichiers réduit le temps de scan et la mémoire utilisée. Envoi Web limité par défaut à 128 Mio par fichier.

Conversion avec FFmpeg :
```sh
ffmpeg -i entree.wav -ar 44100 -c:a pcm_s16le sortie.wav
```

Cette conversion conserve les canaux de la source. Ajouter `-ac 1` pour forcer le mono ou `-ac 2` pour forcer la stéréo. Mono et stéréo peuvent cohabiter sur la carte, sans réglage supplémentaire.

### Préparation avec le script macOS

Le script [Process_WAV_for_SD_macOS.command](Process_WAV_for_SD_macOS.command) fonctionne sous **macOS uniquement**, avec `ffmpeg` et `ffprobe` installés. Par exemple, si Homebrew est déjà installé : `brew install ffmpeg`.

1. Placer le script à côté des WAV source, déjà découpés si nécessaire.
2. Au besoin, le rendre exécutable : `chmod +x Process_WAV_for_SD_macOS.command`.
3. Le lancer depuis le Terminal avec `./Process_WAV_for_SD_macOS.command` ou par double-clic.
4. Les conversions sont écrites dans **SD/** en WAV PCM16 **44,1 kHz stéréo**, et dans **MP3/** en MP3 128 kbit/s pour l'écoute sur ordinateur. Les originaux sont conservés. Les WAV dépassant 128 Mio après conversion sont ignorés. Disco ne lit pas les MP3.
5. Choisir explicitement le volume cible et confirmer son nom. Le script vérifie qu'il s'agit d'un volume externe ou amovible, inscriptible et en FAT32, puis copie les WAV **à sa racine**, sans écraser les fichiers existants.
6. Le script synchronise les écritures et tente de démonter le volume après copie. Ne retirer la carte qu'après confirmation du démontage ou éjection manuelle réussie.

Le script ne formate aucun disque. Les dossiers locaux SD/ et MP3/ ne sont pas à copier en tant que dossiers sur la carte. Les fichiers convertis portant déjà le même nom sont ignorés ; renommer les fichiers concernés pour retraiter une nouvelle version. Les noms qui deviennent identiques après nettoyage sont signalés et ignorés. Aucun nettoyage général des fichiers cachés du dossier de travail n'est effectué.

## Installer et configurer Arduino IDE

1. Installer Arduino IDE 2.x.
2. Ajouter cette URL dans Préférences → URL de gestionnaire de cartes supplémentaires :
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
3. Dans le gestionnaire de cartes, installer **esp32 par Espressif Systems, version 2.0.17 exactement**. Cette version 2.0.17 est la référence retenue après essais sans grésillement sur la carte cible. La migration 3.x est reportée ; ne pas compiler ce projet avec un cœur 3.x.
4. Copier `Disco_ESP/config.example.h` en `Disco_ESP/config.local.h` s'il n'existe pas déjà.
5. Ouvrir `Disco_ESP/Disco_ESP.ino`, avec tous ses fichiers voisins. Aucune bibliothèque supplémentaire à installer.
6. Renseigner les réglages dans **config.local.h**. Pour une mise à jour, conserver les identifiants existants et vérifier `WIFI_FAST 1` : un ancien fichier personnel avec la valeur 0 ne sera pas modifié automatiquement.

### Tous les réglages de l'IDE

Sélectionner la carte **ESP32 Arduino → ESP32 Dev Module** et son port série. Régler ensuite les options dans l'ordre du menu :

| Option | Valeur |
|---|---|
| CPU Frequency | **240 MHz (WiFi/BT)** |
| Core Debug Level | **Info** |
| Erase All Flash Before Sketch Upload | **Disabled** |
| Events Run On | **Core 1** |
| Flash Frequency | **40 MHz** |
| Flash Mode | **DIO** |
| Flash Size | **4MB (32Mb)** |
| JTAG Adapter | **Disabled** |
| Arduino Runs On | **Core 1** |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| PSRAM | **Disabled** |
| Upload Speed | **115200** |

Régler le **moniteur série à 115200 bauds**.

Laisser les options absentes de l'IDE à leur valeur par défaut. Si le téléversement ne démarre pas : maintenir BOOT, appuyer brièvement sur RST, relâcher BOOT quand le transfert commence.

Ne pas effacer toute la flash lors des mises à jour ordinaires : l'urne et le volume y sont conservés. Un changement de partitionnement peut nécessiter un effacement, qui supprime ces réglages.

## Réglages du lecteur

Un seul fichier personnel à modifier : **Disco_ESP/config.local.h**. `config.h` sert uniquement de relais au code.

| Réglage | Valeur initiale | Rôle |
|---|---|---|
| `WIFI_SSID` | `"TON_SSID"` | Nom du réseau ; vide ou valeur initiale = Wi-Fi désactivé |
| `WIFI_PASSWORD` | `"TON_MOT_DE_PASSE"` | Mot de passe Wi-Fi |
| `WIFI_TIMEOUT_MS` | `15000` | Durée maximale de tentative de connexion |
| `WIFI_FAST` | `1` | `0` après le son ; `1` pendant le son, une fois la lecture commencée |
| `AUDIO_VOLUME` | `55` | Volume initial de 0 à 100, utilisé en l'absence de valeur mémorisée |
| `AUDIO_DIRECTORY` | `"/sdcard"` | Point de montage de la racine de la SD, à conserver pour cet usage |
| `PLAYBACK_MODE` | `PLAYBACK_URN` | Mode initial ; le choix Random/Urne mémorisé depuis le Web est prioritaire |
| `AVOID_LAST_TRACK` | `1` | Exclure le dernier titre, seulement en mode aléatoire |
| `WEB_MAX_FILE_BYTES` | `128U * 1024U * 1024U` | Taille maximale d'un envoi Web |
| `CODEC_PROFILE` | `0` | Détection automatique ; voir [HARDWARE.md](HARDWARE.md#profils-audio) |

Recompiler après modification. Le mode et le volume réglés depuis le Web sont prioritaires sur `PLAYBACK_MODE` et `AUDIO_VOLUME`, y compris après une mise à jour du firmware sans effacement de flash.

### Wi-Fi normal et fast

```c
#define WIFI_FAST 1  // par défaut : connexion une fois le morceau en cours de lecture
// ou
#define WIFI_FAST 0  // connexion seulement après la fin du morceau
```

Le mode fast commence la connexion après avoir envoyé plus que la capacité des tampons DMA à I2S : le son a commencé à sortir. Un fichier très court peut se terminer avant ce seuil ; la connexion commence alors à sa fin. La page devient accessible après obtention d'une adresse IP, pas instantanément au premier échantillon.

Le mode normal minimise l'activité réseau pendant le morceau de démarrage. Le mode fast et les écoutes manuelles font fonctionner réseau et audio ensemble : leur stabilité dépend de la carte, de l'alimentation et de la SD. Le délai de démarrage audio n'est pas garanti ; il inclut notamment le scan, la sauvegarde NVS et l'initialisation du codec.

Réseau **2,4 GHz**, personnel WPA/WPA2 ou ouvert ; pas de portail captif ni de Wi-Fi entreprise. Une tentative par démarrage, sans boucle de reconnexion. Si aucune lecture n'est possible, le lecteur tente tout de même le réseau pour permettre le diagnostic.

## Urne et tirage aléatoire

### Urne — mode par défaut

Tous les WAV compatibles sont mélangés une fois. Chaque allumage retire le premier morceau restant. Après épuisement, un nouvel ordre complet est créé au démarrage suivant.

La page affiche les **prochains allumages dans l'ordre réel**, sans le titre déjà tiré. Consulter la page ne consomme rien. Chaque fichier apparaît une fois par cycle ; une répétition entre la fin d'un cycle et le début du suivant reste possible.

Le bouton **Régénérer la liste** remet tous les morceaux dans un nouveau mélange sans lancer de lecture. Un ajout, une suppression ou un renommage réussi fait automatiquement la même chose. Les changements hors ligne sont détectés au prochain démarrage par une empreinte du catalogue trié. Remplacer le contenu d'un WAV compatible sans changer son nom conserve sa place ; utiliser le bouton pour forcer un mélange.

### Random — aléatoire indépendant

Choisir **Random** dans la section **Mode de fonctionnement**. Le choix est enregistré pour les prochains démarrages, sans interrompre le morceau actuel. `PLAYBACK_MODE` ne fournit que la valeur initiale lorsqu’aucun mode n’a encore été mémorisé.

Un nouveau choix est effectué à chaque démarrage. Avec `AVOID_LAST_TRACK=1`, le dernier titre est exclu s'il existe au moins deux WAV. Ce mode ne possède pas d'ordre futur affichable. Un passage ultérieur au mode urne retrouve l'état conservé si le catalogue est inchangé.

Le mélange et les tirages utilisent le générateur matériel avec sa source d'entropie active. La source SAR utilisée avant le démarrage est arrêtée avant I2S et Wi-Fi ; les mélanges demandés depuis le site utilisent le Wi-Fi déjà actif.

## Interface Web

Ouvrir **http://ADRESSE_IP/** depuis un appareil du même réseau. L'IP est affichée dans le moniteur série et dans la liste DHCP du routeur.

### Lecture

La liste déroulante contient tous les WAV et sélectionne le morceau en cours. Choisir un autre titre arrête le précédent, attend la libération du codec puis lance le nouveau depuis le début. **STOP** arrête la lecture ; le sélecteur revient sur « Choisir un morceau… », ce qui permet aussi de relancer le même titre.

La barre de progression et les durées écoulée/totale sont actualisées environ toutes les 750 ms. La position est une estimation calculée à partir des trames transmises, en tenant compte de la capacité DMA (écart possible d’environ un tampon audio). La barre n’est pas un contrôle de déplacement dans le morceau.

Le slider de volume affiche la valeur pendant le déplacement et l’applique et la mémorise au relâchement, sans bouton de validation. Aucun enregistrement n’a lieu à chaque mouvement ; une valeur inchangée n’est pas réécrite. Les erreurs de sauvegarde sont affichées. Le volume demeure mémorisé après extinction.

### Mode de fonctionnement

Choisir **Random** ou **Urne**. Le mode est mémorisé pour les prochains allumages, sans consommer de morceau ni changer la lecture actuelle. En mode Urne, la liste compacte affiche les titres à venir dans leur ordre. **Régénérer la liste** remet tous les titres dans un nouveau mélange, sans lancer de son. Un changement de mode conserve l’urne existante ; les modifications de fichiers la reconstruisent, quel que soit le mode actif.

### Fichiers

Déposer un ou plusieurs WAV dans la zone dédiée, ou utiliser le sélecteur de fichiers, puis cliquer **Téléverser**. La liste compacte indique chaque nom, sa taille en Mio et les actions **Renommer** et **Supprimer**. Les fichiers restent à la racine de la carte. Un fichier existant n’est pas écrasé.

Pendant une lecture, les téléversements, suppressions, renommages et régénérations d’urne sont bloqués côté interface et côté serveur. Appuyer sur STOP pour les effectuer. Le changement de titre, le volume et le mode restent accessibles. Les commandes Web sont traitées successivement.

Les écoutes manuelles **ne consomment pas l’urne et ne changent pas le dernier tirage Random**. Le son sort sur le jack de la carte, pas dans le navigateur. La lecture ne s’enchaîne pas automatiquement.

Serveur HTTP local sans mot de passe : les appareils du même réseau peuvent gérer le lecteur. Ne pas exposer le port 80 sur Internet. Des noms très longs ou fortement encodés peuvent dépasser la limite des en-têtes HTTP ; les raccourcir en cas d'erreur 431.

## Coupures de courant

Le retrait de l'urne ou le dernier tirage est sauvegardé **avant le premier son**. Un morceau coupé compte donc comme tiré ; aucune position n'est conservée et aucune reprise n'a lieu au démarrage suivant. Une coupure après sauvegarde mais avant sortie audio peut consommer un titre sans qu'il ait été entendu.

En cas d'échec de sauvegarde du tirage, aucune lecture automatique n'est lancée. Un reset, y compris après téléversement, consomme également un titre. Un seul fichier compatible sera nécessairement rejoué à chaque allumage.

La SD est lue pendant la lecture. Les modifications de fichiers sont réservées aux périodes d'arrêt. **Attendre la confirmation des opérations Web avant de couper l'alimentation** : FAT32 n'est pas journalisé. Les envois utilisent un fichier temporaire `.upload.part`, ignoré par le lecteur et nettoyé au démarrage ; cela ne garantit pas l'intégrité de la SD lors d'une coupure pendant une écriture. NVS gère l'usure de la flash mais ne remplace pas une alimentation stable.

## Organisation et publication

| Chemin | Contenu |
|---|---|
| `Disco_ESP/` | Sketch Arduino et sources |
| `Disco_ESP/config.example.h` | Modèle public des réglages |
| `Disco_ESP/config.local.h` | Réglages personnels, ignorés par Git |
| `Disco_ESP/data/index.html` | Source de l'interface Web |
| `Disco_ESP/page.cpp` | Page embarquée, générée et versionnée |
| `HARDWARE.md`, `PLAYLIST.md` | Montage et sélection musicale |
| `Process_WAV_for_SD_macOS.command` | Préparation des fichiers sur macOS |
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

1. Trois WAV mono et stéréo à la racine : un seul morceau au démarrage, puis silence.
2. Avec un WAV de test gauche/droite, vérifier la séparation des canaux sur les enceintes. Coupure pendant un morceau : démarrage suivant depuis le début du prochain titre de l'urne.
3. Mode fast : apparition de la page pendant un morceau suffisamment long.
4. Écoute manuelle puis arrêt : urne inchangée.
5. Volume modifié et mémorisé : même niveau après remise sous tension.
6. Modification de fichiers refusée pendant une lecture, autorisée après arrêt.

La lecture audio et le Wi-Fi fast de la base Arduino 2.0.17 ont été confirmés par l’utilisateur sur sa carte. Les nouvelles commandes Web doivent encore être vérifiées sur matériel ; les tests logiciels ne remplacent pas ces essais.

## Licence

GPL-3.0-or-later. Voir [LICENSE](LICENSE) et [THIRD_PARTY.md](THIRD_PARTY.md) pour les pilotes adaptés et les notices à conserver.

## Diagnostic : console et grésillements

Le journal propre à Disco utilise directement `Serial`, à **115200 bauds**,
avec `SERIAL_DIAGNOSTICS 1` dans `config.local.h`. Il est indépendant de
**Core Debug Level** (qui reste utile pour les traces des pilotes Espressif).
Ouvrir le moniteur série puis appuyer sur **RST** : les messages du démarrage
peuvent être manqués si le moniteur est ouvert après la mise sous tension.
Aucune attente du moniteur n'est ajoutée au démarrage.

Les traces donnent la version Arduino, le volume effectif, le codec détecté,
les broches I2S, le format du morceau, les erreurs et la connexion Wi-Fi.
La durée maximale des lectures SD est affichée en fin de lecture ; ce chiffre
ne prouve pas, à lui seul, qu'une coupure audio a eu lieu.

Pour isoler un grésillement :

1. Essayer `WIFI_FAST 0`, un volume Web de 25 à 35 et la sortie **EARPHONES**.
   Le volume mémorisé depuis le Web prime sur `AUDIO_VOLUME`.
2. Compiler avec `AUDIO_DIAGNOSTIC 1` : au démarrage, Disco produit une sinusoïde
   de 1 kHz pendant 5 secondes, puis 2 secondes de silence numérique.
   Le signal est généré en mémoire, sans lire de WAV ni consommer l'urne,
   même sans SD. Le volume est temporairement fixé à 35, sans toucher au volume
   mémorisé. Le Wi-Fi ne démarre qu'après le test.
3. Comparer l'écoute avec un casque filaire sur EARPHONES, puis avec les enceintes.
   Si seul le branchement des enceintes produit du bruit, essayer une alimentation
   séparée pour elles afin de vérifier une éventuelle perturbation par l'alimentation
   ou la masse commune.
4. Si le test interne est propre mais les WAV grésillent, vérifier les fichiers
   sur ordinateur, puis essayer un WAV PCM16 à 44,1 kHz et une autre carte SD.
   Si le test interne grésille aussi au casque, transmettre les traces complètes :
   cela oriente vers la configuration codec/I2S ou le matériel, sans permettre
   encore de conclure lequel est en cause.
5. Remettre `AUDIO_DIAGNOSTIC 0` après les essais.

Le tampon DMA dispose de 16 blocs de 256 trames et les lectures SD sont regroupées
par blocs de 4 Kio pour mieux absorber les variations de latence. Cela ne garantit
pas la disparition d'un bruit analogique ou d'un problème d'horloge.
