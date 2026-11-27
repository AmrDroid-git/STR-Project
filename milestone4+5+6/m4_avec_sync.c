/*
 * m4_avec_sync.c - Milestone 4 : reservation de porte AVEC synchronisation
 *
 * Meme scenario que m4_sans_sync.c : Porte 1 HS, Porte 2 et Porte 3 libres,
 * Avion A et Avion B demandent une porte en meme temps.
 *
 * Deux mecanismes :
 *  - semaphore compteur sem_portes_libres = nombre de portes libres
 *    (un vol qui n'en obtient pas sait qu'il n'y a plus de porte)
 *  - mutex "verrou" = section critique : chercher + reserver une porte
 *    se fait d'un seul bloc, personne ne peut s'intercaler
 *
 * Compilation : gcc -O0 -Wall -pthread m4_avec_sync.c -o m4_avec_sync
 * Execution   : ./m4_avec_sync
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define NB_PORTES 3
#define NB_VOLS   2      /* mettre 3 pour ajouter Avion C (aucune porte libre) */
#define NB_ESSAIS 5

enum { LIBRE = 0, OCCUPEE = 1, HS = 2 };

static const char *noms_portes[NB_PORTES] = {"Porte 1", "Porte 2", "Porte 3"};
static const char *noms_vols[3] = {"Avion A", "Avion B", "Avion C"};

static int etat[NB_PORTES];                 /* ressource partagee */
static int choix[NB_VOLS];                  /* porte obtenue par chaque vol */
static pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem_portes_libres;

static void init_portes(void) {
    etat[0] = HS;       /* Porte 1 indisponible */
    etat[1] = LIBRE;
    etat[2] = LIBRE;
    sem_init(&sem_portes_libres, 0, 2);     /* 2 portes libres : Porte 2 et Porte 3 */
}

/* Thread : un vol demande une porte */
static void *demander_porte(void *arg) {
    int id = *(int *)arg;
    usleep(200 * id);                       /* chaque vol arrive 0,2 ms apres le precedent */

    /* Y a-t-il encore au moins une porte libre ? (sans bloquer) */
    if (sem_trywait(&sem_portes_libres) != 0) {
        printf("  %s : aucune porte libre -> file d'attente\n", noms_vols[id]);
        choix[id] = -1;
        return NULL;
    }

    pthread_mutex_lock(&verrou);            /* ---- debut section critique ---- */

    int i = 0;
    while (i < NB_PORTES && etat[i] != LIBRE) {
        printf("  %s : %s non disponible (%s), suivante\n", noms_vols[id], noms_portes[i],
               etat[i] == HS ? "HS" : "occupee");
        i++;
    }
    /* Grace au semaphore, une porte libre existe forcement ici. */
    printf("  %s : %s est libre\n", noms_vols[id], noms_portes[i]);
    usleep(1000);                           /* meme delai de 1 ms que dans la version sans sync */
    etat[i] = OCCUPEE;
    choix[id] = i;
    printf("  %s : reserve %s\n", noms_vols[id], noms_portes[i]);

    pthread_mutex_unlock(&verrou);          /* ---- fin section critique ---- */
    return NULL;
}

int main(void) {
    int conflits = 0;

    for (int essai = 1; essai <= NB_ESSAIS; essai++) {
        pthread_t th[NB_VOLS];
        int ids[NB_VOLS];

        init_portes();
        printf("--- Essai %d (AVEC synchronisation) ---\n", essai);

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
        if (!conflit) printf("  => OK : aucune porte attribuee deux fois\n");
        conflits += conflit;
        sem_destroy(&sem_portes_libres);
    }

    printf("\nResultat : %d conflit(s) sur %d essais\n", conflits, NB_ESSAIS);
    return 0;
}
