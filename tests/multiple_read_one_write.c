#include <stdio.h>
#include <threads.h>

#include "BLQueue.h"
#include "HazardPointer.h"
#include "LLQueue.h"
#include "SimpleQueue.h"
#include "RingsQueue.h"


#define NUM_THREADS 2
#define READERS 7
#define ELEMENTS 100

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

_Atomic(int) my_counter = 0;

// declare counter mutexes

QueueVTable Q;

void* writer(void* arg)
{
    
    HazardPointer_register(READERS, READERS + 1);
    for (int i = 0; i < ELEMENTS; ++i) {
        
        Q.push(queue, i+1);
        atomic_fetch_add(&counter, 1);
    }


    return NULL;
}

void *reader(void *arg)
{
    int id = (int)arg;
    
    HazardPointer_register(id, READERS + 1);
    int popped[ELEMENTS];
    for (int i = 0; i < ELEMENTS; ++i) {
        popped[i] = 0;
    }
    while (atomic_load(&my_counter) < ELEMENTS || atomic_load(&counter) < ELEMENTS) {
        Value item = Q.pop(queue);
        if (item != EMPTY_VALUE) {
            //printf("%lu\n", item);
            //my_counter++;
            int idx = atomic_fetch_add(&my_counter, 1);
            popped[idx] = item;
        }
    }
    for (int i = 1; i < ELEMENTS && popped[i]; ++i) {
        // check if popped is sorted
        if (popped[i-1] > popped[i]) {
            printf("not sorted\n");
            return NULL;
        }
    }
    printf("sorted\n");

    return NULL;
}

void basic_test()
{
    // run threads
    thrd_t write_thread;
    thrd_t read_thread;
    queue = Q.new();
    for (int i = 0; i < READERS; ++i) {
        thrd_create(&read_thread, reader, (void*)i);
    }
    thrd_create(&write_thread, writer, NULL);
     
    for (int i = 0; i < READERS; ++i) {
        thrd_join(read_thread, NULL);
    }
    thrd_join(write_thread, NULL);
    Q.delete(queue);
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
