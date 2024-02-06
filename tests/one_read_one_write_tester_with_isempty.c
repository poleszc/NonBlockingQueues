#include <stdio.h>
#include <threads.h>

#include "BLQueue.h"
#include "HazardPointer.h"
#include "LLQueue.h"
#include "SimpleQueue.h"
#include "RingsQueue.h"


#define NUM_THREADS 2
#define ELEMENTS 5
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
    //{ "BLQueue", BLQueue_new, BLQueue_push, BLQueue_pop, BLQueue_is_empty, BLQueue_delete },
    { "RingsQueue", RingsQueue_new, RingsQueue_push, RingsQueue_pop, RingsQueue_is_empty, RingsQueue_delete }
};

#pragma GCC diagnostic pop

void* queue;

QueueVTable Q;

void* emptyness_checker(void* arg)
{
    int id = (int)arg;
    int emptyness_counter = 0;
    HazardPointer_register(id + 2, 2 + EMPTYNESS_CHECKERS);
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
    
    HazardPointer_register(0, 2 + EMPTYNESS_CHECKERS);
    for (int i = 0; i < ELEMENTS; ++i) {
        
        Q.push(queue, i+1);
        atomic_fetch_add(&counter, 1);
    }


    return NULL;
}

void *reader(void *arg)
{
    
    HazardPointer_register(1, 2 + EMPTYNESS_CHECKERS);
    int my_counter = 0;
    while (my_counter < ELEMENTS || atomic_load(&counter) < ELEMENTS) {
        Value item = Q.pop(queue);
        if (item != EMPTY_VALUE) {
            printf("%lu\n", item);
            my_counter++;
        } 
    }

    return NULL;
}

void basic_test()
{
    // run threads
    thrd_t write_thread;
    thrd_t read_thread;
    thrd_t emptyness_checkers_threads[EMPTYNESS_CHECKERS];
    int emptyness_checkers_results[EMPTYNESS_CHECKERS];
    queue = Q.new();
    for (int i = 0; i < EMPTYNESS_CHECKERS; ++i) {
        emptyness_checkers_results[i] = 0;
        thrd_create(&emptyness_checkers_threads[i], emptyness_checker, (void*)(long)i);
    }
    thrd_create(&read_thread, reader, NULL);  
    thrd_create(&write_thread, writer, NULL);
     
    thrd_join(read_thread, NULL);
    thrd_join(write_thread, NULL);
    for (int i = 0; i < EMPTYNESS_CHECKERS; ++i) {
        thrd_join(emptyness_checkers_threads[i], (void**)&emptyness_checkers_results[i]);
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
