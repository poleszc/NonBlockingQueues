#include <stdio.h>
#include <threads.h>

#include "../BLQueue.h"
#include "../HazardPointer.h"
#include "../LLQueue.h"
#include "../RingsQueue.h"
#include "../SimpleQueue.h"
#include "err.h"

#define NUM_THREADS 2
#define ELEMENTS 14444

_Atomic(int) counter = 0;

// A structure holding function pointers to methods of some queue type.
struct QueueVTable {
    const char *name;
    void *(*new)(void);
    void (*push)(void *queue, Value item);
    Value (*pop)(void *queue);
    bool (*is_empty)(void *queue);
    void (*delete)(void *queue);
};
typedef struct QueueVTable QueueVTable;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

const QueueVTable queueVTables[] = {
    // {"SimpleQueue", SimpleQueue_new, SimpleQueue_push, SimpleQueue_pop, SimpleQueue_is_empty, SimpleQueue_delete},
    // {"RingsQueue", RingsQueue_new, RingsQueue_push, RingsQueue_pop, RingsQueue_is_empty, RingsQueue_delete},
    {"LLQueue", LLQueue_new, LLQueue_push, LLQueue_pop, LLQueue_is_empty, LLQueue_delete}
    // {"BLQueue", BLQueue_new, BLQueue_push, BLQueue_pop, BLQueue_is_empty, BLQueue_delete},
};

#pragma GCC diagnostic pop

void *queue;

QueueVTable Q;

int writer(void *arg) {

    HazardPointer_register(0, 2);
    for (int i = 0; i < ELEMENTS; ++i) {

        Q.push(queue, i + 1);
        //        atomic_fetch_add(&counter, 1);
    }

    return 0;
}

int reader(void *arg) {

    HazardPointer_register(1, 2);
    int my_counter = 0;
    int old = -1;
    //    while (my_counter < ELEMENTS || atomic_load(&counter) < ELEMENTS) {
    while (my_counter < ELEMENTS) {
        Value item = Q.pop(queue);
        if (item != EMPTY_VALUE) {
            //            printf("%lu\n", item);
            if (old > my_counter) {
                printf("zle posortowane\n");
                return 1;
            }
            old = my_counter;
            my_counter++;
        }
    }

    return 0;
}

int basic_test() {
    //    if (DEBUG) printf("debug kolejki %s\n", Q.name); przyklad jak mozna
    //    uzywac

    // run threads
    thrd_t write_thread;
    thrd_t read_thread;
    queue = Q.new();
    DEBUG(Q.is_empty(queue));

    thrd_create(&read_thread, reader, NULL);
    thrd_create(&write_thread, writer, NULL);

    int res = 0;
    thrd_join(read_thread, &res);
    thrd_join(write_thread, &res);
    DEBUG(Q.is_empty(queue));

    Q.delete(queue);
    if (res == 0) {
        printf("OK %s\n", Q.name);
        return 0;
    } else {
        printf("ERROR %s\n", Q.name);
        return 1;
    }
}

int main(void) {
    printf("Hello, World!\n");

    int res = 0;
    for (int i = 0; i < sizeof(queueVTables) / sizeof(QueueVTable); ++i) {
        Q = queueVTables[i];
        printf("Queue type: %s\n", Q.name);
        res |= basic_test(Q);
    }

    return res;
}
