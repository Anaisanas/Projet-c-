# Détection de droites par transformée de Hough et RANSAC

Projet C++ réalisé dans le cadre du M1 MMAA. Le but est de détecter automatiquement des droites dans une image couleur au format PPM, en combinant la transformée de Hough et l'algorithme RANSAC.

---

## Sujet

Une image numérique est une application définie sur une grille de pixels à valeurs dans [0, 255]³ (RGB). L'objectif est de détecter les segments/droites présents dans cette image, quelle que soit leur épaisseur, en utilisant :

- La **transformée de Hough** pour localiser globalement les droites dans un espace de paramètres
- **RANSAC** pour raffiner chaque droite détectée en se concentrant sur les points les plus cohérents

---

## Points saillants du travail réalisé

### Structures templates

Les structures `Pixel<T>` et `grille<T>` sont templatées, ce qui permet de les réutiliser pour des images d'entiers ou de flottants selon les besoins du traitement.

```cpp
grille<int> img = lirePPM("image.ppm");   // entiers pour le pipeline standard
```

### Lecture PPM robuste

Le parser PPM gère les commentaires (`#`) à n'importe quelle position dans le fichier, pas seulement dans l'en-tête. Le format l'autorise mais beaucoup d'implémentations naïves plantent sur ce cas.

### Pipeline de détection des contours

Avant d'appliquer Hough, l'image passe par :
1. **Conversion en niveaux de gris** avec la formule perceptuelle `0.299R + 0.587G + 0.114B`
2. **Filtre de Sobel** — détection des gradients horizontaux et verticaux
3. **Seuillage** — binarisation pour ne garder que les bords forts

### Hough naïf vs Hough polaire

La méthode naïve (`y = ax + b`) ne peut pas représenter les droites verticales (pente infinie). La méthode polaire `ρ = x·cosθ + y·sinθ` résout ce problème : θ ∈ [0, π] et ρ borné par la diagonale de l'image, donc l'espace des paramètres est toujours fini.

### RANSAC avec gestion des cas dégénérés

RANSAC raffine chaque droite Hough en cherchant les meilleurs inliers parmi les points candidats. La version implémentée vérifie explicitement que les deux points tirés aléatoirement ne sont pas identiques et que la droite résultante a une norme non nulle, pour éviter des divisions par zéro silencieuses.

### Tracé de droites (Bresenham)

Le tracé des droites détectées utilise l'**algorithme de Bresenham**, après un line clipping qui délimite la droite aux bords de l'image. L'algorithme parcourt les pixels de manière incrémentale sans calcul flottant, ce qui le rend rapide et exact sur grille entière.

---

## Compilation

```bash
g++ -std=c++17 -O2 -o detection main.cpp -lm
```

Aucune bibliothèque externe n'est requise.

---

## Utilisation

Le programme prend ses images directement dans le code (chemins relatifs). Placez vos fichiers PPM dans le dossier parent du dossier de compilation, puis lancez :

```bash
./detection
```

### Fichiers générés

| Fichier | Contenu |
|---|---|
| `01_ransac_resultat.ppm` | Image avec la droite RANSAC tracée en rouge (Partie 1) |
| `02_hough_naif.ppm` | Visualisation de l'accumulateur Hough naïf (cartésien) |
| `02_hough_polaire.ppm` | Visualisation de l'accumulateur Hough polaire |
| `02_hough_resultat.ppm` | Image originale avec les deux droites Hough+RANSAC en rouge |
| `04_sobel_magnitude.ppm` | Image des gradients Sobel normalisés |
| `04_contours.ppm` | Contours binaires après seuillage (seuil=50) |
| `04_hough_contours.ppm` | Accumulateur Hough calculé sur les contours Sobel |
| `04_hough_contours_resultat.ppm` | Image finale avec les droites majeures détectées en rouge |

---

## Exemples d'utilisation

### Exemple 1 — RANSAC direct sur imageAvecDeuxSegments

```
Image d'entrée    : imageAvecDeuxSegments.ppm
Itérations RANSAC : 1000
Seuil inliers t   : 2.0 pixels
Fichier produit   : 01_ransac_resultat.ppm
```

RANSAC est appliqué directement sur les points blancs de l'image, sans passer par Hough. Un seul passage détecte la droite dominante.

---

### Exemple 2 — Hough polaire + RANSAC sur imageAvecDeuxSegments

```
Image d'entrée    : imageAvecDeuxSegments.ppm
Seuil candidats ε : 3.0 pixels
Itérations RANSAC : 500
Seuil inliers t   : 1.0 pixel
Droites extraites : 2
Fichier produit   : 02_hough_resultat.ppm
```

Pipeline complet :
```
imageAvecDeuxSegments.ppm
        │
        ▼
   extrairePointsBlancs()  → points blancs
        │
        ▼
   calculerAccHough()      → accumulateur    →  02_hough_polaire.ppm
        │
        ▼
   extraction pics (×2)
        │
        ▼
   pointsProches()         → candidats RANSAC
        │
        ▼
   ransac(500, 1.0)        → droite raffinée
        │
        ▼
   dessinerDroite()        → tracé rouge     →  02_hough_resultat.ppm
```

---

### Exemple 3 — Pipeline complet sur imageM1 (Sobel + Hough + RANSAC)

```
Image d'entrée    : imageM1.ppm
Seuil Sobel       : 50
Seuil candidats ε : 4.0 pixels
Itérations RANSAC : 300
Seuil inliers t   : 2.0 pixels
Droites extraites : jusqu'à 6 (avec seuil = maxAcc / 2)
Fichier produit   : 04_hough_contours_resultat.ppm
```

Pipeline complet :
```
imageM1.ppm
        │
        ▼
   versGris()              → niveaux de gris
        │
        ▼
   sobel()                 → image des gradients  →  04_sobel_magnitude.ppm
        │
        ▼
   seuillage(50)           → bords binaires       →  04_contours.ppm
        │
        ▼
   calculerAccHough()      → accumulateur         →  04_hough_contours.ppm
        │
        ▼
   extraction pics (×6 max, seuil = maxAcc/2)
        │
        ▼
   pointsProches()         → candidats RANSAC
        │
        ▼
   ransac(300, 2.0)        → droite raffinée
        │
        ▼
   dessinerDroite()        → tracé rouge          →  04_hough_contours_resultat.ppm
```

---

### Exemple 4 — Adapter les paramètres à une nouvelle image

Pour une image plus grande ou plus bruitée, on recommande d'ajuster :

```cpp
grille<int> bords = seuillage(contours, 80);  // seuil plus haut si beaucoup de bruit

vector<point<int>> candidats = pointsProches(pts, droiteC, 5.0);  // epsilon plus large si segments épais

DroiteCartesienne droite = ransac(candidats, 1000, 2.0);  // plus d'itérations, seuil inlier plus souple
```

---

## Structure du code

```
main.cpp
├── Structures de base
│   ├── Pixel<T>
│   ├── grille<T>
│   ├── point<T>
│   ├── DroitePolaire
│   └── DroiteCartesienne
│
├── Fonctions droites
│   ├── polaire_vers_cartesienne()
│   └── droiteParDeuxPoints()
│
├── Lecture / écriture PPM
│   ├── lirePPM()
│   └── ecrirePPM()
│
├── Prétraitement
│   ├── versGris()
│   ├── sobel()
│   ├── seuillage()
│   └── extrairePointsBlancs()
│
├── Tracé
│   ├── trouverBords()         ← line clipping
│   └── dessinerDroite()       ← algorithme de Bresenham
│
├── RANSAC
│   ├── pointsProches()
│   └── ransac()
│
└── Hough
    ├── houghNaif()
    ├── calculerAccHough()
    ├── accVersImage()
    └── supprimerPic()
```
