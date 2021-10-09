#include "cacti.h"

#include <pthread.h>
#include <signal.h>

#include "actor.h"
#include "queue_actor_id.h"
#include "utils.h"

/*
 * Struktura przechowująca informacje o systemie aktorów.
 */
typedef struct actors_system {
    actors_array_t actors_array;
    pthread_mutex_t lock;
    unsigned int nthreads;
    pthread_t *threads;
    queue_actor_id_t waiting_actors;
    unsigned long active_actors;
    unsigned long messages;
    bool is_active;
    bool is_interrupted;
    struct sigaction previous_sigset;
} actors_system_t;

/*
 * Obecnie działający system aktorów.
 */
actors_system_t *current_actors_system;

/*
 * Funkcja powoduje przejście systemu aktorów w stan martwy.
 */
static void godie(actors_system_t *actors_system) {
    int err;

    if (actors_system == NULL) {
        return;
    }

    actors_system->is_active = false;

    queue_actor_id_t *actors_queue = &actors_system->waiting_actors;
    actors_array_t *actors_array = &actors_system->actors_array;

    entity_lock(actors_queue);
    queue_actor_id_godie(actors_queue);
    entity_unlock(actors_queue);

    entity_writer_lock(actors_array);
    actors_array_godie(actors_array);
    entity_rw_unlock(actors_array);
}

/*
 * Funkcja obsługuje sygnał SIGINT
 */
static void interrupted() {
    int err;

    entity_lock(current_actors_system);
    current_actors_system->is_interrupted = true;

    if (current_actors_system->messages == 0) {
        // Wszyscy aktorzy czekają na wiadomości
        godie(current_actors_system);
        entity_unlock(current_actors_system);
    } else {
        // Niektórzy aktorzy czekają na wiadomości
        entity_unlock(current_actors_system);

        actors_array_t *actors_array = &current_actors_system->actors_array;

        entity_reader_lock(actors_array);
        int counter = 0;
        for (actor_id_t actor_id = 1; actor_id <= actors_array->nactors; ++actor_id) {
            actor_t *actor = actors_array_get_actor(actors_array, actor_id);
            queue_message_t *messages_queue = &actor->msg_queue;

            entity_lock(actor);
            entity_lock(messages_queue);
            if (actor->is_active && queue_message_is_empty(messages_queue)) {
                actor->is_active = false;
                ++counter;
            }
            entity_unlock(messages_queue);
            entity_unlock(actor);
        }
        entity_rw_unlock(actors_array);

        entity_lock(current_actors_system);
        current_actors_system->active_actors -= counter;
        entity_unlock(current_actors_system);
    }

    actor_system_join(1);
}

/*
 * Funkcja wywołuje wiadomość na aktorze.
 */
static void execute_message(actor_id_t actor_id, message_t message) {
    int err;

    actors_array_t *actors_array = &current_actors_system->actors_array;

    entity_reader_lock(actors_array);
    actor_t *actor = actors_array_get_actor(actors_array, actor_id);
    entity_rw_unlock(actors_array);

    switch (message.message_type) {
        case MSG_SPAWN: {
            entity_lock(current_actors_system);
            if (current_actors_system->is_interrupted) {
                // System aktorów nie przyjmuje nowych aktorów.
                entity_unlock(current_actors_system);
                return;
            }
            entity_unlock(current_actors_system);

            message_t message_hello = {MSG_HELLO, sizeof(actor_id_t), (void *) actor_id};

            entity_writer_lock(actors_array);
            actor_id_t new_actor = actors_array_new_actor(actors_array, message.data);
            entity_rw_unlock(actors_array);

            entity_lock(current_actors_system);
            if (new_actor != -1) {
                current_actors_system->active_actors++;
            }
            entity_unlock(current_actors_system);

            send_message(new_actor, message_hello);
            break;
        }
        case MSG_GODIE: {
            entity_lock(actor);
            actor->is_active = false;
            entity_unlock(actor);
            break;
        }
        default: {
            actor->role->prompts[message.message_type]
                    (&actor->data, message.nbytes, message.data);
            break;
        }
    }
}

_Thread_local actor_id_t current_actor = -1;

/*
 * Funkcja obsługująca działanie wątków
 */
static void *thread_func(UNUSED void *data) {
    int err;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);

    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    queue_actor_id_t *actors_queue = &current_actors_system->waiting_actors;
    actors_array_t *actors_array = &current_actors_system->actors_array;

    while (true) {
        entity_lock(actors_queue);
        // Oczekiwanie na aktora z komunikatem.
        actor_id_t actor_id = queue_actor_id_pop(actors_queue);
        entity_unlock(actors_queue);

        entity_lock(current_actors_system);
        if (!current_actors_system->is_active) {
            // Został wybudzony po przejściu systemu w stan martwy.
            entity_unlock(current_actors_system);
            break;
        }
        entity_unlock(current_actors_system);

        entity_reader_lock(actors_array);
        actor_t *actor = actors_array_get_actor(actors_array, actor_id);
        entity_rw_unlock(actors_array);

        entity_lock(actor);
        actor->state = WORKING;
        entity_unlock(actor);

        queue_message_t *messages_queue = &actor->msg_queue;

        entity_lock(messages_queue);
        message_t message = queue_message_pop(messages_queue);
        entity_unlock(messages_queue);

        entity_lock(actor);
        if (!actor->is_active) {
            // Został wybudzony po przejściu aktora w stan martwy.
            entity_unlock(actor);
            continue;
        }
        entity_unlock(actor);

        current_actor = actor_id;

        execute_message(actor_id, message);

        current_actor = -1;

        entity_lock(current_actors_system);
        current_actors_system->messages--;
        entity_unlock(current_actors_system);

        entity_lock(actor);
        entity_lock(messages_queue);
        if (!queue_message_is_empty(messages_queue)) {
            entity_unlock(messages_queue);
            actor->state = WAITING;
            entity_unlock(actor);

            entity_lock(actors_queue);
            queue_actor_id_push(actors_queue, actor_id);
            entity_unlock(actors_queue);
        } else {
            entity_unlock(messages_queue);
            actor->state = IDLING;

            entity_lock(current_actors_system);
            if (!actor->is_active || current_actors_system->is_interrupted) {
                // Aktor przeszedł w stan martwy lub proces został przerwany
                entity_unlock(current_actors_system);
                actor_godie(actor);
                entity_unlock(actor);

                entity_lock(current_actors_system);
                current_actors_system->active_actors--;

                if (current_actors_system->active_actors == 0) {
                    godie(current_actors_system);
                }
            } else {
                entity_unlock(actor);
            }
            entity_unlock(current_actors_system);
        }
    }

    return 0;
}

/*
 * Funkcja inicjalizuje system aktorów.
 */
static void actor_system_init(actors_system_t *actors_system) {
    int err;

    actors_system->is_active = true;
    actors_system->is_interrupted = false;
    actors_system->active_actors = 0;
    actors_system->nthreads = POOL_SIZE;
    malloc_and_check(actors_system->threads, POOL_SIZE * sizeof(pthread_t));
    queue_actor_id_init(&actors_system->waiting_actors, 0);
    actors_array_init(&actors_system->actors_array);
    mutex_init(&actors_system->lock);
}

/*
 * Funkcja niszczy system aktorów.
 */
static void actor_system_destroy(actors_system_t *actors_system) {
    int err;

    free(actors_system->threads);
    queue_actor_id_destroy(&actors_system->waiting_actors);
    actors_array_destroy(&actors_system->actors_array);
    mutex_destroy(&actors_system->lock);
}

actor_id_t actor_id_self() {
    return current_actor;
}

int actor_system_create(actor_id_t *actor, role_t *const role) {
    if (current_actors_system != NULL) {
        return -1;
    }

    int err;

    malloc_and_check(current_actors_system, sizeof(actors_system_t));
    actor_system_init(current_actors_system);

    sigset_t sigset;
    sigemptyset(&sigset);

    struct sigaction action;
    action.sa_handler = interrupted;
    action.sa_mask = sigset;
    action.sa_flags = 0;
    if ((err = sigaction(SIGINT, &action, &current_actors_system->previous_sigset)) != 0)
        syserr(err, "sigaction failed");

    pthread_attr_t attr;

    actors_array_t *actors_array = &current_actors_system->actors_array;

    entity_writer_lock(actors_array);
    *actor = actors_array_new_actor(actors_array, role);
    entity_rw_unlock(actors_array);

    entity_lock(current_actors_system);
    current_actors_system->active_actors = 1;
    entity_unlock(current_actors_system);

    // Niejawne wysłanie MSG_HELLO do pierwszego aktora
    current_actor = *actor;

    execute_message(*actor, (message_t) {
            .message_type = MSG_HELLO,
            .nbytes = sizeof(actor_id_t),
            .data = (void *) -1
    });

    current_actor = -1;


    thread_attr_init(PTHREAD_CREATE_JOINABLE);

    for (unsigned int i = 0; i < current_actors_system->nthreads; ++i) {
        thread_create(current_actors_system->threads + i, thread_func);
    }

    thread_attr_destroy;

    return 0;
}

void actor_system_join(actor_id_t actor) {
    if (actor > current_actors_system->actors_array.nactors) {
        return;
    }

    int err;
    void *retval;

    for (unsigned int i = 0; i < current_actors_system->nthreads; ++i) {
        thread_join(current_actors_system->threads[i]);
    }

    if ((err = sigaction(SIGINT, &current_actors_system->previous_sigset, NULL)) != 0)
        syserr(err, "sigaction failed");

    actor_system_destroy(current_actors_system);
    free(current_actors_system);
    current_actors_system = NULL;
}

int send_message(actor_id_t actor, message_t message) {
    int err;

    entity_lock(current_actors_system);
    if (current_actors_system->is_interrupted) {
        // System aktorów nie przyjmuje już komunikatów.
        entity_unlock(current_actors_system);
        return -5;
    }
    current_actors_system->messages++;
    entity_unlock(current_actors_system);


    actors_array_t *actors_array = &current_actors_system->actors_array;

    entity_reader_lock(actors_array);
    if (actor > current_actors_system->actors_array.nactors) {
        // Brak aktora o podanym id.
        entity_rw_unlock(actors_array);

        entity_lock(current_actors_system);
        current_actors_system->messages--;
        entity_unlock(current_actors_system);
        return -2;
    }

    actor_t *actor_struct = actors_array_get_actor(actors_array, actor);
    entity_rw_unlock(actors_array);

    entity_lock(actor_struct);
    if (!actor_struct->is_active) {
        entity_unlock(actor_struct);

        entity_lock(current_actors_system);
        current_actors_system->messages--;
        entity_unlock(current_actors_system);
        return -1;
    }
    entity_unlock(actor_struct);

    queue_message_t *actor_messages = &actor_struct->msg_queue;

    entity_lock(actor_messages);
    if ((err = queue_message_push(actor_messages, message)) != 0) {
        entity_unlock(actor_messages);
        return -3;
    }
    entity_unlock(actor_messages);

    entity_lock(actor_struct);
    if (actor_struct->state == IDLING) {
        // Aktor nie miał żadnych komunikatów, więc teraz przechodzi w stan oczekiwania.
        actor_struct->state = WAITING;
        entity_unlock(actor_struct);

        queue_actor_id_t *actors_queue = &current_actors_system->waiting_actors;

        entity_lock(actors_queue);
        queue_actor_id_push(actors_queue, actor);
        entity_unlock(actors_queue);
    } else {
        entity_unlock(actor_struct);
    }

    return 0;
}