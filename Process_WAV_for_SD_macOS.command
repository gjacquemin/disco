#!/bin/bash
cd "$(dirname "$0")"

# Active nullglob (évite une erreur si aucun .wav) et nocaseglob (.wav ou .WAV)
shopt -s nullglob nocaseglob

# Création des dossiers de destination locaux
mkdir -p "SD" "MP3"

# Fonction de nettoyage des noms pour compatibilité FAT32 stricte
nettoyer_nom_fat32() {
    local nom="$1"
    local nom_temp=""
    
    # 1. Suppression des accents via iconv de manière sécurisée
    if command -v iconv >/dev/null 2>&1; then
        nom_temp=$(printf '%s' "$nom" | iconv -f UTF-8 -t ASCII//TRANSLIT 2>/dev/null)
        if [ -n "$nom_temp" ]; then
            nom="$nom_temp"
        fi
    fi

    # 2. Remplacement des caractères interdits en FAT32 (\ / : * ? " < > |) et apostrophes par (_)
    nom=$(printf '%s' "$nom" | sed -E 's/[\\\/:\*\?"<>\|'\'']/_/g')

    # 3. Suppression des caractères de contrôle ASCII
    nom=$(printf '%s' "$nom" | tr -d '[:cntrl:]')

    # 4. Suppression des espaces et points en début et fin de nom
    nom=$(printf '%s' "$nom" | sed -E 's/^[ \.]+|[ \.]+$//g')

    # 5. Vérification des noms réservés MS-DOS/Windows
    if [[ "$nom" =~ ^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$ ]]; then
        nom="${nom}_file"
    fi

    # 6. Sécurité si le nom devient vide
    if [ -z "$nom" ]; then
        nom="audio_sans_nom"
    fi

    # 7. Troncature à 250 caractères maximum
    nom="${nom:0:250}"

    printf '%s' "$nom"
}

# Fonction réutilisable de nettoyage des fichiers cachés macOS dans un dossier cible
nettoyer_caches_macos() {
    local cible="$1"
    [ -d "$cible" ] || return

    # Fusionne/supprime les fichiers AppleDouble (._*) avec l'outil natif macOS
    if command -v dot_clean >/dev/null 2>&1; then
        dot_clean -m "$cible" 2>/dev/null
    fi

    shopt -s dotglob
    for cache in "$cible"/.*; do
        local base
        base=$(basename "$cache")
        if [[ "$base" == "." || "$base" == ".." ]]; then
            continue
        fi
        
        if [ -f "$cache" ]; then
            rm -f "$cache"
            echo "  -> Fichier caché supprimé : $cache"
        elif [ -d "$cache" ] && [[ "$base" == ".Trashes" || "$base" == ".Spotlight-V100" || "$base" == ".fseventsd" || "$base" == ".TemporaryItems" ]]; then
            rm -rf "$cache"
            echo "  -> Dossier système macOS supprimé : $cache"
        fi
    done
    shopt -u dotglob
}

# ==============================================================================
# 1. CONVERSION AUDIO ET NETTOYAGE LOCAL
# ==============================================================================
for fichier in *.wav; do
    [ -f "$fichier" ] || continue
    [[ "$fichier" == .* ]] && continue

    nom_brut="${fichier%.*}"
    nom_propre=$(nettoyer_nom_fat32 "$nom_brut")

    fichier_sd="SD/${nom_propre}.wav"
    fichier_mp3="MP3/${nom_propre}.mp3"

    echo "Traitement de : $fichier"
    if [ "$nom_brut" != "$nom_propre" ]; then
        echo "  -> Nom nettoyé pour FAT32 : ${nom_propre}"
    fi

    # Conversion en WAV 16 bits, 44.1 kHz, stéréo sans métadonnées
    if [ -e "$fichier_sd" ]; then
        echo "ERREUR : Le fichier '$fichier_sd' existe déjà. Conversion WAV ignorée." >&2
    else
        if ffmpeg -i "$fichier" -map_metadata -1 -vn -ar 44100 -ac 2 -c:a pcm_s16le -fflags +bitexact -flags:a +bitexact "$fichier_sd" -loglevel error; then
            echo "  -> WAV converti et nettoyé (sans tags) avec succès dans SD/"
        else
            echo "ERREUR : Échec de la conversion WAV pour '$fichier'." >&2
            continue
        fi
    fi

    # Conversion du fichier SD/ en MP3 128 kbits
    if [ ! -e "$fichier_sd" ]; then
        echo "ERREUR : Le fichier source '$fichier_sd' est introuvable pour la conversion MP3." >&2
        continue
    fi

    if [ -e "$fichier_mp3" ]; then
        echo "ERREUR : Le fichier '$fichier_mp3' existe déjà. Conversion MP3 ignorée." >&2
    else
        if ffmpeg -i "$fichier_sd" -map_metadata -1 -b:a 128k "$fichier_mp3" -loglevel error; then
            echo "  -> MP3 converti avec succès dans MP3/"
            rm "$fichier"
            echo "  -> Fichier original '$fichier' supprimé."
        else
            echo "ERREUR : Échec de la conversion MP3 pour '$fichier_sd'. L'original n'a pas été supprimé." >&2
        fi
    fi
done

echo ""
echo "Nettoyage des fichiers cachés macOS dans les dossiers locaux..."
nettoyer_caches_macos "."
nettoyer_caches_macos "SD"
nettoyer_caches_macos "MP3"

# ==============================================================================
# 2. DÉTECTION DE LA CARTE SD ET COPIE SANS ÉCRASEMENT (MACOS)
# ==============================================================================
echo ""
echo "Recherche d'une carte SD ou d'un volume externe dans /Volumes..."

volumes_externes=()
for vol in /Volumes/*; do
    # Ignorer le disque système Macintosh HD (lien symbolique vers /)
    if [ -d "$vol" ] && [ ! -L "$vol" ]; then
        volumes_externes+=("$vol")
    fi
done

carte_sd=""

if [ ${#volumes_externes[@]} -eq 0 ]; then
    echo "Aucune carte SD ou volume externe détecté dans /Volumes."
elif [ ${#volumes_externes[@]} -eq 1 ]; then
    carte_sd="${volumes_externes}"
    echo "Carte SD / Volume détecté : $carte_sd"
else
    echo "Plusieurs volumes externes détectés :"
    for i in "${!volumes_externes[@]}"; do
        echo "  [$((i+1))] ${volumes_externes[$i]}"
    done
    read -r -p "Entrez le numéro de la carte SD cible (ou 0 pour annuler la copie) : " choix
    if [[ "$choix" =~ ^[0-9]+$ ]] && [ "$choix" -ge 1 ] && [ "$choix" -le ${#volumes_externes[@]} ]; then
        carte_sd="${volumes_externes[$((choix-1))]}"
    else
        echo "Copie vers la carte SD annulée."
        carte_sd=""
    fi
fi

# Si un volume a été identifié, demande de confirmation avant copie
if [ -n "$carte_sd" ]; then
    echo ""
    read -r -p "Appuyez sur [Entrée] pour copier le contenu de 'SD/' vers '$carte_sd' (ou Ctrl+C pour annuler)..."

    fichiers_a_copier=(SD/*.wav)
    if [ ${#fichiers_a_copier[@]} -eq 0 ]; then
        echo "Aucun fichier WAV à copier dans le dossier 'SD/'."
    else
        echo "Copie des fichiers WAV vers $carte_sd (sans écrasement)..."
        for fichier_sd in "${fichiers_a_copier[@]}"; do
            [ -f "$fichier_sd" ] || continue
            nom_fichier=$(basename "$fichier_sd")
            destination="$carte_sd/$nom_fichier"

            # Vérification de l'existence sur la carte SD avant toute copie
            if [ -e "$destination" ]; then
                echo "ERREUR : Le fichier '$nom_fichier' existe déjà sur la carte SD. Copie ignorée." >&2
            else
                if cp -X "$fichier_sd" "$destination"; then
                    echo "  -> Copié : $nom_fichier"
                else
                    echo "ERREUR : Échec de la copie de '$nom_fichier' vers la carte SD." >&2
                fi
            fi
        done
        
        echo "Nettoyage des fichiers cachés macOS sur la carte SD..."
        nettoyer_caches_macos "$carte_sd"
        
        # Synchronisation de l'écriture sur le disque avant éjection
        sync
        
        echo "Démontage de la carte SD ($carte_sd)..."
        if diskutil unmount "$carte_sd"; then
            echo "La carte SD a été démontée en toute sécurité. Vous pouvez la retirer."
        else
            echo "ERREUR : Impossible de démonter '$carte_sd' (vérifiez qu'aucune fenêtre Finder ne l'utilise)." >&2
        fi
    fi
fi

# ==============================================================================
# 3. PAUSE FINALE AVANT DE QUITTER
# ==============================================================================
echo ""
read -r -p "Traitement terminé. Appuyez sur [Entrée] pour quitter..."
