# Format KLX / FXLK de FreeStyle

## En-tête

Entiers non signés 32 bits, little-endian ; aucun alignement supplémentaire.

| Offset | Taille | Champ | Valeur dans FreeStyle |
|---:|---:|---|---:|
| `0x00` | 4 | Signature ASCII `FXLK` (`0x4b4c5846`) | `FXLK` |
| `0x04` | 4 | Taille décompressée de l'index | 6 103 |
| `0x08` | 4 | Taille stockée de l'index | 2 324 |
| `0x0c` | 2 324 | Index LZARI | |
| `0x920` | variable | Charges des fichiers, dans l'ordre de l'index | |

L'index est toujours passé au décodeur LZARI par la routine d'ouverture.
Il ne contient ni compteur d'entrées ni sentinelle : sa taille décodée donne
la fin de la table.

Chaque entrée de l'index décodé contient :

1. Un chemin Windows-1252 terminé par un octet nul.
2. La taille décompressée, `uint32` little-endian.
3. La taille stockée, `uint32` little-endian.

L'offset d'un fichier est `12 + taille_index_stockée + somme_des_tailles_stockées_précédentes`.
La fin du dernier fichier tombe exactement à l'octet 981 601, la taille de
l'archive. Il n'y a pas de checksum d'origine ni de métadonnées de date.

## Charges

Si les tailles stockée et décompressée sont égales, chaque octet est décodé par
`octet ^ 0x9a`. Cela concerne deux fichiers : `chromball.jpg` et `PseudoNULL.lwo`.

Sinon, la charge est un flux LZARI sans en-tête de taille propre. Les 159 autres
fichiers et l'index utilisent ce décodeur :

- Fenêtre LZ de 4 096 octets, correspondances de 3 à 60 octets.
- Les 4 036 premiers octets de la fenêtre commencent à `0x20`, les 60 derniers
  à zéro ; le curseur initial vaut 4 036.
- Alphabet de 314 symboles : 0–255 littéraux ; 256–313 correspondances dont
  la longueur vaut `symbole - 253`.
- Codage arithmétique adaptatif, bornes initiales `[0, 0x20000)`, code initial
  lu sur 17 bits, bits de poids fort en premier.
- Fréquences des caractères initialisées à 1 ; réordonnancement par fréquence,
  réduction de moitié lorsque la somme atteint `0x7fff`.
- Modèle fixe des positions : `cum[4096] = 0`, puis
  `cum[i-1] = cum[i] + 10000 / (i + 200)` pour `i` de 4096 à 1, division entière.
- La distance décodée est comprise entre 1 et 4 096. Les copies chevauchantes
  réutilisent les octets écrits dans la fenêtre.
- Arrêt après la taille décompressée annoncée, sans marqueur de fin de fichier.

Le compresseur termine son codage arithmétique puis pousse sept bits nuls.
La lecture du décodeur peut anticiper au-delà du dernier octet stocké. Le C
autorise au maximum deux octets virtuels nuls pour cette terminaison et refuse
les lectures au-delà. La totalité de l'archive a été comparée à la routine
d'origine avec ce traitement.

## Méthode de reconstruction

`freestyle.exe` est emballé dans un ancien UPX. UPX 5.1.1 et 3.09 ont refusé
la décompression, indiquant une version obsolète. Le stub a donc été émulé en
x86 avec Unicorn, de `0x004a9d00` à `0x004a9e0a`, après la restauration du code
et des appels relatifs, mais avant la résolution des imports Windows.
L'application graphique n'a pas été lancée.

Adresses virtuelles utiles dans l'image à base `0x00400000` :

| Adresse | Rôle constaté |
|---|---|
| `0x00401290` | Ouverture et lecture de l'archive |
| `0x00401319` | Comparaison de la signature `0x4b4c5846` |
| `0x00401338` | Lecture des tailles et de l'index compressé |
| `0x004014c0` | Recherche d'un fichier et lecture de sa charge |
| `0x00401687` | Boucle XOR `0x9a` |
| `0x00401c80` | Initialisation des modèles arithmétiques |
| `0x00401d10` | Mise à jour des fréquences et réordonnancement |
| `0x00402230` | Décodage d'un caractère / symbole de longueur |
| `0x004023c0` | Décodage d'une position |
| `0x00402550` | Compresseur d'origine, utilisé pour les fixtures de tests |
| `0x00402930` | Décompresseur LZARI complet |
| `0x00402ad0` | Configuration du tampon d'entrée et de la taille de sortie |
| `0x00402b00` | Initialisation de l'objet codec |

Pour obtenir une référence indépendante du port C, les routines LZARI et XOR
originales ont été émulées sur chaque charge. Seul `malloc` (`0x00436280`) a
été remplacé par un tampon de travail. L'index et les 161 charges correspondent
octet pour octet au prototype Python. Les sorties du C correspondent ensuite
aux 161 empreintes SHA-256 de ce décodage original, enregistrées dans
`freestyle-manifest.json`, avec l'empreinte de l'exécutable source.

Les scripts et le dump de travail sont conservés dans `analysis/klx/`
(ignoré par Git). Le prototype `tools/unpack_klx.py` ne sert pas au C :
`klx_unpack.c` contient tout ce qui est nécessaire à l'extraction.

## Validation et limites

- 72 JPEG entièrement décodés avec Pillow pendant la validation.
- 64 objets `FORM/LWOB` : tailles FORM et limites de tous les chunks vérifiées.
- 11 scènes portant l'en-tête `LWSC`, version 1.
- 12 fichiers avec signature `MOA3`, conservés sans interprétation complète.
- Musique `Mush.mo3` avec signature `MO3`, extraite sans validation audio ni conversion.
- Construction Windows x64 Release avec MSVC `/W4 /WX` et `/MT` ; uniquement
  `KERNEL32.dll` dans la table des imports.

L'extracteur exige une nouvelle destination, valide l'index et décode toutes
les charges avant de créer des fichiers. Il rejette les traversées de chemin,
les alias absolus non reconnus, les noms réservés usuels Windows, les collisions
insensibles à la casse Windows-1252 et les conflits fichier/répertoire.
Les chemins d'archive `D:\...` deviennent `D/...` sous la destination. Les
chemins UTF-8 sont convertis en UTF-16 pour les opérations Windows.

Les limites de l'outil sont 256 Mio pour l'archive, pour l'index décompressé
et pour la somme des fichiers décodés, 100 000 entrées et 4 096 octets par nom.
`--list` valide la table et les limites des charges, sans décoder les charges.
Sans checksum d'origine, certaines altérations des flux peuvent produire des
données de la bonne taille : les empreintes fournies identifient exactement
les fichiers de cette archive. La compatibilité avec d'autres variantes KLX
n'a pas été établie.
