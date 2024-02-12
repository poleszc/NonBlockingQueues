#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <threads.h>

#include "BLQueue.h"
#include "HazardPointer.h"
#include "LLQueue.h"
#include "RingsQueue.h"
#include "SimpleQueue.h"

#define ELEMENTS 300
bool added[ELEMENTS + 1];
bool freed[ELEMENTS + 1];
//  structure holding function pointers to methods of some queue type.
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
    // { "SimpleQueue", SimpleQueue_new, SimpleQueue_push, SimpleQueue_pop, SimpleQueue_is_empty, SimpleQueue_delete },
    // { "RingsQueue", RingsQueue_new, RingsQueue_push, RingsQueue_pop, RingsQueue_is_empty, RingsQueue_delete },
    // { "LLQueue", LLQueue_new, LLQueue_push, LLQueue_pop, LLQueue_is_empty, LLQueue_delete },
    {"BLQueue", BLQueue_new, BLQueue_push, BLQueue_pop, BLQueue_is_empty, BLQueue_delete}};

#pragma GCC diagnostic pop

void basic_test(QueueVTable Q) {
    memset(added, 0, sizeof(added));
    memset(freed, 0, sizeof(added));
    
    HazardPointer_register(0, 1);
    void *queue = Q.new();

    for (int i = 0; i < ELEMENTS; i++) {
        Q.push(queue, i + 1);
        added[i + 1] = true;
    }

    for (int i = 0; i < ELEMENTS; i++) {
        Value a = Q.pop(queue);
        freed[i + 1] = true;
        assert(a == i + 1);
    }

    for (int i = 1; i < ELEMENTS /2; i++) {
        if (added[i] && !freed[i]) {
            printf("This was not freed: %d\n", i);
        }
    }

    // Q.push(queue, 1);
    // Q.push(queue, 2);
    // Q.push(queue, 3);
    // printf("pushed 1, 2, 3\n");
    // Value a = Q.pop(queue);
    // Value b = Q.pop(queue);
    // Value c = Q.pop(queue);
    // printf("%lu %lu %lu\n", a, b, c);

    assert(Q.is_empty(queue) == true);

    Q.delete(queue);
}

int main(void) {
    printf("Hello, World!\n");

    for (int i = 0; i < sizeof(queueVTables) / sizeof(QueueVTable); ++i) {
        QueueVTable Q = queueVTables[i];
        printf("Queue type: %s\n", Q.name);
        basic_test(Q);
    }

    return 0;
}
