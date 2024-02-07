#include <assert.h>
#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "HazardPointer.h"
#include "RingsQueue.h"

struct RingsQueueNode;
typedef struct RingsQueueNode RingsQueueNode;

struct RingsQueueNode {
    _Atomic(RingsQueueNode *) next;
    // TODO: Maybe it should be a ull*
    _Atomic(unsigned long long)
        push_idx; // How many pushes where done in this node
    _Atomic(unsigned long long)
        pop_idx; // How many pops where done in this node
    Value buffer[RING_SIZE];
};

RingsQueueNode *RingsQueueNode_new() {
    RingsQueueNode *node = (RingsQueueNode *)malloc(sizeof(RingsQueueNode));
    assert(node != NULL);

    node->push_idx = 0;
    node->pop_idx = 0;
    node->next = NULL;

    return node;
}

struct RingsQueue {
    RingsQueueNode *head;
    RingsQueueNode *tail;
    pthread_mutex_t push_mtx;
    pthread_mutex_t pop_mtx;
};

RingsQueue *RingsQueue_new(void) {
    RingsQueue *queue = (RingsQueue *)malloc(sizeof(RingsQueue));
    assert(queue != NULL);
    // TODO: Maybe fix that, cause we lose a lot of bytes on the mock node.
    RingsQueueNode *node = RingsQueueNode_new();

    queue->head = node;
    queue->tail = node;

    assert(pthread_mutex_init(&queue->push_mtx, NULL) >= 0);
    assert(pthread_mutex_init(&queue->pop_mtx, NULL) >= 0);

    return queue;
}

void RingsQueue_delete(RingsQueue *queue) {
    // TODO: Maybe add mutex.
    RingsQueueNode *first = queue->head;
    while (first != NULL) {
        RingsQueueNode *next = atomic_load(&first->next);
        free(first);
        first = next;
    }
    pthread_mutex_destroy(&queue->pop_mtx);
    pthread_mutex_destroy(&queue->push_mtx);
    free(queue);
}

void RingsQueue_push(RingsQueue *queue, Value item) {
    assert(pthread_mutex_lock(&queue->push_mtx) >= 0);
    RingsQueueNode *last = queue->tail;

    if (last->push_idx - last->pop_idx < RING_SIZE) { // Node is not full
        last->buffer[last->push_idx % RING_SIZE] = item;
        atomic_fetch_add(&last->push_idx, 1);
    } else { // Node is full
        RingsQueueNode *new = RingsQueueNode_new();
        new->buffer[0] = item;
        atomic_fetch_add(&new->push_idx, 1);
        last->next = new;
        queue->tail = new;
    }
    assert(pthread_mutex_unlock(&queue->push_mtx) >= 0);
}

Value RingsQueue_pop(RingsQueue *queue) {
    assert(pthread_mutex_lock(&queue->pop_mtx) >= 0);
    RingsQueueNode *first = queue->head;

    if (first->pop_idx < first->push_idx) { // Is not empty.
        Value value = queue->head->buffer[queue->head->pop_idx % RING_SIZE];
        atomic_fetch_add(&first->pop_idx, 1);
        pthread_mutex_unlock(&queue->pop_mtx);
        return value;
    } else {
        RingsQueueNode *next = first->next;
        if (next == NULL) { // Whole queue is empty.
            pthread_mutex_unlock(&queue->pop_mtx);
            return EMPTY_VALUE;
        } else { // Just this node is empty. Next one exists
            queue->head = next;
            Value item = next->buffer[next->pop_idx % RING_SIZE];
            atomic_fetch_add(&next->pop_idx, 1);

            assert(pthread_mutex_unlock(&queue->pop_mtx) >= 0);
            free(first);
            return item;
        }
    }
}

bool RingsQueue_is_empty(RingsQueue *queue) {
    pthread_mutex_lock(&queue->pop_mtx);
    RingsQueueNode *first = queue->head;
    int empty = (first->pop_idx >= first->push_idx && first->next == NULL);
    pthread_mutex_unlock(&queue->pop_mtx);
    return empty;
}
