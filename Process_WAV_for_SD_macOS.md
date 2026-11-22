# Script de préparation macOS

Le script [Process_WAV_for_SD_macOS.command](Process_WAV_for_SD_macOS.command) fonctionne sous **macOS**, avec **FFmpeg** disponible dans le Terminal. Il traite les fichiers `.wav` et `.WAV` placés directement à côté de lui, sans parcourir les sous-dossiers. Il ne modifie pas le programme de l’ESP32 et ne découpe pas les extraits.

Avec Homebrew déjà installé, FFmpeg s’installe avec la commande suivante :

```sh
brew install ffmpeg
```

Le script se place dans un dossier de travail contenant des copies des WAV à traiter. Son autorisation d’exécution et son lancement s’effectuent depuis le Terminal :

```sh
chmod +x Process_WAV_for_SD_macOS.command
./Process_WAV_for_SD_macOS.command
```

Le double-clic constitue une autre possibilité lorsque macOS autorise son exécution. Le script travaille toujours dans son propre dossier, indépendamment du dossier courant du Terminal.

## Fichiers produits

Pour chaque source, le script nettoie le nom, puis prépare quatre versions dans `sounds/` :

| Dossier local | Contenu | Usage |
| --- | --- | --- |
| `sounds/original/` | WAV PCM16, 44,1 kHz, stéréo, non normalisé, sans métadonnées | Version convertie de référence |
| `sounds/norm_stereo/` | WAV PCM16, 44,1 kHz, stéréo, normalisé | Lecture stéréo avec des niveaux plus homogènes |
| `sounds/norm_mono/` | WAV PCM16, 44,1 kHz, mono, normalisé | Version copiée automatiquement sur la carte SD |
| `sounds/mp3/` | MP3 128 kbit/s issu du WAV stéréo normalisé | Écoute sur ordinateur ; non lu par Disco |

Le dossier `original` contient une **conversion**, pas une sauvegarde à l’identique de la source : la fréquence, les canaux, la résolution et les métadonnées peuvent avoir changé.

Les noms sont nettoyés pour limiter les problèmes de compatibilité : translittération des accents lorsque `iconv` le permet, remplacement des caractères réservés et des apostrophes par `_`, suppression des caractères de contrôle et des espaces ou points aux extrémités. Le script traite aussi certains noms réservés et tronque le nom de base à 250 caractères. Cette limite en caractères ne garantit pas la limite en octets du serveur pour tous les noms UTF-8.

## Normalisation sonore

Les versions normalisées utilisent le filtre FFmpeg `loudnorm` en deux passes : une analyse du niveau, puis une conversion à partir des mesures. Le traitement vise à réduire les écarts de volume perçu entre les morceaux ; il ne s’agit pas d’une égalisation des graves et des aigus.

Les trois valeurs se trouvent au début du script :

| Réglage | Valeur | Signification |
| --- | --- | --- |
| `CIBLE_LUFS` | `-14` | Niveau sonore intégré visé |
| `CIBLE_TP` | `-1.0` | Plafond de crête vraie visé, en dBTP |
| `CIBLE_LRA` | `11` | Paramètre de plage dynamique visée, en LU |

Le mono et la stéréo font l’objet de traitements distincts. Le script demande `linear=true` ; FFmpeg peut revenir à une normalisation dynamique lorsque les conditions du mode linéaire ne sont pas réunies. Les valeurs sont des cibles de traitement, pas une mesure de conformité réalisée sur chaque fichier final. La documentation du [filtre loudnorm](https://ffmpeg.org/ffmpeg-filters.html#loudnorm) détaille ces paramètres.

## Devenir des sources et reprise d’un traitement

**Après la création réussie du MP3, le script retire le WAV source du dossier de travail** : il appelle `trash` si cette commande est disponible, sinon il utilise `rm -f`, qui supprime le fichier sans passage par la corbeille. Une copie indépendante des sources préserve les fichiers d’origine. La génération des autres versions a lieu avant cette étape ; les sorties déjà présentes peuvent toutefois provenir d’un traitement antérieur.

Une sortie existante n’est pas écrasée : son étape de conversion est ignorée. Le script ne compare pas son contenu à la source et ne vérifie pas qu’elle utilise les réglages actuels. Une modification des cibles de normalisation nécessite donc de nouveaux noms ou le retrait préalable des anciennes sorties concernées. Deux sources qui aboutissent au même nom nettoyé ne sont pas automatiquement différenciées. Un échec de conversion peut laisser une sortie partielle, qui sera ensuite considérée comme existante.

## Copie sur la carte SD

Après les conversions, le script recherche les dossiers de volumes dans `/Volumes`. Un seul volume détecté est présélectionné ; plusieurs volumes donnent lieu à un choix numéroté. Une confirmation par la touche Entrée précède la copie.

Il copie **le contenu de `sounds/norm_mono/` à la racine du volume choisi**, sans écraser les fichiers existants. Tous les WAV présents dans ce dossier sont candidats à la copie, y compris ceux issus de traitements précédents. Le script ne crée pas de playlist par dossier sur la SD. Pour une playlist dédiée, les WAV se copient manuellement dans son dossier ou se déplacent ensuite depuis l’interface Web.

Le script ne formate pas la carte, ne vérifie pas le FAT32, ne contrôle pas la limite de 128 Mio et ne certifie pas que le volume choisi est réellement une carte SD. La destination et son format se vérifient avant la confirmation. Les versions stéréo se copient manuellement depuis `sounds/norm_stereo/` lorsque la stéréo est souhaitée.

## Nettoyage et éjection

Le script exécute un nettoyage dans `sounds/`, ses quatre dossiers de sortie et la racine du volume cible. Il appelle `dot_clean` lorsque disponible, supprime les fichiers cachés directement présents dans ces emplacements et certains dossiers système macOS. **Ce nettoyage ne se limite pas aux fichiers AppleDouble** : il peut notamment supprimer le cache `.disco-catalog.bin` de la playlist racine, qui sera reconstruit par Disco. Les sous-dossiers de playlists de la SD ne sont pas nettoyés récursivement par la boucle de suppression des fichiers cachés.

Après copie, `sync` synchronise les écritures et `diskutil unmount` tente de démonter le volume. La carte se retire après confirmation du démontage, ou après une éjection manuelle réussie si le démontage a échoué. Les dossiers locaux `sounds/` ne sont pas destinés à être copiés en bloc sur la carte.