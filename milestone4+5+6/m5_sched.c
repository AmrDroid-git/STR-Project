/*
 * m5_sched.c - Milestone 5 : ordonnancement temps reel RM, DM, EDF
 *
 * Simulation pas a pas (1 tick = 1 ms), processeur unique, preemptif.
 * Politiques :
 *   RM  : priorite fixe, periode T la plus courte = priorite la plus haute
 *   DM  : priorite fixe, deadline relative D la plus courte = plus haute
 *   EDF : priorite dynamique, deadline ABSOLUE la plus proche = plus haute
 *
 * Regle : un job qui depasse sa deadline est compte comme RATE, mais il
 * continue de s'executer jusqu'a la fin (il consomme du CPU en retard).
 * La simulation dure 1 hyperperiode (ppcm des periodes).
 *
 * Compilation : gcc -O2 -Wall m5_sched.c -o m5_sched
 * Execution   : ./m5_sched
 */
#include <stdio.h>

#define MAXT    4      /* taches max par scenario */
#define MAXJOBS 256    /* jobs max par simulation */
#define MAXH    400    /* hyperperiode max (ms) */

typedef struct { const char *nom; int C, T, D; } Tache;   /* C=calcul, T=periode, D=deadline */
typedef struct { const char *titre; int n; Tache t[MAXT]; } Scenario;
typedef struct { int tache, release, deadline, reste, fin, rate; } Job;
typedef struct { int rates, total; } Resultat;

enum { RM, DM, EDF, NB_POL };
static const char *noms_pol[NB_POL] = {"RM", "DM", "EDF"};

/* Borne de Liu-Layland pour RM : n*(2^(1/n)-1), n = 1..4 */
static const double bornes_LL[MAXT + 1] = {0, 1.000, 0.828, 0.780, 0.757};

/* ---- Scenarios (unites : ms, echelle reduite pour l'analyse manuelle) ----
 * T1 = surveillance vols, T3 = prediction IA, T4 = decision, T5 = alerte
 * ({nom, C, T, D})                                                        */
static Scenario scenarios[] = {
    { "S0 - EXEMPLE MANUEL (U = 0.70, T5 avec D < T)", 3,
      { {"T1", 2, 5, 5}, {"T4", 2, 10, 10}, {"T5", 2, 20, 4} } },
    { "S1 - FAISABLE (U = 0.69)", 4,
      { {"T1", 5, 20, 20}, {"T3", 15, 100, 100}, {"T4", 10, 40, 40}, {"T5", 2, 50, 50} } },
    { "S2 - CRITIQUE (U = 0.90, T5 avec D < T)", 4,
      { {"T1", 5, 20, 20}, {"T3", 30, 100, 100}, {"T4", 10, 40, 40}, {"T5", 5, 50, 10} } },
    { "S3 - SURCHARGE (U = 1.10)", 4,
      { {"T1", 5, 20, 20}, {"T3", 50, 100, 100}, {"T4", 10, 40, 40}, {"T5", 5, 50, 10} } },
};
#define NB_SCEN ((int)(sizeof(scenarios) / sizeof(scenarios[0])))

static int pgcd(int a, int b) { return b == 0 ? a : pgcd(b, a % b); }

static int hyperperiode(const Scenario *s) {
    int h = 1;
    for (int i = 0; i < s->n; i++)
        h = h / pgcd(h, s->t[i].T) * s->t[i].T;
    return h;
}

/* Cle de priorite : PLUS PETIT = PLUS PRIORITAIRE */
static int cle(const Scenario *s, const Job *j, int pol) {
    const Tache *t = &s->t[j->tache];
    if (pol == RM) return t->T;
    if (pol == DM) return t->D;
    return j->deadline;                    /* EDF : deadline absolue */
}

/* Vrai si le job a est plus prioritaire que le job b */
static int prioritaire(const Scenario *s, const Job *a, const Job *b, int pol) {
    int ka = cle(s, a, pol), kb = cle(s, b, pol);
    if (ka != kb) return ka < kb;
    if (a->release != b->release) return a->release < b->release;   /* egalite : le plus ancien */
    return a->tache < b->tache;
}

/* Marque rate tout job dont la deadline est atteinte sans etre termine */
static void marquer_rates(Job *jobs, int nj, int t, int *rates) {
    for (int j = 0; j < nj; j++)
        if (!jobs[j].rate && jobs[j].reste > 0 && jobs[j].deadline <= t) {
            jobs[j].rate = 1;
            rates[jobs[j].tache]++;
        }
}

/* Simule une politique sur un scenario, affiche la trace et les stats */
static Resultat simuler(const Scenario *s, int pol) {
    int H = hyperperiode(s);
    Job jobs[MAXJOBS];
    int nj = 0;
    char cpu[MAXH + 1];
    int rates[MAXT] = {0}, total[MAXT] = {0};

    for (int t = 0; t < H; t++) {
        marquer_rates(jobs, nj, t, rates);                 /* 1) deadlines depassees */

        for (int i = 0; i < s->n; i++)                     /* 2) nouvelles liberations */
            if (t % s->t[i].T == 0 && nj < MAXJOBS) {
                Job nouveau = { i, t, t + s->t[i].D, s->t[i].C, -1, 0 };
                jobs[nj++] = nouveau;
                total[i]++;
            }

        int best = -1;                                     /* 3) choisir le job prioritaire */
        for (int j = 0; j < nj; j++)
            if (jobs[j].reste > 0 && (best < 0 || prioritaire(s, &jobs[j], &jobs[best], pol)))
                best = j;

        if (best >= 0) {                                   /* 4) executer 1 ms */
            cpu[t] = s->t[jobs[best].tache].nom[1];        /* '1', '3', '4' ou '5' */
            if (--jobs[best].reste == 0) jobs[best].fin = t + 1;
        } else {
            cpu[t] = '.';                                  /* processeur au repos */
        }
    }
    marquer_rates(jobs, nj, H, rates);                     /* deadlines tombant a la fin */

    /* ---- Affichage ---- */
    printf("\n--- %s ---\n", noms_pol[pol]);
    printf("CPU (1 car = 1 ms, '.' = repos, chiffre = tache en cours) :\n");
    for (int b = 0; b < H; b += 50) {
        int fin = (b + 50 < H) ? b + 50 : H;
        printf("  %3d |", b);
        for (int t = b; t < fin; t++) {
            putchar(cpu[t]);
            if ((t + 1 - b) % 10 == 0 && t + 1 < fin) putchar(' ');
        }
        printf("|\n");
    }

    int nb_aff = 0, tot_rates = 0, tot_jobs = 0;
    for (int j = 0; j < nj && nb_aff < 6; j++)
        if (jobs[j].rate) {
            printf("  RATE : %s lance a %d ms, echeance %d ms, ",
                   s->t[jobs[j].tache].nom, jobs[j].release, jobs[j].deadline);
            if (jobs[j].fin >= 0) printf("fini a %d ms\n", jobs[j].fin);
            else                  printf("pas fini a %d ms\n", H);
            nb_aff++;
        }
    printf("Tache  jobs  rates  taux de reussite\n");
    for (int i = 0; i < s->n; i++) {
        printf("  %s   %3d   %3d   %6.1f %%\n", s->t[i].nom, total[i], rates[i],
               100.0 * (total[i] - rates[i]) / total[i]);
        tot_rates += rates[i];
        tot_jobs  += total[i];
    }
    printf("Global : %d deadline(s) ratee(s) sur %d jobs -> reussite %.1f %%\n",
           tot_rates, tot_jobs, 100.0 * (tot_jobs - tot_rates) / tot_jobs);

    Resultat r = { tot_rates, tot_jobs };
    return r;
}

int main(void) {
    Resultat res[NB_SCEN][NB_POL];

    for (int k = 0; k < NB_SCEN; k++) {
        const Scenario *s = &scenarios[k];
        double U = 0, dens = 0;

        printf("\n==================================================\n");
        printf("%s\n", s->titre);
        printf("Tache    C    T    D   C/T\n");
        for (int i = 0; i < s->n; i++) {
            printf("  %s  %3d  %3d  %3d  %.2f\n", s->t[i].nom, s->t[i].C, s->t[i].T,
                   s->t[i].D, (double)s->t[i].C / s->t[i].T);
            U    += (double)s->t[i].C / s->t[i].T;
            dens += (double)s->t[i].C / s->t[i].D;
        }
        printf("U = somme Ci/Ti = %.2f | borne RM (n=%d) = %.3f | somme Ci/Di = %.2f | hyperperiode = %d ms\n",
               U, s->n, bornes_LL[s->n], dens, hyperperiode(s));

        for (int p = 0; p < NB_POL; p++)
            res[k][p] = simuler(s, p);
    }

    printf("\n==================================================\n");
    printf("RECAPITULATIF : deadlines ratees / jobs (taux de reussite)\n");
    printf("Scenario                                              RM            DM            EDF\n");
    for (int k = 0; k < NB_SCEN; k++) {
        printf("%-48s", scenarios[k].titre);
        for (int p = 0; p < NB_POL; p++)
            printf("  %2d/%-3d %5.1f%%", res[k][p].rates, res[k][p].total,
                   100.0 * (res[k][p].total - res[k][p].rates) / res[k][p].total);
        printf("\n");
    }
    return 0;
}
