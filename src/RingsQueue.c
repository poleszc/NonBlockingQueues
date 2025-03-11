#include <assert.h>
#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>

#include "HazardPointer.h"
#include "RingsQueue.h"

struct RingsQueueNode;
typedef struct RingsQueueNode RingsQueueNode;

struct RingsQueueNode {
    _Atomic(RingsQueueNode *) next;
    _Atomic(unsigned long long) push_idx; // Pushes done in this node
    _Atomic(unsigned long long) pop_idx;  // Pops done in this node
    Value buffer[RING_SIZE];
};

RingsQueueNode *RingsQueueNode_new() {
    RingsQueueNode *node = (RingsQueueNode *)malloc(sizeof(RingsQueueNode));
    assert(node != NULL);

    atomic_init(&node->push_idx, 0);
    atomic_init(&node->pop_idx, 0);
    atomic_init(&node->next, NULL);

    return node;
}

struct RingsQueue {
    RingsQueueNode *head;
    RingsQueueNode *tail;
    pthread_mutex_t push_mtx;
    pthread_mutex_t pop_mtx;
    _Atomic(unsigned long long) queue_size;

};

RingsQueue *RingsQueue_new(void) {
    RingsQueue *queue = (RingsQueue *)malloc(sizeof(RingsQueue));
    assert(queue != NULL);
    RingsQueueNode *node = RingsQueueNode_new();

    queue->head = node;
    queue->tail = node;

    atomic_init(&queue->queue_size, 0);
    assert(pthread_mutex_init(&queue->push_mtx, NULL) >= 0);
    assert(pthread_mutex_init(&queue->pop_mtx, NULL) >= 0);

    return queue;
}

void RingsQueue_delete(RingsQueue *queue) {
    RingsQueueNode *node = queue->head;
    while (node) {
        RingsQueueNode *next = atomic_load(&node->next);
        free(node);
        node = next;
    }

    pthread_mutex_destroy(&queue->pop_mtx);
    pthread_mutex_destroy(&queue->push_mtx);
    free(queue);
}

void RingsQueue_push(RingsQueue *queue, Value item) {
    assert(pthread_mutex_lock(&queue->push_mtx) >= 0);

    RingsQueueNode *last = queue->tail;

    if (atomic_load(&last->push_idx) - atomic_load(&last->pop_idx) < RING_SIZE) { // Node is not full
        last->buffer[atomic_load(&last->push_idx) % RING_SIZE] = item;
        atomic_fetch_add(&queue->queue_size, 1);
        atomic_fetch_add(&last->push_idx, 1);
    } else { // Node is full
        RingsQueueNode *new = RingsQueueNode_new();
        new->buffer[0] = item;
        atomic_fetch_add(&new->push_idx, 1);
        atomic_store(&last->next, new);
        queue->tail = new;
        atomic_fetch_add(&queue->queue_size, 1);
    }

    assert(pthread_mutex_unlock(&queue->push_mtx) >= 0);
}

Value RingsQueue_pop(RingsQueue *queue) {
    assert(pthread_mutex_lock(&queue->pop_mtx) >= 0);

    RingsQueueNode *first = queue->head;

    if (atomic_load(&first->pop_idx) < atomic_load(&first->push_idx)) { // Is not empty.
        Value value = first->buffer[atomic_load(&first->pop_idx) % RING_SIZE];
        atomic_fetch_add(&first->pop_idx, 1);
        atomic_fetch_sub(&queue->queue_size, 1);
        pthread_mutex_unlock(&queue->pop_mtx);

        return value;
    } else { // Buffer in that node is empty.
        RingsQueueNode *next = atomic_load(&first->next);

        if (!next) { // Whole queue is empty.
            atomic_store(&queue->queue_size, 0);
            pthread_mutex_unlock(&queue->pop_mtx);
            
            return EMPTY_VALUE;
        } else { // Just this node is empty. Next one exists
            queue->head = next;
            Value item = next->buffer[atomic_load(&next->pop_idx) % RING_SIZE];
            
            atomic_fetch_add(&next->pop_idx, 1);
            atomic_fetch_sub(&queue->queue_size, 1);
            assert(pthread_mutex_unlock(&queue->pop_mtx) >= 0);

            free(first);
            return item;
        }
    }
}

bool RingsQueue_is_empty(RingsQueue *queue) {
    return atomic_load(&queue->queue_size) == 0;
}
