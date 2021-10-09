#include <stdio.h>
#include <stdlib.h>

#include "cacti.h"
#include "err.h"
#include "utils.h"

/*
 * Interakcja między aktorami:
 * A tworzy nowego aktora B.
 * B wysyła MSG_READY ze swoim actor_id_t (id ojca otrzymał w MSG_HELLO).
 * A dokonuje obliczeń, wysyła ich wynik do B (MSG_EXECUTE) i wysyła do siebie MSG_GODIE.
 * Trwa to do momentu policzenia silni. Wtedy aktor wywołujący MSG_EXECUTE wypisuje wynik i umiera.
 */

#define MSG_EXECUTE (message_type_t) 0x01
#define MSG_READY (message_type_t) 0x02

typedef int num_t;
typedef long fact_t;

typedef struct execute_info {
    num_t k;
    fact_t factorial;
    num_t remaining;
} execute_info_t;

void hello(execute_info_t **stateptr, size_t nbytes, void *data);

void execute(execute_info_t **stateptr, size_t nbytes, execute_info_t *data);

void ready(execute_info_t **stateptr, size_t nbytes, void *data);

role_t role = {
        .nprompts = 3,
        .prompts = (act_t[3]) {
                (act_t) hello,
                (act_t) execute,
                (act_t) ready
        }
};

message_t msg_spawn = {MSG_SPAWN, sizeof(role_t), &role};
message_t msg_godie = {MSG_GODIE, sizeof(NULL), NULL};

void hello(UNUSED execute_info_t **stateptr, UNUSED size_t nbytes, void *data) {
    actor_id_t actor_id = (actor_id_t) data;

    if (actor_id == -1) {
        return;
    }

    message_t message = {
            MSG_READY, sizeof(actor_id_t), (void *) actor_id_self()
    };

    send_message(actor_id, message);
}

void execute(execute_info_t **stateptr, UNUSED size_t nbytes, execute_info_t *data) {
    *stateptr = data;
    execute_info_t *info = *stateptr;

    if (info->remaining <= 0) {
        printf("%ld\n", info->factorial);
        send_message(actor_id_self(), msg_godie);
        return;
    }

    info->k++;
    info->factorial *= info->k;
    info->remaining--;

    send_message(actor_id_self(), msg_spawn);
}

void ready(execute_info_t **stateptr, UNUSED size_t nbytes, void *data) {
    actor_id_t actor_id = (actor_id_t) data;

    send_message(actor_id, (message_t) {MSG_EXECUTE, sizeof(execute_info_t *), *stateptr});
    send_message(actor_id_self(), msg_godie);
}

int main() {
    num_t n;

    scanf("%d", &n);

    if (n < 0) {
        fprintf(stderr, "n is not natural number\n");
        return -1;
    }

    int err;
    actor_id_t actor_id;

    if ((err = actor_system_create(&actor_id, &role)) != 0) {
        syserr(err, "actor system create failed");
    }

    execute_info_t info = (execute_info_t) {1, 1, n - 1};

    message_t message = {
            MSG_EXECUTE, sizeof(execute_info_t), &info
    };

    send_message(actor_id, message);

    actor_system_join(actor_id);

    return 0;
}
