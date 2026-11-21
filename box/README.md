# Disco — boîtier ESP32 Audio Kit V1

Boîtier en deux pièces imprimées, sans vis, écrous, inserts ni colle, pour la carte ESP32-Audio-Kit V2.2 montrée sur les photos. Fond violet et couvercle doré dans l’aperçu : ces couleurs servent seulement à distinguer les pièces. Une seule couleur de PLA+ suffit.

**Prototype dimensionné à partir des photos : aucune validation physique n’a encore été réalisée.** La carte est prise à 83 × 68 mm, antenne exclue. Les centres des trous, les hauteurs et les positions des prises sont estimés. Imprimer d’abord le gabarit : ne pas forcer une carte qui ne s’emboîte pas.

## Contenu

- `00_Gabarit_carte.stl` : cadre léger pour vérifier les quatre trous, les appuis et le dégagement de l’antenne.
- `01_Fond.stl` : fond du boîtier, face plane sur le plateau.
- `02_Couvercle_face_exterieure_sur_plateau.stl` : couvercle déjà retourné pour l’impression, clips et colonnettes vers le haut.
- `Disco_Boitier_assemblage.step` : les deux pièces en position assemblée, pour modification en CAO. Ne pas imprimer l’assemblage fermé.
- `generer.py` : modèle paramétrique Python / CadQuery 2.7 ; paramètres regroupés au début du fichier.
- `Apercu.png` : vue fermée et vue éclatée ; la carte représentée est schématique.
- `verification_geometrique.json` : dimensions, volumes et contrôle de validité des solides.

Les STL sont en millimètres, à importer à **100 %**. Ne pas redimensionner globalement le boîtier pour rattraper un trou : cela changerait aussi les jeux des clips et les ouvertures.

## Dispositif mécanique

Le circuit repose sur quatre petits appuis, avec 4 mm libres entre le fond intérieur et la face inférieure de la carte. Quatre pions fins de 1,8 mm passent librement dans les trous de montage. Ils centrent la carte sans serrage. Le couvercle comporte quatre butées au-dessus des mêmes zones, avec 0,3 mm de jeu au-dessus d’un circuit supposé épais de 1,6 mm. Elles limitent le soulèvement du circuit lors des branchements.

Quatre languettes intégrées au couvercle s’enclenchent dans des logements du fond. Leur engagement nominal est de 0,25 mm. Une petite encoche à l’arrière permet de soulever progressivement le couvercle avec l’ongle. Les clips sont destinés à des ouvertures occasionnelles : leur force et leur durée de vie restent à valider avec l’impression réelle.

La zone antenne est ouverte dans le fond, la paroi et le couvercle : aucun pont de plastique ne passe au-dessus de sa partie sortante. Une grande fenêtre à droite laisse accéder aux deux micro-USB et à la microSD. À gauche, une fenêtre laisse accéder à HEADPHONES, appelé EARPHONES sur certaines sérigraphies. La prise LINE IN n’est pas prévue comme accès permanent.

Les fenêtres montent jusqu’au couvercle, ce qui évite les longs ponts à imprimer. Les boutons, borniers haut-parleurs et le connecteur batterie restent accessibles en retirant le couvercle. Six fentes sur le couvercle permettent l’aération.

## Dimensions et hypothèses à vérifier

Orientation : carte vue de dessus, antenne vers soi, jacks à gauche, USB à droite. Origine XY au coin avant gauche du circuit principal. X va vers la droite, Y vers l’arrière.

| Élément | Cote du modèle |
| --- | --- |
| Circuit principal, hors antenne | 83 × 68 mm |
| Encombrement du boîtier assemblé | 88,4 × 73,4 × 22 mm, hors antenne et câbles |
| Épaisseur du fond, des parois, du toit | 2 mm |
| Jeu latéral autour du circuit | 0,7 mm par côté |
| Épaisseur de circuit supposée | 1,6 mm |
| Centres des trous estimés | (3,5 ; 3,5), (79,5 ; 3,5), (3,5 ; 64,5), (79,5 ; 64,5) mm |
| Entraxes correspondants | 76 × 61 mm |
| Dessous carte par rapport au dessous du boîtier | 6 mm |
| Hauteur disponible au-dessus de la carte | 12,4 mm, sauf clips et butées en périphérie |
| Ouverture antenne | X = 30 à 56 mm, jusqu’à Y = 8 mm vers l’intérieur |
| Fenêtre HEADPHONES | Y = 5 à 22 mm sur la paroi gauche |
| Fenêtre USB et SD | Y = 6 à 54 mm sur la paroi droite |
| Hauteur des fenêtres latérales | Z = 5,2 à 20 mm |

Le gabarit valide surtout l’entraxe et le placement des appuis. Il ne valide pas la hauteur des composants ou le passage des câbles. Vérifier séparément que les soudures sous le circuit ne dépassent pas 4 mm, que les composants ne dépassent pas 12,4 mm au-dessus du circuit, et que les zones de montage sont libres sur les deux faces. Vérifier également les embouts des câbles réels avant fermeture.

## Impression sur Bambu Lab A1, eSUN PLA+

Point de départ proposé pour une buse de **0,4 mm** et un plateau PEI texturé. Ces réglages sont à adapter au profil du filament et à l’étiquette de la bobine, notamment si elle est marquée PLA+ HS.

| Réglage | Proposition |
| --- | --- |
| Imprimante | Bambu Lab A1, buse 0,4 mm |
| Filament | Profil eSUN PLA+ s’il est disponible ; sinon Generic PLA ajusté |
| Hauteur de couche | 0,20 mm, première couche 0,20 mm |
| Température de buse | 220 °C pour commencer |
| Plateau PEI texturé | 55 °C pour commencer |
| Parois | 3 boucles |
| Couches supérieures / inférieures | 5 / 5 |
| Remplissage | 15 % gyroid |
| Ventilation | Profil PLA, jusqu’à 100 % après les premières couches |
| Paroi externe / petits périmètres | 40 à 60 mm/s pour préserver les clips et les pions |
| Supports | Désactivés dans les orientations fournies |
| Bordure | Aucune au départ ; bordure extérieure si les coins se soulèvent |
| Compensation de pied d’éléphant | 0,15 mm comme point de départ |
| Échelle | 100 %, unités mm |

Les deux pièces tiennent côte à côte sur le plateau de l’A1. Les importer comme objets séparés, avec les orientations fournies, puis les poser sur le plateau. Le couvercle doit avoir sa grande face extérieure contre le plateau et ses quatre clips vers le haut. Vérifier l’aperçu du tranchage : les languettes et les petites pointes doivent apparaître comme des volumes continus, pas comme des éléments supprimés par le trancheur.

Les températures proposées se situent dans la plage eSUN publiée pour le PLA+ : buse 210–230 °C et plateau 45–60 °C. La fiche Bambu A1 indique un volume de 256 × 256 × 256 mm et une buse fournie de 0,4 mm. Ce sont des points de départ, pas un profil d’impression testé pour ce boîtier.

## Ordre de montage

1. Imprimer `00_Gabarit_carte.stl`, puis laisser refroidir. Enlever les éventuelles bavures des quatre pions.
2. Carte débranchée, la poser sur les appuis : les quatre pions doivent entrer sans effort. Vérifier que chaque appui touche seulement la zone de montage et que l’antenne est libre. Si les trous ne correspondent pas, relever les entraxes et modifier `HOLE_INSET_X` / `HOLE_INSET_Y` avant d’imprimer les pièces finales.
3. Vérifier les hauteurs réelles et les cotes des ouvertures ci-dessus, puis imprimer le fond et le couvercle.
4. Essayer d’abord la fermeture à vide. Enclencher progressivement les quatre clips, sans torsion. Si c’est trop dur, ébavurer les logements ; ne pas forcer. Dans le modèle, augmenter `FIT` de 0,05 mm diminue l’engagement, ou diminuer `BEAD_PROJECTION` de 0,05 mm. Régénérer le modèle et garder un engagement positif.
5. Ouvrir, poser la carte sur les pions, vérifier le passage de HEADPHONES, des deux USB et de la carte SD avec les câbles utilisés. Débrancher les câbles pour la fermeture si nécessaire.
6. Orienter les encoches d’antenne l’une sur l’autre et fermer le couvercle. Il ne doit pas appuyer sur les composants ; les butées visent uniquement les quatre coins de fixation.
7. Pour ouvrir, soulever doucement depuis l’encoche arrière puis libérer progressivement les clips. Ne pas tirer sur la carte, l’antenne ou les prises.

Le boîtier est ajouré, non étanche. Ne pas le placer dans une zone exposée aux projections d’eau. Les aérations et la zone antenne restent libres.

## Modifier le modèle

Installer CadQuery 2.7 dans un environnement Python compatible puis lancer :

```sh
python3 generer.py
```

Le script régénère les trois STL, le STEP et le rapport géométrique dans son propre dossier. Les paramètres sont exprimés en millimètres. Le STEP permet aussi une modification avec un logiciel de CAO acceptant ce format. Le rendu n’est pas régénéré par ce script.

Vérifications réalisées : chaque pièce est un solide valide et unique ; pas d’intersection volumique entre fond et couvercle en position assemblée. Cela ne remplace ni une impression d’essai, ni la mesure des composants réels, ni un essai de tenue des clips. Aucun G-code propre à une imprimante n’est fourni.

## Sources des paramètres d’impression

- eSUN PLA+ : https://www.esun3d.com/pla-pro-product
- Bambu Lab A1, fiche technique : https://store.bblcdn.eu/s8/default/29c02564a6374ed5be971523eb9193e2/A1_Spec_EN_1214.pdf

Consultées le 21 novembre 2026.
