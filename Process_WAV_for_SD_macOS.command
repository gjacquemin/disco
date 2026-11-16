#!/bin/bash
# Disco — préparation WAV et copie sur microSD, macOS / Bash 3.2.
# Originaux conservés. Aucun formatage ni nettoyage général des fichiers cachés.
shopt -s nullglob nocaseglob
export COPYFILE_DISABLE=1
MAX_WAV_BYTES=$((128 * 1024 * 1024))
TMP_WAV=""; TMP_MP3=""; TMP_COPY=""; INFO=""
cleanup() {
    local p
    for p in "$TMP_WAV" "$TMP_MP3" "$TMP_COPY" "$INFO"; do
        [ -z "$p" ] || rm -f -- "$p"
    done
}
trap cleanup EXIT
trap 'exit 130' INT TERM
fail() { printf 'ERREUR : %s\n' "$*" >&2; exit 1; }
pause_fin() { if [ -t 0 ]; then read -r -p "Appuyez sur Entrée pour quitter… " _ || true; fi; }
[ "$(uname -s)" = "Darwin" ] || fail "Ce script fonctionne uniquement sous macOS."
[ "$EUID" -ne 0 ] || fail "Lancez ce script sans sudo."
for outil in ffmpeg ffprobe diskutil iconv; do
    command -v "$outil" >/dev/null 2>&1 || fail "$outil est introuvable. Pour FFmpeg : brew install ffmpeg (si Homebrew est installé)."
done
[ -x /usr/libexec/PlistBuddy ] || fail "PlistBuddy est indisponible."
cd "$(dirname "$0")" || fail "Impossible d'ouvrir le dossier du script."
mkdir -p SD MP3 || fail "Impossible de créer SD/ et MP3/."

nettoyer_nom() {
    local nom="$1" ascii="" maj
    # iconv peut ne pas translittérer tous les caractères selon macOS.
    ascii=$(printf '%s' "$nom" | iconv -f UTF-8 -t ASCII//TRANSLIT 2>/dev/null) || ascii=""
    [ -z "$ascii" ] || nom="$ascii"
    nom=$(printf '%s' "$nom" | LC_ALL=C tr -c ' -~' '_' | sed -E 's/[\\/:*?"<>|]/_/g; s/^[ .]+//; s/[ .]+$//')
    nom=${nom:0:120}
    nom=$(printf '%s' "$nom" | sed -E 's/[ .]+$//')
    [ -n "$nom" ] || nom="audio_sans_nom"
    maj=$(printf '%s' "$nom" | LC_ALL=C tr '[:lower:]' '[:upper:]')
    case "${maj%%.*}" in CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9]) nom="${nom}_file";; esac
    printf '%s' "$nom"
}
# Évite aussi les doublons de casse sur un disque de travail sensible à la casse.
existe_nom() {
    local dossier="$1" cherche candidat base
    cherche=$(printf '%s' "$2" | LC_ALL=C tr '[:upper:]' '[:lower:]')
    for candidat in "$dossier"/*; do
        base=$(printf '%s' "${candidat##*/}" | LC_ALL=C tr '[:upper:]' '[:lower:]')
        [ "$base" != "$cherche" ] || return 0
    done
    return 1
}

vus=(); convertis=0; ignores=0
for fichier in ./*.wav; do
    [ -f "$fichier" ] || continue
    base=${fichier##*/}; nom=$(nettoyer_nom "${base%.*}")
    cle=$(printf '%s' "$nom" | LC_ALL=C tr '[:upper:]' '[:lower:]')
    collision=0
    for deja in "${vus[@]}"; do [ "$deja" != "$cle" ] || collision=1; done
    if [ "$collision" -eq 1 ]; then
        printf 'IGNORÉ : collision de nom après nettoyage pour %s\n' "$base" >&2
        ignores=$((ignores+1)); continue
    fi
    vus+=("$cle")
    if existe_nom SD "$nom.wav"; then
        printf 'IGNORÉ : SD/%s.wav existe déjà ; aucun original supprimé.\n' "$nom"
        ignores=$((ignores+1)); continue
    fi
    printf 'Conversion : %s → SD/%s.wav\n' "$base" "$nom"
    TMP_WAV=$(mktemp "$PWD/SD/.disco-wav.XXXXXX") || fail "Création du temporaire impossible."
    if ! ffmpeg -nostdin -hide_banner -loglevel error -y -i "$fichier" -map 0:a:0 -map_metadata -1 -vn \
        -ar 44100 -ac 2 -c:a pcm_s16le -fflags +bitexact -flags:a +bitexact -f wav "$TMP_WAV"; then
        printf 'ERREUR : conversion échouée pour %s ; original conservé.\n' "$base" >&2
        rm -f -- "$TMP_WAV"; TMP_WAV=""; ignores=$((ignores+1)); continue
    fi
    taille=$(stat -f '%z' "$TMP_WAV") || fail "Taille du WAV illisible."
    if [ "$taille" -gt "$MAX_WAV_BYTES" ]; then
        printf 'IGNORÉ : %s dépasse 128 Mio après conversion.\n' "$base" >&2
        rm -f -- "$TMP_WAV"; TMP_WAV=""; ignores=$((ignores+1)); continue
    fi
    if ! mv -n "$TMP_WAV" "SD/$nom.wav" || [ -e "$TMP_WAV" ]; then
        printf 'ERREUR : destination existante ou déplacement impossible.\n' >&2
        rm -f -- "$TMP_WAV"; TMP_WAV=""; continue
    fi
    TMP_WAV=""; convertis=$((convertis+1))
    if ! existe_nom MP3 "$nom.mp3"; then
        TMP_MP3=$(mktemp "$PWD/MP3/.disco-mp3.XXXXXX") || fail "Création du temporaire MP3 impossible."
        if ffmpeg -nostdin -hide_banner -loglevel error -y -i "SD/$nom.wav" -map_metadata -1 -vn -c:a libmp3lame -b:a 128k -f mp3 "$TMP_MP3"; then
            mv -n "$TMP_MP3" "MP3/$nom.mp3" || printf 'ERREUR : déplacement MP3 impossible.\n' >&2
        else
            printf 'ERREUR : MP3 non créé pour %s ; le WAV et l’original restent disponibles.\n' "$nom" >&2
        fi
        [ ! -e "$TMP_MP3" ] || rm -f -- "$TMP_MP3"
        TMP_MP3=""
    fi
done
printf '\n%s WAV converti(s), %s ignoré(s). Les originaux sont conservés.\n' "$convertis" "$ignores"
fichiers=(SD/*.wav)
if [ "${#fichiers[@]}" -eq 0 ]; then printf 'Aucun WAV à copier.\n'; pause_fin; exit 0; fi

volumes=()
for vol in /Volumes/*; do
    [ ! -d "$vol" ] || [ -L "$vol" ] || volumes+=("$vol")
done
if [ "${#volumes[@]}" -eq 0 ]; then printf 'Aucun volume monté ; conversions disponibles dans SD/.\n'; pause_fin; exit 0; fi
printf '\nVolumes montés (le support choisi sera vérifié avant copie) :\n'
for i in "${!volumes[@]}"; do printf '  [%s] %s\n' "$((i+1))" "${volumes[$i]}"; done
read -r -p "Numéro de la carte cible, ou 0 pour conserver seulement les conversions : " choix || choix=0
if ! [[ "$choix" =~ ^[1-9][0-9]*$ ]] || [ "${#choix}" -gt 4 ] || [ "$choix" -gt "${#volumes[@]}" ]; then
    printf 'Copie annulée.\n'; pause_fin; exit 0
fi
carte="${volumes[$((choix-1))]}"
INFO=$(mktemp "${TMPDIR:-/tmp}/disco-volume.XXXXXX") || fail "Temporaire volume indisponible."
diskutil info -plist "$carte" > "$INFO" || fail "Volume non reconnu par diskutil."
plist() { /usr/libexec/PlistBuddy -c "Print :$1" "$INFO" 2>/dev/null || true; }
if [ "$(plist Internal)" != "false" ] && [ "$(plist RemovableMedia)" != "true" ] && [ "$(plist Removable)" != "true" ]; then
    fail "Le support doit être externe ou amovible (par exemple une carte SD)."
fi
[ "$(plist MountPoint)" = "$carte" ] || fail "Le point de montage ne correspond pas au volume choisi."
[ "$(plist FilesystemType)" = "msdos" ] || fail "FAT32 requis ; aucune conversion ou aucun formatage du volume ne sera tenté."
fsname="$(plist FilesystemName) $(plist FileSystemPersonality)"
case "$fsname" in *FAT32*) ;; *) fail "Le volume n'est pas identifié comme FAT32.";; esac
[ -w "$carte" ] && [ "$(plist ReadOnlyVolume)" != "true" ] && [ "$(plist ReadOnlyMedia)" != "true" ] || fail "Le volume est en lecture seule."
printf '\nCible : %s\nCopie des WAV à la racine, sans écrasement ni formatage.\n' "$carte"
read -r -p "Taper COPIER pour confirmer : " accord || accord=""
[ "$accord" = "COPIER" ] || { printf 'Copie annulée.\n'; pause_fin; exit 0; }

copies=0; echecs=0
for fichier in "${fichiers[@]}"; do
    [ -f "$fichier" ] || continue
    nom=${fichier##*/}
    if existe_nom "$carte" "$nom"; then printf 'IGNORÉ sur la carte : %s existe déjà.\n' "$nom"; continue; fi
    # Revalide les WAV existants dans SD/, pas seulement ceux convertis pendant ce lancement.
    infos=$(ffprobe -v error -select_streams a:0 -show_entries stream=codec_name,sample_rate,channels -of csv=p=0 "$fichier") || infos=""
    case "$infos" in pcm_s16le,44100,1|pcm_s16le,44100,2|pcm_s16le,48000,1|pcm_s16le,48000,2) ;; *)
        printf 'IGNORÉ : WAV incompatible %s (%s).\n' "$nom" "$infos" >&2; echecs=$((echecs+1)); continue;; esac
    taille=$(stat -f '%z' "$fichier") || fail "Taille illisible."
    if [ "$taille" -gt "$MAX_WAV_BYTES" ]; then printf 'IGNORÉ : %s dépasse 128 Mio.\n' "$nom" >&2; echecs=$((echecs+1)); continue; fi
    TMP_COPY=$(mktemp "$carte/.disco-copy.XXXXXX") || fail "Impossible d'écrire sur la carte."
    if cp -X "$fichier" "$TMP_COPY" && cmp -s "$fichier" "$TMP_COPY" && mv -n "$TMP_COPY" "$carte/$nom" && [ ! -e "$TMP_COPY" ]; then
        TMP_COPY=""; copies=$((copies+1)); printf 'Copié : %s\n' "$nom"
        # Seulement l'AppleDouble associé au fichier que ce script vient de copier.
        [ ! -f "$carte/._$nom" ] || rm -f -- "$carte/._$nom"
    else
        printf 'ERREUR : copie non finalisée pour %s ; original conservé.\n' "$nom" >&2
        rm -f -- "$TMP_COPY"; TMP_COPY=""; echecs=$((echecs+1))
    fi
done
sync
printf '\n%s fichier(s) copié(s), %s échec(s).\n' "$copies" "$echecs"
if diskutil unmount "$carte"; then
    printf 'Volume démonté. Vous pouvez retirer la carte.\n'
else
    printf 'Démontage impossible : éjecter la carte depuis le Finder avant de la retirer.\n' >&2
fi
pause_fin
