/*
 * m4_sans_sync.c - Milestone 4 : conflit SANS synchronisation
 *
 * Scenario : Porte 1 est HS, Porte 2 et Porte 3 sont libres.
 * Avion A et Avion B demandent une porte presque en meme temps.
 * Sans protection, les deux voient Porte 2 libre et la reservent.
 *
 * Compilation : gcc -O0 -Wall -pthread m4_sans_sync.c -o m4_sans_sync
 * Execution   : ./m4_sans_sync
 */
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define NB_PORTES 3
#define NB_VOLS   2      /* mettre 3 pour ajouter Avion C */
#define NB_ESSAIS 5

enum { LIBRE = 0, OCCUPEE = 1, HS = 2 };

static const char *noms_portes[NB_PORTES] = {"Porte 1", "Porte 2", "Porte 3"};
static const char *noms_vols[3] = {"Avion A", "Avion B", "Avion C"};

static volatile int etat[NB_PORTES];  /* etat de chaque porte (ressource partagee) */
static int choix[NB_VOLS];            /* porte obtenue par chaque vol (-1 = aucune) */

static void init_portes(void) {
    etat[0] = HS;       /* Porte 1 indisponible */
    etat[1] = LIBRE;    /* Porte 2 libre */
    etat[2] = LIBRE;    /* Porte 3 libre */
}

/* Thread : un vol demande une porte */
static void *demander_porte(void *arg) {
    int id = *(int *)arg;
    usleep(200 * id);                 /* chaque vol arrive 0,2 ms apres le precedent */

    /* 1) chercher la premiere porte libre */
    int i = 0;
    while (i < NB_PORTES && etat[i] != LIBRE) {
        printf("  %s : %s non disponible, suivante\n", noms_vols[id], noms_portes[i]);
        i++;
    }
    if (i == NB_PORTES) {
        printf("  %s : aucune porte libre\n", noms_vols[id]);
        choix[id] = -1;
        return NULL;
    }
    printf("  %s : %s est libre\n", noms_vols[id], noms_portes[i]);

    usleep(1000);                     /* 2) delai de traitement (1 ms) = fenetre du conflit */

    etat[i] = OCCUPEE;                /* 3) reserver (trop tard : l'autre a deja choisi) */
    choix[id] = i;
    printf("  %s : reserve %s\n", noms_vols[id], noms_portes[i]);
    return NULL;
}

int main(void) {
    int conflits = 0;

    for (int essai = 1; essai <= NB_ESSAIS; essai++) {
        pthread_t th[NB_VOLS];
        int ids[NB_VOLS];

        init_portes();
        printf("--- Essai %d (SANS synchronisation) ---\n", essai);

        for (int v = 0; v < NB_VOLS; v++) {
            ids[v] = v;
            choix[v] = -1;
            pthread_create(&th[v], NULL, demander_porte, &ids[v]);
        }
        for (int v = 0; v < NB_VOLS; v++)
            pthread_join(th[v], NULL);

        /* Verification : une porte attribuee a plus d'un vol = conflit */
        int conflit = 0;
        for (int p = 0; p < NB_PORTES; p++) {
            int nb = 0;
            for (int v = 0; v < NB_VOLS; v++)
                if (choix[v] == p) nb++;
            if (nb > 1) {
                printf("  => CONFLIT : %s attribuee a %d vols\n", noms_portes[p], nb);
                conflit = 1;
            }
        }
        if (!conflit) printf("  => OK (hasard : pas de conflit cette fois)\n");
        conflits += conflit;
    }

    printf("\nResultat : %d conflit(s) sur %d essais\n", conflits, NB_ESSAIS);
    return 0;
}
