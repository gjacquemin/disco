# Architecture Matérielle & Branchements

Ce document détaille l'ensemble des composants utilisés pour le lecteur audio autonome ainsi que le schéma de câblage complet de l'installation.

---

## 📦 Liste du matériel (BOM)

| Composant | Modèle / Référence | Rôle dans l'installation | Lien |
| :--- | :--- | :--- | :--- |
| **Microcontrôleur Audio** | Carte de développement ESP32-A1S (Audio Kit) | Cerveau du système : lit les fichiers audio et gère la sortie son via son DAC intégré. | [Voir sur Amazon](https://www.amazon.fr/dp/B0DF58DCQ7?ref=ppx_yo2ov_dt_b_fed_asin_title) |
| **Stockage** | Carte microSD SanDisk (64 Go) | Stocke la playlist et les morceaux découpés (formatée en FAT32). | [Voir sur Amazon](https://www.amazon.fr/SanDisk-Extreme-RescuePRO-Deluxe-jusquà/dp/B0B7NXBM6P/ref=sr_1_5) |
| **Diffusion sonore** | Enceintes stéréo Creative Pebble 2.0 | Restituent le signal audio analogique issu de l'ESP32. | [Voir sur Amazon](https://www.amazon.fr/CREATIVE-Enceintes-Haut-parleurs-Connection-100-17000/dp/B0791J2658/ref=asc_df_B0791J2658) |
| **Alimentation** | Chargeur secteur double port USB (Nisiyama) | Fournit l'alimentation 5V simultanée à l'ESP32 et aux enceintes. | [Voir sur Amazon](https://www.amazon.fr/Chargeur-Charger-Adaptateur-Alimentation-Nisiyama/dp/B0BXD4XRM2/ref=sr_1_2_sspa) |

---

## 🔌 Schéma des branchements

GitHub génère automatiquement le diagramme ci-dessous grâce à la syntaxe **Mermaid** :

```mermaid
flowchart LR
    %% Définition des styles
    classDef power fill:#ffebee,stroke:#c62828,stroke-width:2px,color:#000
    classDef compute fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#000
    classDef storage fill:#fff8e1,stroke:#f57f17,stroke-width:2px,color:#000
    classDef audio fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#000

    subgraph Secteur ["⚡ Alimentation 230V"]
        PSU["🔌 Chargeur Double USB<br/>(Sorties 5V)"]:::power
    end

    subgraph UniteCentrale ["🧠 Traitement & Stockage"]
        SD["💾 Carte microSD<br/>(Playlist Audio)"]:::storage
        ESP["🎛️ Carte ESP32-A1S<br/>(Audio Kit)"]:::compute
    end

    subgraph SortieAudio ["🔊 Diffusion"]
        SPK["🔈 Enceintes Creative Pebble 2.0"]:::audio
    end

    %% Connexions d'alimentation (5V)
    PSU -- "Câble USB (Alimentation 5V)" --> ESP
    PSU -- "Câble USB intégré (Alimentation 5V)" --> SPK

    %% Connexions de données et signal audio
    SD -. "Insertion slot microSD (SPI/SDIO)" .-> ESP
    ESP == "Câble Jack 3.5 mm (Audio Analogique)" ==> SPK
```

---

## 🛠️ Guide de montage pas à pas

1. **Préparation du stockage :**
   * Insérez la **carte microSD** (préalablement formatée en `FAT32` et contenant vos fichiers audio) dans le lecteur de carte intégré à la carte **ESP32**.
2. **Liaison Audio :**
   * Branchez le câble **Mini-Jack 3.5 mm** provenant des enceintes **Creative Pebble** sur la prise casque / sortie ligne (`EARPHONE` / `LINE OUT`) de la carte **ESP32**.
3. **Alimentation électrique :**
   * Branchez le **chargeur double USB** sur une prise murale.
   * Reliez le câble d'alimentation USB de l'**ESP32** (port `PWR` ou `USB`) sur le **Port USB n°1** du chargeur.
   * Reliez le câble USB des **enceintes Creative Pebble** sur le **Port USB n°2** du chargeur.

> **💡 Note technique :** Le fait d'alimenter l'ESP32 et les enceintes via le même bloc secteur permet de partager une masse commune, ce qui réduit généralement les risques de parasites audio (boucle de masse), tout en garantissant un courant suffisant sur chaque port pour éviter les chutes de tension lors des pics sonores.