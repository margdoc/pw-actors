#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cacti.h"
#include "err.h"
#include "utils.h"

#define MILI_TO_MICRO 1000

/*
 * Interakcja między aktorami:
 * Aktorzy tworzą się rekurencyjnie, tzn. pierwszy tworzy drugiego, drugi trzeciego, itd.
 * Po utworzeniu wszystkich aktorów, ostatni aktor wysyła do pierwszego MSG_START_COUNTING.
 * Pierwszy aktor wysyła do siebie n wiadomości MSG_COUNT odpowiadających wierszom.
 * Aktor po obliczeniu elementu wysyła MSG_COUNT kolejnemu aktorowi.
 * Po wykonaniu n obliczeń, aktor wysyła do siebie MSG_GODIE.
 */

typedef long sum_t;
typedef int value_t;;
typedef int num_t;
typedef int microseconds_t;

typedef struct element {
    value_t value;
    microseconds_t time;
} element_t;

#define MSG_FIRST_ACTOR (message_type_t) 0x1
#define MSG_READY (message_type_t) 0x2
#define MSG_SPAWN_ACTORS (message_type_t) 0x3
#define MSG_START_COUNTING (message_type_t) 0x4
#define MSG_COUNT (message_type_t) 0x5

typedef struct matrix_info {
    element_t **matrix;
    sum_t *output;
    num_t n, k;
} matrix_info_t;

typedef struct actor_state {
    element_t *column_elements;
    num_t column;
    num_t remaining_elements;
    num_t remaining_actors;
    actor_id_t next_actor;
    actor_id_t first;
    matrix_info_t *matrix_info;
} actor_state_t;

typedef struct msg_spawn_actors {
    matrix_info_t *matrix_info;
    num_t column;
    num_t remaining;
    actor_id_t first;
    num_t n;
} msg_spawn_actors_t;

typedef struct msg_count {
    num_t row;
    num_t sum;
    sum_t *output;
} msg_count_t;

void hello(actor_state_t **stateptr, size_t nbytes, void *data);

void first_actor(actor_state_t **stateptr, size_t nbytes, matrix_info_t *matrix_info);

void ready(actor_state_t **stateptr, size_t nbytes, void *data);

void spawn_actors(actor_state_t **stateptr, size_t nbytes, msg_spawn_actors_t *data);

void start_counting(actor_state_t **stateptr, size_t nbytes, void *data);

void count(actor_state_t **stateptr, size_t nbytes, msg_count_t *data);

role_t role = {
        .nprompts = 6,
        .prompts = (act_t[6]) {
                (act_t) hello,
                (act_t) first_actor,
                (act_t) ready,
                (act_t) spawn_actors,
                (act_t) start_counting,
                (act_t) count
        }
};

message_t msg_spawn = {MSG_SPAWN, sizeof(role_t), &role};
message_t msg_godie = {MSG_GODIE, sizeof(NULL), NULL};

void hello(UNUSED actor_state_t **stateptr, UNUSED size_t nbytes, void *data) {
    actor_id_t actor_id = (actor_id_t) data;

    if (actor_id == -1) {
        return;
    }

    message_t message = {
            MSG_READY, sizeof(actor_id_t), (void *) actor_id_self()
    };

    send_message(actor_id, message);
}

void first_actor(actor_state_t **stateptr, UNUSED size_t nbytes, matrix_info_t *matrix_info) {
    malloc_and_check(*stateptr, sizeof(actor_state_t));
    actor_state_t *state = *stateptr;
    state->matrix_info = matrix_info;

    actor_id_t actor_id = actor_id_self();
    msg_spawn_actors_t *msg_spawn_actors;
    malloc_and_check(msg_spawn_actors, sizeof(msg_spawn_actors_t));
    msg_spawn_actors->matrix_info = matrix_info;
    msg_spawn_actors->column = 0;
    msg_spawn_actors->remaining = matrix_info->k - 1;
    msg_spawn_actors->first = actor_id;
    msg_spawn_actors->n = matrix_info->n;

    send_message(actor_id, (message_t) {
            MSG_SPAWN_ACTORS,
            sizeof(msg_spawn_actors_t),
            msg_spawn_actors
    });
}

void ready(actor_state_t **stateptr, UNUSED size_t nbytes, void *data) {
    actor_id_t actor_id = (actor_id_t) data;

    actor_state_t *state = *stateptr;
    state->next_actor = actor_id;

    if (state->remaining_actors == 0) {
        send_message(state->first, (message_t) {
                MSG_START_COUNTING, sizeof(NULL), NULL
        });
        return;
    }

    msg_spawn_actors_t *msg_spawn_actors;
    malloc_and_check(msg_spawn_actors, sizeof(msg_spawn_actors_t));
    msg_spawn_actors->matrix_info = state->matrix_info;
    msg_spawn_actors->column = state->column + 1;
    msg_spawn_actors->remaining = state->remaining_actors - 1;
    msg_spawn_actors->first = state->first;
    msg_spawn_actors->n = state->remaining_elements;

    send_message(actor_id, (message_t) {
            MSG_SPAWN_ACTORS,
            sizeof(msg_spawn_actors_t),
            msg_spawn_actors
    });
}

void spawn_actors(actor_state_t **stateptr, UNUSED size_t nbytes, msg_spawn_actors_t *data) {
    if (*stateptr == NULL) {
        malloc_and_check(*stateptr, sizeof(actor_state_t));
    }

    actor_state_t *state = *stateptr;

    state->matrix_info = data->matrix_info;
    state->column_elements = *(data->matrix_info->matrix + (data->matrix_info->k - data->remaining) - 1);
    state->column = data->column;
    state->remaining_actors = data->remaining;
    state->first = data->first;
    state->remaining_elements = data->n;

    if (state->remaining_actors == 0) {
        send_message(state->first, (message_t) {
                MSG_START_COUNTING, sizeof(NULL), NULL
        });
    }
    else {
        send_message(actor_id_self(), msg_spawn);
    }
    free(data);
}

void start_counting(actor_state_t **stateptr, UNUSED size_t nbytes, UNUSED void *data) {
    actor_state_t *state = *stateptr;

    for (num_t i = 0; i < state->matrix_info->n; ++i) {
        msg_count_t *msg_count;
        malloc_and_check(msg_count, sizeof(msg_count_t));
        msg_count->row = i;
        msg_count->sum = 0;
        msg_count->output = state->matrix_info->output + i;

        send_message(actor_id_self(), (message_t) {
                MSG_COUNT,
                sizeof(msg_count_t),
                msg_count
        });
    }
}

void count(actor_state_t **stateptr, UNUSED size_t nbytes, msg_count_t *data) {
    actor_state_t *state = *stateptr;

    element_t *element = state->column_elements + data->row;
    usleep(MILI_TO_MICRO * element->time);
    data->sum += element->value;

    if (state->remaining_actors == 0) {
        *data->output = data->sum;
        free(data);
    }
    else {
        send_message(state->next_actor, (message_t) {
                MSG_COUNT, sizeof(msg_count_t), data
        });
    }

    state->remaining_elements--;

    if (state->remaining_elements == 0) {
        send_message(actor_id_self(), msg_godie);
        free(state);
        return;
    }
}

int main() {
    int err;

    num_t n, k;
    scanf("%d%d", &n, &k);
    element_t *matrix[k];

    for (num_t i = 0; i < k; ++i) {
        malloc_and_check(matrix[i],n * sizeof(element_t));
    }

    for (num_t i = 0; i < n; ++i) {
        for (num_t j = 0; j < k; ++j) {
            scanf("%d%d", &matrix[j][i].value, &matrix[j][i].time);
        }
    }

    actor_id_t actor_id;

    if ((err = actor_system_create(&actor_id, &role)) != 0) {
        syserr(err, "actor system create failed");
    }

    sum_t outputs[n];
    matrix_info_t matrix_info = {
            .matrix = matrix,
            .output = outputs,
            .k = k,
            .n = n
    };

    send_message(actor_id, (message_t) {
        MSG_FIRST_ACTOR, sizeof(matrix_info_t), &matrix_info
    });

    actor_system_join(actor_id);

    for (num_t i = 0; i < n; ++i) {
        printf("%ld\n", outputs[i]);
    }

    for (num_t i = 0; i < k; ++i) {
        free(matrix[i]);
    }

    return 0;
}
