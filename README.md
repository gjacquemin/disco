# Disco — Lecteur audio aléatoire

Lecteur pour **ESP32-A1S Audio Kit V2.2**, développé pour **Arduino IDE**. À chaque mise sous tension, il joue un seul WAV depuis la carte microSD, puis s'arrête. Une interface Web locale permet de gérer les fichiers, consulter l'urne, écouter un titre et régler le volume.

## Fonctionnalités

- Lecture dès le démarrage, sans attendre le réseau.
- Deux modes sélectionnables depuis le Web et mémorisés : Urne persistante ou Random.
- Aucune reprise après coupure : chaque morceau commence au début.
- **Wi-Fi fast activé par défaut** : connexion dès que la lecture a commencé.
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
| `WEB_MAX_HTML_BYTES` | `1024U * 1024U` | Taille maximale de chaque fichier Web sur SD (1 Mio) |
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

La barre de progression et les durées écoulée/totale sont actualisées environ toutes les 1,5 seconde. La position est une estimation calculée à partir des trames transmises, en tenant compte de la capacité DMA (écart possible d’environ un tampon audio). La barre n’est pas un contrôle de déplacement dans le morceau.

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

Pour actualiser la page de secours embarquée dans le firmware :
```sh
python3 tools/embed_web.py
```
Puis recompiler dans Arduino IDE. La page est embarquée dans le firmware comme secours. Pour modifier seulement le HTML/CSS/JavaScript après installation du firmware compatible, téléverser `Disco_ESP/data/index.html` depuis la section Fichiers → Mettre à jour l’interface Web, sans recompiler. Les modifications du code C/C++ ou des API nécessitent toujours un téléversement du firmware.

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

## Voyant de connexion

Le voyant vert **Connecté** indique que le navigateur reçoit une réponse de l’ESP32,
pas simplement que le téléphone ou l’ordinateur dispose d’une connexion Internet.
Une vérification est effectuée environ toutes les 1,5 seconde, avec un délai maximal
de réponse de 3 secondes. Au premier échec, le voyant passe au rouge **Déconnecté**,
les commandes sont désactivées et l’affichage de lecture est remis à l’état déconnecté.
La page reste ouverte pour détecter le retour de la carte ; elle se recharge entièrement
une seule fois lors du passage rouge → vert. Le premier affichage connecté ne provoque
pas de rechargement en boucle. Si l’adresse IP de la carte change, ouvrir sa nouvelle adresse.

La surveillance est suspendue pendant une commande ou un téléversement : le serveur
ESP32 traite les requêtes successivement, et son occupation ne doit pas être confondue
avec une déconnexion. Elle reprend à la fin de l’opération.

## Smartphone et écran d’accueil

L’interface s’adapte aux téléphones en portrait et paysage : commandes tactiles d’au moins 44 px, noms longs sur plusieurs lignes, prise en compte des encoches et de la zone du geste d’accueil. Le zoom du navigateur reste disponible. Le curseur de volume conserve son comportement : enregistrement au relâchement du doigt seulement.

- **iPhone / iPad** : ouvrir l’adresse du lecteur dans Safari, puis **Partager → Sur l’écran d’accueil** (ou **Ajouter à l’écran d’accueil**). Activer « Ouvrir comme app » si cette option est proposée, puis Ajouter. Le nom est **Disco**, avec une boule disco dorée et une note de musique.
- **Android** : ouvrir l’adresse dans Chrome, puis le menu **Ajouter à l’écran d’accueil**. Selon le navigateur et sa version, il peut s’agir d’un raccourci ouvrant le navigateur plutôt que d’une application autonome.
- Le téléphone doit pouvoir joindre la carte sur son réseau local. Une adresse réservée dans le DHCP du routeur évite que le raccourci devienne obsolète si l’adresse change.

Le manifeste demande un affichage autonome et les métadonnées iOS sont incluses. Le serveur ESP32 utilise HTTP : l’installation PWA complète n’est donc pas garantie, car elle exige normalement HTTPS (hors localhost). Aucun service worker ni cache hors connexion n’est installé ; le lecteur doit être accessible pour ouvrir la page. Au retour au premier plan, la page vérifie la connexion. La musique sort toujours sur le jack de la carte, pas sur le téléphone.

Pour cette version, **reflasher une fois le firmware** afin d’ajouter les routes du manifeste et des icônes. Si un ancien `index.html` est déjà présent sur la SD, téléverser aussi le nouveau `Disco_ESP/data/index.html` depuis l’interface (ou restaurer la page embarquée), sinon il reste prioritaire. Les évolutions suivantes de la mise en page restent téléversables sans reflash. Les icônes et le manifeste sont embarqués et disponibles même dans `/recovery` ; leur modification nécessite de régénérer `page.cpp` / `web_assets.h` avec `python3 tools/embed_web.py`, puis de reflasher. Supprimer puis recréer le raccourci si le téléphone conserve une ancienne icône.

Sources : [installation PWA (MDN)](https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps/Guides/Making_PWAs_installable) ; [configuration des applications Web iOS (Apple)](https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/SafariWebContent/ConfiguringWebApplications/ConfiguringWebApplications.html).

## Dossier Web sur la SD

Cette version utilise `/www/index.html`. Déplacer l’ancien `index.html` de la racine vers `/www/index.html` : il n’est plus chargé à la racine. Les WAV restent à la racine, sans dossier. Le dossier `/www` est créé au premier téléversement Web.

Exemple : `/www/index.html`, `/www/style.css`, `/www/app.js`, `/www/image.png`. Dans le HTML : `<link rel="stylesheet" href="style.css">`, `<script src="app.js"></script>` et `<img src="image.png" alt="">`. Pas de sous-dossiers. Utiliser des URL absolues pour les API (`/api/player`, etc.) et `/recovery`.

Depuis Fichiers → Mettre à jour l’interface Web, sélectionner plusieurs fichiers, puis Téléverser les fichiers Web. Le nom est conservé et un fichier existant est remplacé. Les dépendances sont envoyées avant `index.html` ; un échec interrompt le lot. Cliquer ensuite sur Ouvrir l’interface. La liste affiche les tailles et permet la suppression. Les écritures nécessitent STOP. Formats : HTML, JS, CSS, PNG, JPG/JPEG, GIF, WebP, SVG, ICO, JSON et WOFF2 ; limite `WEB_MAX_HTML_BYTES` (1 Mio) par fichier, y compris images. Les noms ne doivent pas contenir de séparateurs ni commencer par un point.

La racine HTTP redirige vers `/www/index.html` pour résoudre correctement les liens relatifs. Si l’index manque, est vide, trop gros ou inaccessible, l’interface embarquée prend le relais ; `/recovery` reste toujours disponible. Restaurer la page embarquée supprime seulement `/www/index.html`, en conservant les autres ressources. Pour une page HTML invalide ou un JS défectueux, passer manuellement par `/recovery`. Les ressources absentes renvoient 404.

Les transferts utilisent un fichier temporaire puis un remplacement avec sauvegarde de l’ancien fichier. Un lot complet n’est pas atomique : ne pas couper le courant pendant la mise à jour. Les noms des sons, le volume et l’urne ne sont pas modifiés. Aucun accès HTTP général à la carte SD n’est ouvert.

Nouvelles API : GET `/api/ui/files`, POST `/api/ui/upload` (corps brut, en-têtes `X-Manage: 1` et `X-File-Name` encodé par `encodeURIComponent`), POST `/api/ui/delete` (mêmes en-têtes). `/api/ui/reset` restaure la page embarquée. Reflasher une fois pour activer ces routes. Ensuite, les fichiers du dossier Web se mettent à jour sans reflash.

Le dossier **`www` fourni dans le projet est prêt à copier à la racine de la SD** : index.html, style.css, app.js et icon-192.png. On peut aussi téléverser ces quatre fichiers depuis `/recovery`. La page de secours reste autonome. Après modification de sa source `Disco_ESP/data/index.html`, lancer `python3 tools/export_www.py` pour régénérer la version séparée pour la SD. Les icônes d’écran d’accueil et le manifeste restent servis par le firmware.

## Identifiants Wi-Fi depuis la carte SD

Au démarrage, les identifiants mémorisés en NVS sont utilisés en priorité sur `WIFI_SSID` et `WIFI_PASSWORD` du fichier `config.local.h`. En l’absence de valeur mémorisée, les paramètres compilés sont utilisés. Après la fin du morceau de démarrage (ou son arrêt/échec), le programme recherche `/wifi.txt` à la racine de la SD, puis importe et mémorise les identifiants valides. Aucune recherche de ce fichier ni écriture de ces identifiants en NVS n’a lieu pendant la lecture. Le mode WIFI_FAST garde son fonctionnement habituel.

Copier `wifi.example.txt`, le renommer en `wifi.txt` et renseigner :

```text
ssid=MonReseau
password=MonMotDePasse
```

Texte UTF-8, fins de lignes Windows ou Unix acceptées. Les clés `ssid` et `password` sont obligatoires, en minuscules, une seule fois chacune. Ne pas mettre de guillemets ni d’espaces autour du signe `=` : les espaces dans les valeurs sont conservés. Les lignes vides et les commentaires commençant par `#` sont acceptés. SSID : 1 à 32 octets UTF-8 ; mot de passe : 8 à 63 octets, ou 64 caractères hexadécimaux ; `password=` pour un réseau ouvert. Maximum 512 octets pour le fichier. Le chemin est réglable avec `WIFI_SD_FILE` (défaut `/sdcard/wifi.txt`).

Après un import réussi, le fichier peut être retiré : les identifiants restent mémorisés après coupure du courant, même sans SD. S’il reste présent, aucune écriture NVS supplémentaire n’est effectuée tant que le contenu utile est identique. Un fichier mal formé ou un échec de mémorisation conserve les anciens paramètres. L’import valide le format, pas la connexion au réseau : un mot de passe erroné mais bien formé est mémorisé ; le corriger avec un nouveau `wifi.txt` puis redémarrer.

Le port série annonce l’import ou son échec sans afficher le mot de passe. Le fichier SD contient les identifiants en clair : le retirer après import si souhaité. `wifi.txt` est exclu de Git ; seul le modèle sans identifiants personnels est fourni. Effacer complètement la mémoire flash efface aussi les identifiants mémorisés. Modifier seulement le fichier `.h` ne remplace pas une configuration déjà mémorisée : utiliser `wifi.txt` pour cela.

### Import Wi-Fi différé

Avec `WIFI_FAST=1`, la connexion pendant le morceau utilise les identifiants déjà mémorisés ou compilés. Si une tentative a été lancée, les identifiants importés après le morceau seront utilisés au prochain redémarrage, sans couper la connexion actuelle. Avec `WIFI_FAST=0`, ou si aucun SSID n’était configuré, l’import a lieu avant la première tentative et les nouveaux identifiants sont utilisés immédiatement. Sans morceau lisible, l’import est effectué sans attendre. Une coupure avant la fin du morceau empêche cet import : laisser terminer une fois le morceau pour mémoriser le nouveau réseau.

### Choisir le premier morceau de l’urne

Dans la liste des fichiers, le bouton flèche vers le haut devant chaque morceau régénère toute l’urne, place ce morceau en premier et mélange les autres sans doublon. Il active et mémorise le mode Urne. Aucun son n’est lancé : le morceau sera joué au prochain allumage. Arrêter la lecture avant cette action. Cette fonction nécessite le firmware mis à jour ; remplacer uniquement les fichiers Web ne suffit pas.

### Playlists par dossier

Créez des dossiers directement à la racine de la carte SD, par exemple `Quotidien/`, `Soiree/` et `Weekend/`, puis placez les WAV dedans. Les fichiers à la racine restent utilisables dans « Racine de la carte SD ». Le dossier `www` est réservé à l’interface. Les sous-dossiers imbriqués ne sont pas parcourus. Noms de dossiers : moins de 128 octets UTF-8, sans séparateurs de chemin ni caractères réservés.

Dans « Mode de fonctionnement », arrêtez la lecture puis choisissez la playlist active. Ce choix est mémorisé et s’applique au prochain allumage ; il ne lance aucun son. Chaque dossier possède une urne persistante indépendante : changer de playlist puis revenir conserve la progression. Le réglage Random / Urne reste commun. L’ajout, suppression ou renommage de WAV régénère uniquement l’urne du dossier actif. Les téléversements et écoutes manuelles concernent aussi ce dossier.

Le bouton ↑ régénère l’urne de la playlist active avec le morceau choisi en premier. Les dossiers peuvent être créés, renommés et supprimés depuis l’interface Web. Si un dossier actif est retiré ou renommé hors ligne, la lecture échoue : choisissez un dossier existant dans l’interface. Une playlist vide ne produit aucun son.

Cette évolution nécessite de reflasher le firmware et de mettre à jour les fichiers `www` sur la carte SD.

### Gérer les playlists depuis le Web

À l’arrêt, utilisez Créer, Renommer ou Supprimer sous le sélecteur de playlist. La racine ne peut pas être renommée ou supprimée. Renommer conserve l’ordre et le curseur de l’urne ; supprimer demande confirmation et efface les WAV du dossier, puis le dossier. Le lecteur revient à la racine. La suppression est refusée si le dossier contient des sous-dossiers ou des fichiers autres que des WAV.

Le bouton Déplacer de chaque morceau ouvre un sélecteur de destination. Aucun fichier existant n’est écrasé. Les urnes source et destination sont régénérées. Aucune de ces actions ne lance de son.

Les changements sur la SD et la mémoire interne ne forment pas une transaction unique : évitez de couper l’alimentation pendant une modification. Une suppression interrompue peut laisser un dossier partiellement vidé. Après une erreur, actualisez et vérifiez le contenu.

### Démarrage accéléré : catalogue des WAV

Chaque playlist possède un fichier technique `.disco-catalog.bin`, créé automatiquement lors du premier parcours complet. Aux démarrages suivants, le lecteur charge et contrôle la structure de cet index, sans parcourir le dossier ni vérifier les métadonnées de tous les fichiers. Le morceau sélectionné est ouvert normalement pour la lecture. Le tirage reste sauvegardé avant le son.

Un index absent, tronqué, incohérent ou ne correspondant plus au dossier déclenche un parcours complet. Les modifications Web reconstruisent le catalogue concerné. Les modifications effectuées depuis un ordinateur sont prises en compte lors de la vérification complète différée après la lecture. Un remplacement conservant exactement nom, taille et date ne peut pas être détecté par ces métadonnées : régénérez alors l’urne depuis le Web ou supprimez `.disco-catalog.bin`. Les WAV incompatibles peuvent entraîner un parcours complet à chaque démarrage ; retirez-les pour bénéficier du cache.

Le cache est facultatif : si son écriture échoue, la sélection reste possible avec un parcours complet. Les fichiers `.disco-catalog.bin` et `.disco-catalog.tmp` sont gérés automatiquement et supprimés avec leur playlist. Le premier démarrage reste plus long. Les logs indiquent les temps du montage SD et de la sélection/sauvegarde de l’urne, en microsecondes ; aucun gain chiffré n’est garanti sans mesure sur la carte.

La liste des WAV affiche la taille et la durée (minutes:secondes). Les durées sont calculées lors du parcours des WAV puis stockées dans le catalogue : afficher la liste n’ouvre pas à nouveau tous les fichiers audio pendant la lecture. Une durée inconnue ou un WAV incompatible apparaît avec « — ». Le nouveau format de catalogue est reconstruit automatiquement une fois après cette mise à jour.

### Démarrage rapide et vérification différée

Avant le son, le lecteur charge le catalogue, tire un morceau, valide uniquement son en-tête WAV puis sauvegarde le tirage en NVS. Un catalogue manquant, corrompu ou vide déclenche un parcours complet. Si le morceau choisi manque ou est incompatible, le catalogue est reconstruit et un nouveau tirage est tenté une fois, sans consommer le tirage invalide.

La vérification complète s’exécute après la lecture (ou STOP), dans la tâche du serveur Web, lorsqu’aucune lecture n’est active. Elle conserve la progression de l’urne si la liste des morceaux n’a pas changé ; sinon elle la reconstruit. Elle nécessite que le serveur Wi-Fi soit démarré. Si une autre lecture a déjà commencé, elle est reportée ; un prochain contrôle de connexion de la page la déclenche à l’arrêt. Les modifications Web continuent de mettre à jour les catalogues immédiatement. Une coupure avant la vérification différée peut donc reporter la détection des ajouts faits depuis un ordinateur au prochain cycle complet.

Les logs BOOT distinguent désormais : catalogue (cache ou reconstruction), chargement de l’urne et tirage, vérification du WAV choisi, sauvegarde NVS avant audio, éventuelle reconstruction de secours, et vérification complète après lecture. Le montage SD conserve son chronométrage séparé. Les temps sont en microsecondes.
