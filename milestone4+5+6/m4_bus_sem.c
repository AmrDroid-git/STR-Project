/*
 * m4_bus_sem.c - Milestone 4 : semaphore compteur = bus de transfert
 *
 * 6 groupes de 20 passagers doivent etre transportes, mais il n'y a que
 * NB_BUS bus disponibles (3 par defaut, mettre 5 pour tester).
 * Le semaphore compteur vaut le nombre de bus libres :
 *   sem_wait = prendre un bus (attend s'il n'y en a plus)
 *   sem_post = rendre le bus
 *
 * Compilation : gcc -O0 -Wall -pthread m4_bus_sem.c -o m4_bus_sem
 * Execution   : ./m4_bus_sem
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

#define NB_BUS          3     /* bus disponibles (essayer 5) */
#define NB_GROUPES      6
#define PASSAGERS       20    /* passagers par groupe */
#define DUREE_TRAJET_MS 300   /* duree d'un trajet */

static sem_t sem_bus;                       /* nombre de bus libres */
static pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;
static int bus_en_service = 0;              /* bus utilises en ce moment */
static int max_en_service = 0;              /* maximum observe (doit rester <= NB_BUS) */
static double t0;

static double maintenant_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* Thread : un groupe de passagers en correspondance */
static void *groupe(void *arg) {
    int id = *(int *)arg;

    printf("[%5.0f ms] Groupe %d (%d pax) attend un bus\n",
           maintenant_ms() - t0, id, PASSAGERS);

    sem_wait(&sem_bus);                     /* prendre un bus (bloque si aucun libre) */

    pthread_mutex_lock(&verrou);
    bus_en_service++;
    if (bus_en_service > max_en_service) max_en_service = bus_en_service;
    int n = bus_en_service;
    pthread_mutex_unlock(&verrou);
    printf("[%5.0f ms] Groupe %d monte dans un bus (bus utilises : %d/%d)\n",
           maintenant_ms() - t0, id, n, NB_BUS);

    usleep(DUREE_TRAJET_MS * 1000);         /* trajet jusqu'a la porte */

    pthread_mutex_lock(&verrou);
    bus_en_service--;
    pthread_mutex_unlock(&verrou);
    printf("[%5.0f ms] Groupe %d arrive, bus libere\n", maintenant_ms() - t0, id);

    sem_post(&sem_bus);                     /* rendre le bus */
    return NULL;
}

int main(void) {
    pthread_t th[NB_GROUPES];
    int ids[NB_GROUPES];

    sem_init(&sem_bus, 0, NB_BUS);
    t0 = maintenant_ms();

    for (int g = 0; g < NB_GROUPES; g++) {
        ids[g] = g + 1;
        pthread_create(&th[g], NULL, groupe, &ids[g]);
    }
    for (int g = 0; g < NB_GROUPES; g++)
        pthread_join(th[g], NULL);

    printf("\nMax de bus utilises en meme temps : %d (limite %d) -> %s\n",
           max_en_service, NB_BUS, max_en_service <= NB_BUS ? "OK" : "ERREUR");
    printf("Duree totale : %.0f ms (sequentiel : %d ms)\n",
           maintenant_ms() - t0, NB_GROUPES * DUREE_TRAJET_MS);

    sem_destroy(&sem_bus);
    return 0;
}
