#include "actor.h"

#include <stdlib.h>

#include "utils.h"

void actor_init(actor_t *actor, const role_t *role) {
    int err;

    actor->is_active = true;
    actor->role = role;
    actor->state = IDLING;
    queue_message_init(&actor->msg_queue, ACTOR_QUEUE_LIMIT);
    mutex_init(&actor->lock);
    actor->data = NULL;
}

void actor_destroy(actor_t *actor) {
    int err;

    queue_message_destroy(&actor->msg_queue);
    mutex_destroy(&actor->lock);
}

void actor_godie(actor_t *actor) {
    queue_message_godie(&actor->msg_queue);
}

void actors_array_init(actors_array_t *array) {
    int err;

    array->nactors = 0;
    array->max_actors = STARTING_ACTORS_COUNT;
    malloc_and_check(array->actors, STARTING_ACTORS_COUNT * sizeof(actor_t));
    rwlock_init(&array->rwlock);
}

void actors_array_destroy(actors_array_t *array) {
    int err;

    rwlock_destroy(&array->rwlock);

    for (unsigned int i = 0; i < array->nactors && array->actors[i] != NULL; ++i) {
        actor_destroy(array->actors[i]);
        free(array->actors[i]);
    }

    free(array->actors);
}

actor_id_t actors_array_new_actor(actors_array_t *array, const role_t *role) {
    if (array->nactors == array->max_actors) {
        if (array->max_actors * 2 <= CAST_LIMIT) {
            array->max_actors *= 2;
        } else {
            return -1;
        }

        realloc_and_check(array->actors, array->max_actors * sizeof(actor_t));
    }

    ++array->nactors;

    malloc_and_check(array->actors[array->nactors - 1], sizeof(actor_t));
    actor_init(array->actors[array->nactors - 1], role);
    actor_id_t output = array->nactors;

    return output;
}

actor_t *actors_array_get_actor(actors_array_t *array, actor_id_t actor_id) {
    if (actor_id > array->nactors || actor_id <= 0) {
        return NULL;
    }

    return array->actors[actor_id - 1];
}

void actors_array_godie(actors_array_t *array) {
    for (unsigned int i = 0; i < array->nactors && array->actors[i] != NULL; ++i) {
        actor_godie(array->actors[i]);
    }
}