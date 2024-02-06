#include <stdio.h>
#include <threads.h>

#include "BLQueue.h"
#include "HazardPointer.h"
#include "LLQueue.h"
#include "SimpleQueue.h"
#include "RingsQueue.h"


#define NUM_THREADS 2
#define WRITERS 7
#define ELEMENTS 5

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

void* writer(void* arg)
{
    int id = (int)arg;
    
    HazardPointer_register(id, WRITERS + 1);
    for (int i = 0; i < ELEMENTS; ++i) {
        
        Q.push(queue, id*WRITERS + i + 1);
        atomic_fetch_add(&counter, 1);
    }


    return NULL;
}

void *reader(void *arg)
{
    
    HazardPointer_register(WRITERS, WRITERS + 1);
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
        for (int j = 0; j < ELEMENTS; ++j) {
            printf("%d ", anwsers[i][j]);
        }
        printf("\n");
    }

    return NULL;
}

void basic_test()
{
    // run threads
    thrd_t write_threads[WRITERS];
    thrd_t read_thread;
    queue = Q.new();
    thrd_create(&read_thread, reader, NULL);  
    for (int i = 0; i < WRITERS; ++i) {
        thrd_create(&write_threads[i], writer, (void*)i);
    }
     
    thrd_join(read_thread, NULL);
    for (int i = 0; i < WRITERS; ++i) {
        thrd_join(write_threads[i], NULL);
    }
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
