# Preservation of FreeStyle

Extraction des ressources de **FreeStyle**, démo Win32 de Condense / Syndrome
(2000), codée par xBaRr. Les fichiers de distribution sont conservés dans
`demo-releases/` et `demo-unpack/`.

## Compiler sous Windows

CMake 3.15 ou plus récent et Visual Studio avec les outils C/C++ suffisent :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Le résultat est **`bin/klx_unpack.exe`**. Comme dans
`preservation-hcl-demos`, l'extracteur tient dans un fichier C99, utilise
`/W4 /WX` avec MSVC et lie statiquement le runtime C (`/MT`).
Il n'utilise aucune bibliothèque externe : ni zlib, ni Python, ni émulateur.
Le binaire Windows x64 fourni importe uniquement `KERNEL32.dll`.

## Extraire

Depuis la racine du projet, vers un dossier qui n'existe pas encore :

```powershell
.\bin\klx_unpack.exe demo-unpack\cds-freestyle\Freestyle\FreeStyle.klx demo-assets\cds-freestyle
```

Cette extraction a déjà été effectuée dans `demo-assets/cds-freestyle/`.
Pour la refaire, choisir une autre destination.

Les chemins d'origine deviennent des chemins locaux :
`D:\FreeStyle\Acet1.jpg` devient `D/FreeStyle/Acet1.jpg` sous la destination.
La casse et les accents Windows-1252 sont conservés. Les fichiers eux-mêmes
restent identiques aux données décodées par la démo ; les références absolues
à l'intérieur des scènes ne sont pas réécrites.

Afficher l'index sans extraire :

```powershell
.\bin\klx_unpack.exe --list demo-unpack\cds-freestyle\Freestyle\FreeStyle.klx
```

L'archive contient **161 fichiers**, soit **1 532 731 octets** décodés :

| Type | Nombre | Contenu vérifié |
|---|---:|---|
| `.jpg` | 72 | Images JPEG, décodage complet vérifié |
| `.lwo` | 64 | Objets LightWave `FORM/LWOB` |
| `.lws` | 11 | Scènes LightWave `LWSC`, version 1 |
| `.moa` | 12 | Fichiers propriétaires avec signature `MOA3` |
| `.mo3` | 1 | `D/FreeStyle/BGM/Mush.mo3`, signature `MO3` |
| `.txt` | 1 | Script / déroulé de la démo |

Il n'y a pas de TGA ni de MP3 autonome dans cet index. La musique est extraite
dans son conteneur MO3 d'origine, sans conversion audio.

## Vérifier

Les tests utilisent uniquement la bibliothèque standard de Python 3.10+ ;
Python reste facultatif pour compiler et utiliser l'extracteur.

```powershell
ctest --test-dir build -C Release --output-on-failure
# Ou directement :
python tests\unpack.py
```

Les empreintes SHA-256 des 161 fichiers de référence proviennent de la routine
x86 originale exécutée sous émulation pour l'analyse. Les tests comparent tous
les résultats du C à ces empreintes, vérifient les chemins Unicode, le stockage
XOR, les entrées vides, les archives malformées et le refus d'écraser une
destination existante. Les jeux de tests du petit index ont été produits par
le compresseur original, également présent dans `freestyle.exe`.

Voir [la documentation du format](documentation/klx-format.md) et
[le manifeste de référence](documentation/freestyle-manifest.json).
