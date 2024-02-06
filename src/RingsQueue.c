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
    ASSERT_NOT_NULL(node);

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
    ASSERT_NOT_NULL(queue);
    // TODO: Maybe fix that, cause we lose a lot of bytes on the mock node.
    RingsQueueNode *node = RingsQueueNode_new();

    queue->head = node;
    queue->tail = node;

    ASSERT_SYS_OK(pthread_mutex_init(&queue->push_mtx, NULL));
    ASSERT_SYS_OK(pthread_mutex_init(&queue->pop_mtx, NULL));

    return queue;
}

void RingsQueue_delete(RingsQueue *queue) {
    // TODO: Maybe add mutex.
    RingsQueueNode* first = queue->head;
    while (first != NULL) {
        RingsQueueNode* next = first->next; // TODO: Atomic
        free(first);
        first = next;
    }
    pthread_mutex_destroy(&queue->pop_mtx);
    pthread_mutex_destroy(&queue->push_mtx);
    free(queue);
}

void RingsQueue_push(RingsQueue *queue, Value item) {
    ASSERT_SYS_OK(pthread_mutex_lock(&queue->push_mtx));
    RingsQueueNode *last = queue->tail;

    if (last->push_idx - last->pop_idx < RING_SIZE) { // Node is not full
        last->buffer[last->push_idx % RING_SIZE] = item;
        last->push_idx++; // TODO: Change that to atomic
    } else {              // Node is full
        RingsQueueNode *new = RingsQueueNode_new();
        new->buffer[0] = item;
        new->push_idx++; // TODO: Change to atomic
        last->next = new;
        queue->tail = new;
    }
    ASSERT_SYS_OK(pthread_mutex_unlock(&queue->push_mtx));
}

Value RingsQueue_pop(RingsQueue *queue) {
    ASSERT_SYS_OK(pthread_mutex_lock(&queue->pop_mtx));
    RingsQueueNode *first = queue->head;

    if (first->pop_idx < first->push_idx) { // Is not empty.
        Value value = queue->head->buffer[queue->head->pop_idx % RING_SIZE];
        first->pop_idx++;
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
            next->pop_idx++; // TODO: Atomic
            ASSERT_SYS_OK(pthread_mutex_unlock(&queue->pop_mtx));

            free(first);
            return item;
        }
    }
}

bool RingsQueue_is_empty(RingsQueue *queue) {
    pthread_mutex_lock(&queue->pop_mtx); // TODO: Are mutex needed?
    RingsQueueNode *first = queue->head;
    int empty = (first->pop_idx >= first->push_idx && first->next == NULL);
    pthread_mutex_unlock(&queue->pop_mtx);
    return empty;
}
