// Projet
// Réalisé par Anaïs Delerue, Lilou Guillain et Sephora Ingani



//============================================================================
// DÉTECTION DE DROITES PAR TRANSFORMÉE DE HOUGH ET RANSAC
// ============================================================================
// Projet : Détection robuste de segments/droites dans une image
// Méthodes : Hough polaire + RANSAC pour raffinage
// ============================================================================

#include <algorithm>
#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
#include <string>
#include <cstdlib>
using namespace std;

// ============================================================================
// SECTION 1 : STRUCTURES DE DONNÉES
// ============================================================================

template<typename T>
struct Pixel {
    T R, G, B;  // Rouge, Vert, Bleu
};


// Représente une grille d'image 2D
//Stocke les pixels et permet un accès rapide par (ligne, colonne)

template<typename T>
struct grille {
    int height, width;
    vector<vector<Pixel<T>>> grilles;

    // Accès rapide aux pixels
    Pixel<T>& at(int row, int col) { return grilles[row][col]; }
    const Pixel<T>& at(int row, int col) const { return grilles[row][col]; }
};


// Représente un point 2D (x, y)
// T : type générique (int pour pixels, double pour calculs)

template<typename T>
struct point {
    T x, y;
};


// Représente une droite en coordonnées POLAIRES
// rho   : distance de la droite à l'origine
// theta : angle avec l'axe des abscisses
// Avantage : gère droites verticales (theta = π/2)

struct DroitePolaire {
    double rho;
    double theta;
};


// Représente une droite en coordonnées CARTÉSIENNES
// Éq : a*x + b*y + c = 0

// Permet de calculer distance d'un point à la droite

struct DroiteCartesienne {
    double a, b, c;


     // Calcule distance perpendiculaire d'un point (x, y) à la droite
     // Distance = |ax + by + c| / sqrt(a^2 + b^2)

    double distance(double x, double y) const {
        double norme = sqrt(a*a + b*b);
        if (norme < 1e-9) return 1e9;
        return fabs(a*x + b*y + c) / norme;
    }
};

// ============================================================================
// SECTION 2 : CONVERSIONS ENTRE REPRÉSENTATIONS DE DROITES
// ============================================================================

// Convertit une droite polaire (rho, theta) en cartésienne (ax + by + c = 0)

//Formules :
//   a = cos(theta)
//   b = sin(theta)
//  c = -rho

DroiteCartesienne polaire_vers_cartesienne(DroitePolaire d) {
    return { cos(d.theta), sin(d.theta), -d.rho };
}


// Calcule la droite passant par deux points P1 et P2

// Méthode : équation (y2-y1)*x + (x1-x2)*y + (x2*y1 - x1*y2) = 0
//   a = y2 - y1
//   b = x1 - x2
//   c = -(a*x1 + b*y1)

DroiteCartesienne droiteParDeuxPoints(point<double> p1, point<double> p2) {
    double a = p2.y - p1.y;
    double b = p1.x - p2.x;
    double c = -(a * p1.x + b * p1.y);
    return {a, b, c};
}

// ============================================================================
// SECTION 3 : LECTURE/ÉCRITURE D'IMAGES PPM
// ============================================================================


// Lit un fichier PPM P3
 
// Format PPM P3 :
//   - En-tête : P3
//   - Commentaires (#)
//   - Dimensions : largeur hauteur
//   - Max couleur : 255
//   - Données : pixels R G B

grille<int> lirePPM(const string& chemin) {
    ifstream f(chemin);
    if (!f) {
        cout << "ERREUR : impossible d'ouvrir " << chemin << endl;
        exit(1);
    }

    // Lambda pour lire un token en ignorant commentaires et espaces
    auto lireToken = [&]() -> string {
        string tok;
        while (true) {
            // Saute les espaces
            while (f.peek() == ' ' || f.peek() == '\t' ||
                   f.peek() == '\n' || f.peek() == '\r')
                f.get();
            // Saute les commentaires
            if (f.peek() == '#') {
                string ligne;
                getline(f, ligne);
                continue;
            }
            f >> tok;
            break;
        }
        return tok;
    };

    // Lecture de l'en-tête
    string type = lireToken();
    if (type != "P3") {
        cout << "ERREUR : ce n'est pas un fichier PPM P3" << endl;
        exit(1);
    }

    int w      = stoi(lireToken());
    int h      = stoi(lireToken());
    int maxval = stoi(lireToken());
    (void)maxval;

    cout << "✓ Image chargee : " << w << "x" << h << endl;

    // Initialisation de la grille
    grille<int> img;
    img.width  = w;
    img.height = h;
    img.grilles.resize(h, vector<Pixel<int>>(w));

    // Lecture des pixels
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            img.grilles[y][x].R = stoi(lireToken());
            img.grilles[y][x].G = stoi(lireToken());
            img.grilles[y][x].B = stoi(lireToken());
        }

    return img;
}

// Écrit une grille d'image au format PPM P3
// Assure que les valeurs sont dans [0, 255]

void ecrirePPM(const string& chemin, const grille<int>& img) {
    ofstream f(chemin);
    if (!f) {
        cout << "ERREUR : impossible de creer " << chemin << endl;
        exit(1);
    }

    // En-tête PPM
    f << "P3\n";
    f << img.width << " " << img.height << "\n";
    f << "255\n";

    // Pixels
    for (int row = 0; row < img.height; row++) {
        for (int col = 0; col < img.width; col++) {
            const auto& p = img.grilles[row][col];
            int r = max(0, min(255, p.R));
            int g = max(0, min(255, p.G));
            int b = max(0, min(255, p.B));
            f << r << " " << g << " " << b;
            if (col + 1 < img.width) f << "  ";
        }
        f << "\n";
    }

    cout << " Image sauvegardee : " << chemin << endl;
}

// ============================================================================
// SECTION 4 : TRAITEMENT D'IMAGES (FILTRES)
// ============================================================================

// Convertit une image couleur en niveaux de gris
// Coefficients privilégient vert (sensibilité occulaire)

grille<int> versGris(const grille<int>& img) {
    grille<int> gris;
    gris.width  = img.width;
    gris.height = img.height;
    gris.grilles.resize(img.height, vector<Pixel<int>>(img.width));

    for (int r = 0; r < img.height; r++)
        for (int c = 0; c < img.width; c++) {
            const auto& p = img.at(r, c);
            int lum = (int)(0.299*p.R + 0.587*p.G + 0.114*p.B);
            gris.grilles[r][c] = {lum, lum, lum};
        }
    return gris;
}

// FILTRE DE SOBEL : Détection de contours par calcul de gradients

// Principe :
//   1. Calcule le gradient horizontal (Gx) et vertical (Gy)
//   2. Magnitude = sqrt(Gx^2 + Gy^2)
//   3. Normalise à [0, 255]

// Noyaux Sobel (3x3) :
//   Gx = [-1  0  +1]    Gy = [-1  -2  -1]
//        [-2  0  +2]         [ 0   0   0]
//        [-1  0  +1]         [+1  +2  +1]
 
// IMPORTANT : Normalisation avant seuillage (sinon perte info)
 
grille<int> sobel(const grille<int>& gris) {
    int h = gris.height;
    int w = gris.width;

    grille<int> res;
    res.height = h;
    res.width  = w;
    res.grilles.resize(h, vector<Pixel<int>>(w, {0,0,0}));

    // Étape 1 : Calcul magnitudes
    vector<vector<int>> magnitudes(h, vector<int>(w, 0));
    int maxMag = 0;

    for (int r = 1; r < h-1; r++) {
        for (int c = 1; c < w-1; c++) {
            // Gradient horizontal (Gx)
            int Gx = -gris.at(r-1,c-1).R + gris.at(r-1,c+1).R
                     -2*gris.at(r,c-1).R  + 2*gris.at(r,c+1).R
                     -gris.at(r+1,c-1).R  + gris.at(r+1,c+1).R;

            // Gradient vertical (Gy)
            int Gy = -gris.at(r-1,c-1).R - 2*gris.at(r-1,c).R - gris.at(r-1,c+1).R
                     +gris.at(r+1,c-1).R  + 2*gris.at(r+1,c).R + gris.at(r+1,c+1).R;

            // Magnitude du gradient
            int mag = (int)sqrt((double)(Gx*Gx + Gy*Gy));
            magnitudes[r][c] = mag;
            maxMag = max(maxMag, mag);
        }
    }

    // Étape 2 : Normalisation à [0, 255]
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            int val = (maxMag > 0) ? (magnitudes[r][c] * 255 / maxMag) : 0;
            res.grilles[r][c] = {val, val, val};
        }
    }

    return res;
}

 // Seuillage : Conversion en image binaire (noir/blanc)

// Règle :
//   - Si pixel > seuil  --> blanc (255)
//   - Sinon             --> noir (0)

grille<int> seuillage(const grille<int>& img, int seuil) {
    grille<int> res;
    res.height = img.height;
    res.width  = img.width;
    res.grilles.resize(img.height, vector<Pixel<int>>(img.width, {0,0,0}));

    for (int r = 0; r < img.height; r++)
        for (int c = 0; c < img.width; c++) {
            int val = img.at(r,c).R > seuil ? 255 : 0;
            res.grilles[r][c] = {val, val, val};
        }
    return res;
}


// Extrait les coordonnées de tous les pixels blancs d'une image
 
 vector<point<int>> extrairePointsBlancs(const grille<int>& img) {
    vector<point<int>> pts;
    for (int r = 0; r < img.height; r++)
        for (int c = 0; c < img.width; c++)
            if (img.at(r,c).R > 128)
                pts.push_back({c, r});
    return pts;
}

// ============================================================================
// SECTION 5 : DESSIN DE DROITES (BRESENHAM)
// ============================================================================


// Trouve les deux points où la droite (ax + by + c = 0) coupe les bords de l'image.
// Teste les 4 bords : gauche (x=0), droite (x=w-1), haut (y=0), bas (y=h-1).
// Retourne false si droite ne traverse pas l'image (moins de 2 points trouvés).


bool trouverBords(double a, double b, double c,
                  int w, int h,
                  int& x1, int& y1, int& x2, int& y2) {
    vector<pair<int,int>> pts;

    // Bords verticaux (x=0 et x=w-1) --> y = -(c + a*x) / b
    if (fabs(b) > 1e-9) {
        int y = (int)round(-c / b);
        if (y >= 0 && y < h) pts.push_back({0, y});

        y = (int)round(-(c + a*(w-1)) / b);
        if (y >= 0 && y < h) pts.push_back({w-1, y});
    }

    // Bords horizontaux (y=0 et y=h-1) --> x = -(c + b*y) / a
    if (fabs(a) > 1e-9) {
        int x = (int)round(-c / a);
        if (x >= 0 && x < w) pts.push_back({x, 0});

        x = (int)round(-(c + b*(h-1)) / a);
        if (x >= 0 && x < w) pts.push_back({x, h-1});
    }

    if (pts.size() < 2) return false;
    x1 = pts[0].first;  y1 = pts[0].second;
    x2 = pts[1].first;  y2 = pts[1].second;
    return true;
}


// Dessine  droite dans image en utilisant l'algo de Bresenham.

// Étapes :
//   1. Trouver les points d'entrée/sortie de la droite dans l'image (line clipping)
//   2. Calculer les deltas dx et dy, et les directions sx et sy (+1 ou -1)
//   3. Avancer pixel par pixel en mettant à jour l'erreur pour choisir la direction

void dessinerDroite(grille<int>& img, const DroiteCartesienne& droite,
                    int R, int G, int B) {

    int x1, y1, x2, y2;
    if (!trouverBords(droite.a, droite.b, droite.c,
                      img.width, img.height, x1, y1, x2, y2))
        return;

    // Algorithme de Bresenham
    int dx  =  abs(x2 - x1);
    int dy  =  abs(y2 - y1);
    int sx  = (x1 < x2) ? 1 : -1;
    int sy  = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        // Dessiner le pixel courant
        if (x1 >= 0 && x1 < img.width && y1 >= 0 && y1 < img.height) {
            img.at(y1, x1).R = R;
            img.at(y1, x1).G = G;
            img.at(y1, x1).B = B;
        }

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

// ============================================================================
// SECTION 6 : ALGORITHME RANSAC
// ============================================================================


// Retourne tous les points situés à distance ≤ epsilon de la droite
// Utilisé pour identifier les "inliers" (points valides)

vector<point<int>> pointsProches(const vector<point<int>>& points,
                                  const DroiteCartesienne& droite,
                                  double epsilon) {
    vector<point<int>> proches;
    for (const auto& p : points)
        if (droite.distance(p.x, p.y) <= epsilon)
            proches.push_back(p);
    return proches;
}


 // RANSAC (RANdom SAmple Consensus) : Trouvez la meilleure droite

// Algo :
//   1. Sélectionner aléatoirement 2 points
//   2. Calculer la droite passant par ces 2 points
//   3. Compter les "inliers" (points à distance < seuil)
//   4. Garder le modèle avec le plus d'inliers

// Paramètres :
//   - K : nombre d'itérations (plus = meilleur mais plus lent)
//   - t : seuil de distance pour un inlier

// Robustesse : tolère jusqu'à 50% de points aberrants 

DroiteCartesienne ransac(const vector<point<int>>& candidats, int K, double t) {
    if (candidats.size() < 2) return {0, 0, 0};

    DroiteCartesienne meilleure = {0, 0, 0};
    int maxInliers = 0;

    for (int iter = 0; iter < K; iter++) {
        // Sélection aléatoire de 2 points distincts
        int i1 = rand() % candidats.size();
        int i2 = rand() % candidats.size();
        if (i1 == i2) continue;

        point<double> pa = {(double)candidats[i1].x, (double)candidats[i1].y};
        point<double> pb = {(double)candidats[i2].x, (double)candidats[i2].y};

        // Vérifier que les points ne sont pas confondus
        if (fabs(pa.x-pb.x) < 1e-9 && fabs(pa.y-pb.y) < 1e-9) continue;

        // Calculer la droite
        DroiteCartesienne droite = droiteParDeuxPoints(pa, pb);
        double norme = sqrt(droite.a*droite.a + droite.b*droite.b);
        if (norme < 1e-9) continue;  // Droite invalide

        // Compter les inliers
        int nbInliers = 0;
        for (const auto& p : candidats)
            if (droite.distance(p.x, p.y) <= t)
                nbInliers++;

        // Mettre à jour si meilleur
        if (nbInliers > maxInliers) {
            maxInliers = nbInliers;
            meilleure  = droite;
        }
    }

    cout << "  RANSAC : " << maxInliers << " inliers sur "
         << candidats.size() << " candidats" << endl;
    return meilleure;
}

// ============================================================================
// SECTION 7 : TRANSFORMÉE DE HOUGH
// ============================================================================


// HOUGH NAÏF (Variante cartésienne) : y = ax + b

// Pbm  : Impossible de représenter les droites verticales (la pente a serait infinie)
// Donc on préfère la variante polaire

grille<int> houghNaif(const grille<int>& img) {
    int tailleA = 40;
    int tailleB = img.width + img.height;

    vector<vector<int>> acc(tailleA, vector<int>(tailleB, 0));

    // Pour chaque pixel blanc de l'image
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++)
            if (img.at(y,x).R > 128)
                // Pour chaque pente possible
                for (int ia = 0; ia < tailleA; ia++) {
                    double a  = (ia - tailleA/2);
                    double b  = y - a * x;
                    int    ib = (int)(b + tailleB/2);
                    if (ib >= 0 && ib < tailleB)
                        acc[ia][ib]++;
                }

    // Trouver le pic maximum
    int maxVal = 0, bestA = 0, bestB = 0;
    for (int ia = 0; ia < tailleA; ia++)
        for (int ib = 0; ib < tailleB; ib++)
            if (acc[ia][ib] > maxVal) {
                maxVal = acc[ia][ib];
                bestA = ia; bestB = ib;
            }

    double a = bestA - tailleA/2;
    double b = bestB - tailleB/2;
    cout << "[Hough naif] y = " << a << "x + " << b
         << " (score=" << maxVal << ")" << endl;

    // Visualiser l'accumulateur
    grille<int> result;
    result.width  = tailleB;
    result.height = tailleA;
    result.grilles.resize(tailleA, vector<Pixel<int>>(tailleB));
    for (int ia = 0; ia < tailleA; ia++)
        for (int ib = 0; ib < tailleB; ib++) {
            int val = min(255, acc[ia][ib] * 10);
            result.grilles[ia][ib] = {val, val, val};
        }
    return result;
}


// HOUGH POLAIRE : p = x*cos(theta) + y*sin(theta)

// Avantages :
//   1. Représente TOUTES les droites (y compris les droites verticales)
//   2. Espace paramétrique BORNÉ : θ ∈ [0, π], ρ ∈ [-diag, +diag]
//   3. Les pics de l'accumulateur sont plus NETS (forme de sinusoïde)

// Paramètres:
//   - pts : points de contour détectés
//   - nbTheta : résolution angulaire (180 = 1deg par pixel)
//   - nbRho : résolution radiale (2*diag + 1 pixels)
//   - diag : distance diagonale de l'image

vector<vector<int>> calculerAccHough(const vector<point<int>>& pts,
                                      int nbTheta, int nbRho, double diag) {
    vector<vector<int>> acc(nbTheta, vector<int>(nbRho, 0));

    // Pour chaque point
    for (const auto& pt : pts)
        // Pour chaque angle θ
        for (int it = 0; it < nbTheta; it++) {
            double theta = it * M_PI / nbTheta;
            // Calculer ρ correspondant
            double rho   = pt.x * cos(theta) + pt.y * sin(theta);
            int    ir    = (int)(rho + diag);
            // Incrémenter l'accumulateur
            if (ir >= 0 && ir < nbRho)
                acc[it][ir]++;
        }
    return acc;
}


// Convertit un accumulateur Hough en image pour visualisation

// Pixels clairs = pics forts (beaucoup de votes)
// Pixels sombres = peu de votes

grille<int> accVersImage(const vector<vector<int>>& acc, int nbTheta, int nbRho) {
    // Trouver la val max pour normaliser
    int maxVal = 0;
    for (int it = 0; it < nbTheta; it++)
        for (int ir = 0; ir < nbRho; ir++)
            maxVal = max(maxVal, acc[it][ir]);

    // Créer image
    grille<int> result;
    result.width  = nbRho;
    result.height = nbTheta;
    result.grilles.resize(nbTheta, vector<Pixel<int>>(nbRho));

    // Remplir avec vals normalisées
    for (int it = 0; it < nbTheta; it++)
        for (int ir = 0; ir < nbRho; ir++) {
            int val = (maxVal > 0) ? min(255, acc[it][ir] * 255 / maxVal) : 0;
            result.grilles[it][ir] = {val, val, val};
        }
    return result;
}

// Supprime un pic et sa voisinage dans l'accumulateur

// Obj : après avoir détecté une droite, "effacer" le pic
// pour pouvoir trouver les droites suivantes (moins dominantes)

// Paramètre zone : demi-largeur du carré de suppression (pixels)

void supprimerPic(vector<vector<int>>& acc, int nbTheta, int nbRho,
                  int bestT, int bestR, int zone) {
    for (int dt = -zone; dt <= zone; dt++)
        for (int dr = -zone; dr <= zone; dr++) {
            int t = bestT + dt;
            int r = bestR + dr;
            if (t >= 0 && t < nbTheta && r >= 0 && r < nbRho)
                acc[t][r] = 0;
        }
}

// ============================================================================
// SECTION 8 : PROGRAMME PRINCIPAL
// ============================================================================

int main() {
    srand(42);  // Graine pour reproductibilité

    cout << "\n" << string(70, '=') << endl;
    cout << "DETECTION DE DROITES PAR HOUGH ET RANSAC" << endl;
    cout << string(70, '=') << "\n" << endl;

    // ========================================================================
    // PARTIE 1 : TESTS DE BASE
    // ========================================================================
    
    cout << "\n[PARTIE 1] TESTS FONCTIONNALITÉS DE BASE\n" << endl;

    point<double> p1 = {0.0, 0.0};
    point<double> p2 = {1.0, 1.0};
    DroiteCartesienne d = droiteParDeuxPoints(p1, p2);
    cout << "  Test droiteParDeuxPoints : a=" << d.a << " b=" << d.b
         << " c=" << d.c << endl;
    cout << "  Distance (1,0) à y=x : " << d.distance(1.0, 0.0)
         << " (attendu ≈ 0.707)" << endl;

    DroitePolaire dp = {3.0, 0.0};
    DroiteCartesienne dc = polaire_vers_cartesienne(dp);
    cout << "  Test polaire→cartésienne : a=" << dc.a << " b=" << dc.b
         << " c=" << dc.c << "\n" << endl;

    // ========================================================================
    // PARTIE 2 : RANSAC SUR IMAGE AVEC DEUX SEGMENTS
    // ========================================================================
    
    cout << "\n[PARTIE 2] RANSAC - DÉTECTION DE DROITE SIMPLE\n" << endl;

    grille<int> imgSeg = lirePPM("../imageAvecDeuxSegments.ppm");
    grille<int> grisSeg = versGris(imgSeg);
    vector<point<int>> tousPoints = extrairePointsBlancs(grisSeg);
    cout << "  Points blancs détectés : " << tousPoints.size() << endl;

    DroiteCartesienne droiteRansac = ransac(tousPoints, 1000, 2.0);
    grille<int> resultatRansac = imgSeg;
    dessinerDroite(resultatRansac, droiteRansac, 255, 0, 0);
    ecrirePPM("01_ransac_resultat.ppm", resultatRansac);

    // ========================================================================
    // PARTIE 3 : HOUGH NAÏF
    // ========================================================================
    
    cout << "\n[PARTIE 3] HOUGH NAÏF (variante cartésienne)\n" << endl;
    cout << "  Limitation : impossible de détecter droites verticales" << endl;

    grille<int> accNaif = houghNaif(grisSeg);
    ecrirePPM("02_hough_naif.ppm", accNaif);

    // ========================================================================
    // PARTIE 4 : HOUGH POLAIRE 
    // ========================================================================
    
    cout << "\n[PARTIE 4] HOUGH POLAIRE (variante polaire)\n" << endl;
    cout << " Avantages : gère toutes les orientations, espace borné" << endl;

    const double diag   = sqrt((double)(imgSeg.width*imgSeg.width +
                                        imgSeg.height*imgSeg.height));
    const int    nbTheta = 180;
    const int    nbRho   = 2*(int)diag + 1;

    cout << "  Dimensions accumulateur : " << nbTheta << " x " << nbRho << endl;

    vector<vector<int>> acc = calculerAccHough(tousPoints, nbTheta, nbRho, diag);
    grille<int> imgAcc = accVersImage(acc, nbTheta, nbRho);
    ecrirePPM("02_hough_polaire.ppm", imgAcc);

    // Extraire les 2 meilleures droites
    grille<int> resultatHough = imgSeg;
    int nDroites = 2;

    for (int n = 0; n < nDroites; n++) {
        // Trouver pic max dans l'accumulateur
        int maxVal = 0, bestT = 0, bestR = 0;
        for (int it = 0; it < nbTheta; it++)
            for (int ir = 0; ir < nbRho; ir++)
                if (acc[it][ir] > maxVal) {
                    maxVal = acc[it][ir];
                    bestT = it;
                    bestR = ir;
                }

        double theta = bestT * M_PI / nbTheta;
        double rho   = bestR - diag;
        cout << "  Droite " << n+1 << " : θ=" << (theta*180/M_PI) << "° ρ="
             << (int)rho << " (score=" << maxVal << ")" << endl;

        // Convertir en cartésien et raffiner avec RANSAC
        DroitePolaire    droiteP = {rho, theta};
        DroiteCartesienne droiteC = polaire_vers_cartesienne(droiteP);

        vector<point<int>> candidats = pointsProches(tousPoints, droiteC, 3.0);
        DroiteCartesienne droiteRaf  = ransac(candidats, 500, 1.0);

        double norme = sqrt(droiteRaf.a*droiteRaf.a + droiteRaf.b*droiteRaf.b);
        if (norme < 1e-9) droiteRaf = droiteC;

        dessinerDroite(resultatHough, droiteRaf, 255, 0, 0);
        supprimerPic(acc, nbTheta, nbRho, bestT, bestR, 5);
    }

    ecrirePPM("02_hough_resultat.ppm", resultatHough);

    // ========================================================================
    // PARTIE 5 : HOUGH SUR CONTOURS
    // ========================================================================
    cout << "\n[PARTIE 5] HOUGH SUR CONTOURS - DÉTECTION ROBUSTE\n" << endl;
    cout << "  Étapes : Image → Gris → Sobel → Seuillage → Hough → RANSAC" << endl;

    // Charger image
    grille<int> imgM1 = lirePPM("../imageM1.ppm");
    grille<int> grisM1 = versGris(imgM1);

    // Détection contours (Sobel + Normalisation)
    cout << "\n  [Étape 1] Détection de contours (Sobel)..." << endl;
    grille<int> contoursM1 = sobel(grisM1);
    ecrirePPM("04_sobel_magnitude.ppm", contoursM1);

    // Seuillage
    cout << "  [Étape 2] Seuillage des contours (seuil=50)..." << endl;
    int seuil = 50;
    grille<int> bordsM1 = seuillage(contoursM1, seuil);
    ecrirePPM("04_contours.ppm", bordsM1);

    // Extraction points de contour
    vector<point<int>> ptsBords = extrairePointsBlancs(bordsM1);
    cout << "  Points de contour trouvés : " << ptsBords.size() << endl;

    if (!ptsBords.empty()) {

        // Hough polaire sur contours
        cout << "\n  [Étape 3] Transformée de Hough polaire..." << endl;
        const double diag2 = sqrt((double)(imgM1.width*imgM1.width +
                                           imgM1.height*imgM1.height));
        const int nbTheta2 = 180;
        const int nbRho2  = 2*(int)diag2 + 1;

        vector<vector<int>> acc2 = calculerAccHough(ptsBords, nbTheta2, nbRho2, diag2);

        int maxAcc = 0;
        for (int it = 0; it < nbTheta2; it++)
            for (int ir = 0; ir < nbRho2; ir++)
                maxAcc = max(maxAcc, acc2[it][ir]);

        grille<int> imgAcc2 = accVersImage(acc2, nbTheta2, nbRho2);
        ecrirePPM("04_hough_contours.ppm", imgAcc2);

        // Détection droites majeures
        cout << "\n  [Étape 4] Extraction des droites principales..." << endl;
        grille<int> resultatContours = imgM1;
        int nDroites3 = 6;
        int seuil_pic = maxAcc / 2;
        int droitesTouvees = 0;

        for (int n = 0; n < nDroites3; n++) {

            int maxVal2 = 0, bestT2 = 0, bestR2 = 0;

            for (int it = 0; it < nbTheta2; it++)
                for (int ir = 0; ir < nbRho2; ir++)
                    if (acc2[it][ir] > maxVal2) {
                        maxVal2 = acc2[it][ir];
                        bestT2 = it;
                        bestR2 = ir;
                    }

            if (maxVal2 < seuil_pic) break;

            double theta2 = bestT2 * M_PI / nbTheta2;
            double rho2   = bestR2 - diag2;

            cout << "    Droite " << droitesTouvees+1 << " : θ="
                 << (int)(theta2*180/M_PI) << "° ρ=" << (int)rho2
                 << " (score=" << maxVal2 << ")" << endl;

            // Convertir et raffiner avec RANSAC
            DroitePolaire dp2 = {rho2, theta2};
            DroiteCartesienne dc2 = polaire_vers_cartesienne(dp2);

            vector<point<int>> candidats = pointsProches(ptsBords, dc2, 4.0);

            if (candidats.size() < 3) {
                supprimerPic(acc2, nbTheta2, nbRho2, bestT2, bestR2, 20);
                continue;
            }

            DroiteCartesienne droiteRaf = ransac(candidats, 300, 2.0);
            double norme = sqrt(droiteRaf.a*droiteRaf.a + droiteRaf.b*droiteRaf.b);
            if (norme < 1e-9) droiteRaf = dc2;

            // Dessiner la droite
            dessinerDroite(resultatContours, droiteRaf, 255, 0, 0);
            droitesTouvees++;

            // Supprimer le pic pour trouver le suivant
            supprimerPic(acc2, nbTheta2, nbRho2, bestT2, bestR2, 20);
        }

        cout << "\n  Droites détectées et dessinées : " << droitesTouvees << endl;
        ecrirePPM("04_hough_contours_resultat.ppm", resultatContours);
    }

    cout << "\n" << string(70, '=') << endl;
    cout << " PROGRAMME TERMINÉ AVEC SUCCÈS" << endl;
    cout << string(70, '=') << "\n" << endl;

    return 0;
}