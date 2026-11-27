/*
 * m6_ai_sched.c - Milestone 6 : ordonnancement intelligent guide par l'IA
 *
 * Trois vols arrivent au meme instant t = 0 : TU702, TU703, TU701.
 * Chaque vol demande 3 sous-taches : recherche de porte, calcul du chemin
 * passager, notification.
 *
 * Mode 1 "Priorite fixe" : ordre d'arrivee, l'IA n'intervient pas.
 * Mode 2 "Priorite IA"   : le vol au plus fort risque de correspondance
 *                          manquee est traite en premier.
 *
 * Les temps de reponse sont mesures en reel (CLOCK_MONOTONIC) : chaque
 * sous-tache est simulee par une attente active de sa duree.
 *
 * Compilation : gcc -O2 -Wall m6_ai_sched.c -o m6_ai_sched -lm
 * Execution   : ./m6_ai_sched
 */
#include <stdio.h>
#include <math.h>
#include <time.h>

#define NB_VOLS     3
#define NB_SOUS     3
#define NB_ESSAIS   5        /* executions pour la moyenne */
#define DEADLINE_MS 150.0    /* echeance de decision par vol (a ajuster) */

enum { MODE_FIXE, MODE_IA };

typedef struct {
    const char *nom;
    double retard, nb_corr, restant, deplac, secu;   /* entrees du modele IA */
    double risque;                                   /* sortie IA : P(manquee) */
} Vol;

/* Vols dans l'ordre d'arrivee (= ordre de la priorite fixe) */
static Vol vols[NB_VOLS] = {
    /* nom     retard nb_corr restant deplac secu risque */
    { "TU702",   10,    25,     35,     10,    5,   0 },
    { "TU703",   25,    20,     34,     10,   10,   0 },
    { "TU701",   30,    35,     40,      5,   15,   0 },
};

static const char *noms_sous[NB_SOUS] = { "recherche porte", "chemin passager", "notification" };
static const int   duree_ms[NB_SOUS]  = { 20, 30, 10 };    /* duree de chaque sous-tache */

/* ---- Modele IA : regression logistique (poids exacts) ---- */
static double predict_risk(double retard, double nb_corr, double restant,
                           double deplac, double secu) {
    double z = 1.9032
             + 0.0127 * retard
             + 0.2752 * nb_corr
             - 0.5139 * restant
             + 0.5004 * deplac
             + 0.5364 * secu;
    return 1.0 / (1.0 + exp(-z));
}

static const char *niveau(double p) {
    if (p >= 0.70) return "URGENT";
    if (p >= 0.40) return "WARNING";
    return "NORMAL";
}

/* ---- Temps ---- */
static double maintenant_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static void travailler(int ms) {          /* simule un calcul de ms millisecondes */
    double fin = maintenant_ms() + ms;
    while (maintenant_ms() < fin) { }
}

/*
 * Execute les 3 vols selon le mode. Remplit resp[] (temps de reponse de
 * chaque vol, en ms, depuis t0). En mode IA, l'inference est comprise
 * dans le temps mesure. Retourne le temps d'inference (ms) en mode IA.
 */
static double executer(int mode, double resp[NB_VOLS], int verbose) {
    int prio[NB_VOLS];                    /* 0 = le plus prioritaire */
    int suivant[NB_VOLS] = {0};           /* prochaine sous-tache de chaque vol */
    int ev_vol[NB_VOLS * NB_SOUS], ev_sous[NB_VOLS * NB_SOUS];
    double ev_fin[NB_VOLS * NB_SOUS];
    double t_ia = 0;
    double t0 = maintenant_ms();

    if (mode == MODE_IA) {
        for (int i = 0; i < NB_VOLS; i++)           /* 1) prediction pour chaque vol */
            vols[i].risque = predict_risk(vols[i].retard, vols[i].nb_corr,
                                          vols[i].restant, vols[i].deplac, vols[i].secu);
        for (int i = 0; i < NB_VOLS; i++) {         /* 2) priorite = rang du risque */
            prio[i] = 0;
            for (int j = 0; j < NB_VOLS; j++)
                if (vols[j].risque > vols[i].risque) prio[i]++;
        }
        t_ia = maintenant_ms() - t0;
    } else {
        for (int i = 0; i < NB_VOLS; i++) prio[i] = i;   /* ordre d'arrivee */
    }

    /* Ordonnanceur : a chaque etape, sous-tache du vol le plus prioritaire */
    for (int n = 0; n < NB_VOLS * NB_SOUS; n++) {
        int v = -1;
        for (int i = 0; i < NB_VOLS; i++)
            if (suivant[i] < NB_SOUS && (v < 0 || prio[i] < prio[v])) v = i;

        int k = suivant[v];
        travailler(duree_ms[k]);
        ev_vol[n] = v; ev_sous[n] = k; ev_fin[n] = maintenant_ms() - t0;

        if (++suivant[v] == NB_SOUS) resp[v] = ev_fin[n];   /* vol termine */
    }

    if (verbose) {
        printf("\n--- Mode %s : trace d'execution ---\n",
               mode == MODE_FIXE ? "PRIORITE FIXE" : "PRIORITE IA");
        if (mode == MODE_IA)
            printf("Inference IA (3 vols) : %.3f ms\n", t_ia);
        for (int n = 0; n < NB_VOLS * NB_SOUS; n++)
            printf("  [%6.1f ms] %s : %s (%d ms)\n", ev_fin[n],
                   vols[ev_vol[n]].nom, noms_sous[ev_sous[n]], duree_ms[ev_sous[n]]);
    }
    return t_ia;
}

int main(void) {
    double resp[2][NB_VOLS] = {{0}}, moy[2][NB_VOLS] = {{0}};
    double moy_ia = 0;
    double r[NB_VOLS];

    /* Scenario : entrees et risque calcule par l'IA */
    printf("SCENARIO : 3 vols arrivent en meme temps (t = 0)\n");
    printf("Vol    retard  pax  restant  deplac  secu   risque  niveau\n");
    for (int i = 0; i < NB_VOLS; i++) {
        vols[i].risque = predict_risk(vols[i].retard, vols[i].nb_corr,
                                      vols[i].restant, vols[i].deplac, vols[i].secu);
        printf("%s  %5.0f  %3.0f  %6.0f  %6.0f  %4.0f  %5.1f%%  %s\n", vols[i].nom,
               vols[i].retard, vols[i].nb_corr, vols[i].restant, vols[i].deplac,
               vols[i].secu, 100.0 * vols[i].risque, niveau(vols[i].risque));
    }
    printf("Sous-taches par vol : %s %d ms, %s %d ms, %s %d ms (total %d ms)\n",
           noms_sous[0], duree_ms[0], noms_sous[1], duree_ms[1], noms_sous[2], duree_ms[2],
           duree_ms[0] + duree_ms[1] + duree_ms[2]);

    /* Une execution avec trace pour chaque mode */
    executer(MODE_FIXE, resp[MODE_FIXE], 1);
    executer(MODE_IA,   resp[MODE_IA],   1);

    /* Moyenne sur NB_ESSAIS executions (sans trace) */
    for (int e = 0; e < NB_ESSAIS; e++) {
        executer(MODE_FIXE, r, 0);
        for (int i = 0; i < NB_VOLS; i++) moy[MODE_FIXE][i] += r[i] / NB_ESSAIS;
        moy_ia += executer(MODE_IA, r, 0) / NB_ESSAIS;
        for (int i = 0; i < NB_VOLS; i++) moy[MODE_IA][i] += r[i] / NB_ESSAIS;
    }

    /* Tableau comparatif */
    int ratees[2] = {0, 0};
    printf("\n==== COMPARAISON (moyenne de %d executions, echeance %.0f ms) ====\n",
           NB_ESSAIS, DEADLINE_MS);
    printf("Vol    Risque  | Fixe : reponse  echeance | IA : reponse  echeance | Gain\n");
    for (int i = 0; i < NB_VOLS; i++) {
        int rf = moy[MODE_FIXE][i] > DEADLINE_MS, ri = moy[MODE_IA][i] > DEADLINE_MS;
        double gain = moy[MODE_FIXE][i] - moy[MODE_IA][i];
        if (fabs(gain) < 0.05) gain = 0.0;               /* evite l'affichage "-0.0" */
        ratees[MODE_FIXE] += rf;
        ratees[MODE_IA]   += ri;
        printf("%s  %5.1f%%  | %9.1f ms   %-6s     | %7.1f ms   %-6s    | %+7.1f ms\n",
               vols[i].nom, 100.0 * vols[i].risque,
               moy[MODE_FIXE][i], rf ? "RATEE" : "OK",
               moy[MODE_IA][i],   ri ? "RATEE" : "OK",
               gain);
    }
    double fixe701 = moy[MODE_FIXE][NB_VOLS - 1], ia701 = moy[MODE_IA][NB_VOLS - 1];
    printf("\nGain sur TU701 (vol critique) : %.1f ms (%.0f %% plus rapide)\n",
           fixe701 - ia701, 100.0 * (fixe701 - ia701) / fixe701);
    printf("Cout de l'inference IA : %.3f ms (negligeable)\n", moy_ia);
    printf("Echeances ratees : Fixe = %d, IA = %d (meme charge totale : %d ms)\n",
           ratees[MODE_FIXE], ratees[MODE_IA], NB_VOLS * (duree_ms[0] + duree_ms[1] + duree_ms[2]));
    printf("=> L'IA change QUEL vol est servi en premier ; elle ne cree pas de capacite.\n");
    return 0;
}
