#include <stdio.h>
#include <threads.h>

#include "BLQueue.h"
#include "HazardPointer.h"
#include "LLQueue.h"
#include "SimpleQueue.h"
#include "RingsQueue.h"


#define NUM_THREADS 2
#define WRITERS 7
#define READERS 7
#define ELEMENTS 100
#define EMPTYNESS_CHECKERS 10

_Atomic(int) counter = 0;

// A structure holding function pointers to methods of some queue type.
struct QueueVTable {
    const char* name;
    void* (*new)(void);
    void (*push)(void* queue, Value item);
    Value (*pop)(void* queue);
    bool (*is_empty)(void* queue);
    void (*delete)(void* queue);
};
typedef struct QueueVTable QueueVTable;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

const QueueVTable queueVTables[] = {
    { "SimpleQueue", SimpleQueue_new, SimpleQueue_push, SimpleQueue_pop, SimpleQueue_is_empty, SimpleQueue_delete },
    { "LLQueue", LLQueue_new, LLQueue_push, LLQueue_pop, LLQueue_is_empty, LLQueue_delete },
    { "BLQueue", BLQueue_new, BLQueue_push, BLQueue_pop, BLQueue_is_empty, BLQueue_delete },
    { "RingsQueue", RingsQueue_new, RingsQueue_push, RingsQueue_pop, RingsQueue_is_empty, RingsQueue_delete }
};

#pragma GCC diagnostic pop

void* queue;

QueueVTable Q;

void* emptyness_checker(void* arg)
{
    int id = (int)arg;
    int emptyness_counter = 0;
    HazardPointer_register(id + WRITERS + READERS, WRITERS + READERS + EMPTYNESS_CHECKERS);
    while (atomic_load(&counter) < ELEMENTS) {
        bool is_empty = Q.is_empty(queue);
        if (is_empty) {
            emptyness_counter++;
        }
    }
    return (void*)(long)emptyness_counter;
}


void* writer(void* arg)
{
    int id = (int)arg;
    
    HazardPointer_register(id, WRITERS + READERS + EMPTYNESS_CHECKERS);
    for (int i = 0; i < ELEMENTS; ++i) {
        
        Q.push(queue, id*WRITERS + i + 1);
        atomic_fetch_add(&counter, 1);
    }


    return NULL;
}

void *reader(void *arg)
{
    int id = (int)arg;
    
    HazardPointer_register(WRITERS + id, WRITERS + READERS + EMPTYNESS_CHECKERS);
    int my_counter = 0;
    int anwsers[WRITERS][ELEMENTS];
    int indexes[WRITERS];
    for (int i = 0; i < WRITERS; ++i) {
        indexes[i] = 0;
    }
    while (my_counter < ELEMENTS * WRITERS || atomic_load(&counter) < ELEMENTS * WRITERS) {
        Value item = Q.pop(queue);
        if (item != EMPTY_VALUE) {
            //printf("%lu\n", item);
            int writer_id = item / WRITERS;
            item = item % WRITERS;
            //printf("writer_id: %d, item: %d\n", writer_id, item);
            anwsers[writer_id][indexes[writer_id]] = item;
            indexes[writer_id]++;
            my_counter++;
        }
    }

    for (int i = 0; i < WRITERS; ++i) {
        for (int j = 1; j < ELEMENTS && anwsers[i][j]; ++j) {
            if (anwsers[i][j] < anwsers[i][j-1]) {
                printf("not sorted\n");
                return NULL;
            }
        }
    }
    printf("sorted\n");

    return NULL;
}

void basic_test()
{
    // run threads
    thrd_t write_threads[WRITERS];
    thrd_t read_threads[READERS];
    thrd_t emptyness_checker_threads[EMPTYNESS_CHECKERS];
    int emptyness_checkers_results[EMPTYNESS_CHECKERS]; 
    queue = Q.new();
    for (int i = 0; i < EMPTYNESS_CHECKERS; ++i) {
        emptyness_checkers_results[i] = 0;
        thrd_create(&emptyness_checker_threads[i], emptyness_checker, (void*)(long)i);
    }
    for (int i = 0; i < READERS; ++i) {
        thrd_create(&read_threads[i], reader, (void*)i);
    }
    
    for (int i = 0; i < WRITERS; ++i) {
        thrd_create(&write_threads[i], writer, (void*)i);
    }
     
    for (int i = 0; i < READERS; ++i) {
        thrd_join(read_threads[i], NULL);
    }
    for (int i = 0; i < WRITERS; ++i) {
        thrd_join(write_threads[i], NULL);
    }
    for (int i = 0; i < EMPTYNESS_CHECKERS; ++i) {
        thrd_join(emptyness_checker_threads[i], (void**)&emptyness_checkers_results[i]);
    }
    Q.delete(queue);
    for (int i = 0; i < EMPTYNESS_CHECKERS; ++i) {
        printf("%d ", emptyness_checkers_results[i]);
    }
    printf("\n");

}

int main(void)
{
    printf("Hello, World!\n");

    for (int i = 0; i < sizeof(queueVTables) / sizeof(QueueVTable); ++i) {
        Q = queueVTables[i];
        printf("Queue type: %s\n", Q.name);
        basic_test(Q);
    }

    return 0;
}
