#ifndef ACTOR_H
#define ACTOR_H

#include <stdbool.h>

#include "cacti.h"
#include "queue_message.h"

#ifndef ACTOR_QUEUE_LIMIT
#define ACTOR_QUEUE_LIMIT 1024
#endif

#define STARTING_ACTORS_COUNT 4

/*
 * Enumerator informacji o stanie aktora.
 */
typedef enum state {
    WORKING,
    WAITING,
    IDLING
} state_t;

/*
 * Struktura przechowująca informacje o aktorze.
 */
typedef struct actor {
    const role_t *role;
    state_t state;
    void *data;
    queue_message_t msg_queue;
    bool is_active;
    pthread_mutex_t lock;
} actor_t;

/*
 * Id jest przypisywane aktorom od 1, w kolejności ich tworzenia.
 */
typedef long actor_id_t;

/*
 * Struktura przechowująca tablicę aktorów.
 */
typedef struct actors_array {
    unsigned int nactors;
    unsigned int max_actors;
    actor_t **actors;
    pthread_rwlock_t rwlock;
} actors_array_t;

/*
 * Funkcja inicjuje aktora.
 */
void actor_init(actor_t *actor, const role_t *role);

/*
 * Funkcja niszczy aktora.
 */
void actor_destroy(actor_t *actor);

/*
 * Funkcja powoduje przejście aktora w stan martwy.
 */
void actor_godie(actor_t *actor);

/*
 * Funkcja inicjuje tablicę aktorów.
 */
void actors_array_init(actors_array_t *array);

/*
 * Funkcja niszczy tablicę aktorów.
 */
void actors_array_destroy(actors_array_t *array);

/*
 * Funkcja tworzy nowego aktora o podanej roli, dodaje do tablicy i zwraca jego id.
 * Funkcja powinna mieć ochronę pisarza.
 */
actor_id_t actors_array_new_actor(actors_array_t *array, const role_t *role);

/*
 * Funkcja zwraca wskaźnik na aktora o podanym id (NULL jeśli nie istnieje).
 * Funkcja powinna mieć ochronę czytelnika.
 */
actor_t *actors_array_get_actor(actors_array_t *array, actor_id_t actor_id);

/*
 * Funkcja powoduje przejście wszystkich aktorów w stan martwy.
 */
void actors_array_godie(actors_array_t *array);

#endif //ACTOR_H
