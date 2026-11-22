#!/bin/bash
cd "$(dirname "$0")"

# Active nullglob (évite une erreur si aucun .wav) et nocaseglob (.wav ou .WAV)
shopt -s nullglob nocaseglob

# Réglages de normalisation sonore (norme EBU R128)
CIBLE_LUFS="-14"   # Niveau cible en LUFS (ex: -14 pour standard, -12 pour plus fort)
CIBLE_TP="-1.0"    # Plafond True Peak en dBTP (évite la saturation)
CIBLE_LRA="11"     # Plage dynamique cible (Loudness Range)

# Création des dossiers de destination locaux dans sounds/
mkdir -p "sounds/original" "sounds/norm_stereo" "sounds/norm_mono" "sounds/mp3"

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

# Fonction réutilisable de normalisation LUFS double passe
normaliser_loudnorm() {
    local source="$1"
    local destination="$2"
    local canaux="$3" # 1 pour mono, 2 pour stéréo
    local label="$4"

    local stats in_i in_tp in_lra in_thresh offset
    stats=$(ffmpeg -i "$source" -vn -ac "$canaux" -af "loudnorm=I=${CIBLE_LUFS}:TP=${CIBLE_TP}:LRA=${CIBLE_LRA}:print_format=json" -f null - 2>&1 | awk '/\{/,/\}/')

    in_i=$(printf '%s' "$stats" | grep '"input_i"' | sed -E 's/.*"input_i" : "([^"]+)".*/\1/')
    in_tp=$(printf '%s' "$stats" | grep '"input_tp"' | sed -E 's/.*"input_tp" : "([^"]+)".*/\1/')
    in_lra=$(printf '%s' "$stats" | grep '"input_lra"' | sed -E 's/.*"input_lra" : "([^"]+)".*/\1/')
    in_thresh=$(printf '%s' "$stats" | grep '"input_thresh"' | sed -E 's/.*"input_thresh" : "([^"]+)".*/\1/')
    offset=$(printf '%s' "$stats" | grep '"target_offset"' | sed -E 's/.*"target_offset" : "([^"]+)".*/\1/')

    if [ -n "$in_i" ] && [ -n "$in_tp" ] && [ -n "$in_lra" ] && [ -n "$in_thresh" ] && [ -n "$offset" ]; then
        if ffmpeg -i "$source" -map_metadata -1 -vn -ar 44100 -ac "$canaux" \
            -af "loudnorm=I=${CIBLE_LUFS}:TP=${CIBLE_TP}:LRA=${CIBLE_LRA}:measured_I=${in_i}:measured_TP=${in_tp}:measured_LRA=${in_lra}:measured_thresh=${in_thresh}:offset=${offset}:linear=true" \
            -c:a pcm_s16le -fflags +bitexact -flags:a +bitexact "$destination" -loglevel error; then
            echo "  -> ${label} égalisé à ${CIBLE_LUFS} LUFS (mesuré : ${in_i} LUFS)"
            return 0
        else
            echo "ERREUR : Échec de la passe 2 loudnorm (${label}) pour '$source'." >&2
            return 1
        fi
    else
        echo "ERREUR : Impossible d'analyser le niveau LUFS (${label}) de '$source'." >&2
        return 1
    fi
}

# Fonction réutilisable de nettoyage des fichiers cachés macOS dans un dossier cible
nettoyer_caches_macos() {
    local cible="$1"
    [ -d "$cible" ] || return

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

    fichier_orig="sounds/original/${nom_propre}.wav"
    fichier_norm_stereo="sounds/norm_stereo/${nom_propre}.wav"
    fichier_norm_mono="sounds/norm_mono/${nom_propre}.wav"
    fichier_mp3="sounds/mp3/${nom_propre}.mp3"

    echo "Traitement de : $fichier"
    if [ "$nom_brut" != "$nom_propre" ]; then
        echo "  -> Nom nettoyé pour FAT32 : ${nom_propre}"
    fi

    # 1A. Conversion en WAV stéréo original (non normalisé, sans métadonnées)
    if [ -e "$fichier_orig" ]; then
        echo "ERREUR : Le fichier '$fichier_orig' existe déjà. Conversion ignorée." >&2
    else
        if ffmpeg -i "$fichier" -map_metadata -1 -vn -ar 44100 -ac 2 -c:a pcm_s16le -fflags +bitexact -flags:a +bitexact "$fichier_orig" -loglevel error; then
            echo "  -> WAV original (stéréo non normalisé) enregistré dans sounds/original/"
        else
            echo "ERREUR : Échec de la conversion WAV original pour '$fichier'." >&2
            continue
        fi
    fi

    # 1B. Conversion en WAV stéréo normalisé LUFS
    if [ -e "$fichier_norm_stereo" ]; then
        echo "ERREUR : Le fichier '$fichier_norm_stereo' existe déjà. Conversion norm_stereo ignorée." >&2
    else
        normaliser_loudnorm "$fichier" "$fichier_norm_stereo" 2 "WAV stéréo (sounds/norm_stereo/)" || continue
    fi

    # 1C. Conversion en WAV mono normalisé LUFS
    if [ -e "$fichier_norm_mono" ]; then
        echo "ERREUR : Le fichier '$fichier_norm_mono' existe déjà. Conversion norm_mono ignorée." >&2
    else
        normaliser_loudnorm "$fichier" "$fichier_norm_mono" 1 "WAV mono (sounds/norm_mono/)" || continue
    fi

    # 1D. Conversion en MP3 128 kbits depuis le fichier stéréo normalisé
    if [ ! -e "$fichier_norm_stereo" ]; then
        echo "ERREUR : Le fichier source '$fichier_norm_stereo' est introuvable pour la conversion MP3." >&2
        continue
    fi

    if [ -e "$fichier_mp3" ]; then
        echo "ERREUR : Le fichier '$fichier_mp3' existe déjà. Conversion MP3 ignorée." >&2
    else
        if ffmpeg -i "$fichier_norm_stereo" -map_metadata -1 -b:a 128k "$fichier_mp3" -loglevel error; then
            echo "  -> MP3 converti avec succès dans sounds/mp3/"
            if command -v trash >/dev/null 2>&1; then
                trash "$fichier"
                echo "  -> Fichier source '$fichier' envoyé à la corbeille."
            else
                rm -f "$fichier"
                echo "  -> Fichier source '$fichier' supprimé (commande 'trash' introuvable)."
            fi
        else
            echo "ERREUR : Échec de la conversion MP3 pour '$fichier_norm_stereo'. Le fichier source n'a pas été supprimé." >&2
        fi
    fi
done

echo ""
echo "Nettoyage des fichiers cachés macOS dans les dossiers locaux..."
nettoyer_caches_macos "sounds"
nettoyer_caches_macos "sounds/original"
nettoyer_caches_macos "sounds/norm_stereo"
nettoyer_caches_macos "sounds/norm_mono"
nettoyer_caches_macos "sounds/mp3"

# ==============================================================================
# 2. DÉTECTION DE LA CARTE SD ET COPIE SANS ÉCRASEMENT (MACOS)
# ==============================================================================
echo ""
echo "Recherche d'une carte SD ou d'un volume externe dans /Volumes..."

volumes_externes=()
for vol in /Volumes/*; do
    if [ -d "$vol" ] && [ ! -L "$vol" ]; then
        volumes_externes+=("$vol")
    fi
done

carte_sd=""

if [ ${#volumes_externes[@]} -eq 0 ]; then
    echo "Aucune carte SD ou volume externe détecté dans /Volumes."
elif [ ${#volumes_externes[@]} -eq 1 ]; then
    carte_sd="${volumes_externes[0]}"
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

if [ -n "$carte_sd" ]; then
    echo ""
    read -r -p "Appuyez sur [Entrée] pour copier le contenu de 'sounds/norm_mono/' vers '$carte_sd' (ou Ctrl+C pour annuler)..."

    fichiers_a_copier=(sounds/norm_mono/*.wav)
    if [ ${#fichiers_a_copier[@]} -eq 0 ]; then
        echo "Aucun fichier WAV à copier dans le dossier 'sounds/norm_mono/'."
    else
        echo "Copie des fichiers WAV mono normalisés vers $carte_sd (sans écrasement)..."
        for fichier_sd in "${fichiers_a_copier[@]}"; do
            [ -f "$fichier_sd" ] || continue
            nom_fichier=$(basename "$fichier_sd")
            destination="$carte_sd/$nom_fichier"

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