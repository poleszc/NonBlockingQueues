#include <assert.h>
#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// https://github.com/DennisCGc/ringbuffer/blob/master/ringbuffer.h
// https://github.com/DennisCGc/ringbuffer/blob/master/ringbuffer.c

#include "BLQueue.h"
#include "HazardPointer.h"

struct BLNode;
typedef struct BLNode BLNode;
typedef _Atomic(BLNode *) AtomicBLNodePtr;

struct BLNode {
    AtomicBLNodePtr next;
    _Atomic(Value) buffer[BUFFER_SIZE];
    _Atomic(unsigned long long)
        push_idx; // Increments on every push(successful or not).
    _Atomic(unsigned long long)
        pop_idx; // Increments on every pop(successful or not).
};

BLNode *BLQueueNode_new(Value val) {
    BLNode *node = (BLNode *)malloc(sizeof(BLNode));
    assert(node != NULL);

    node->next = NULL;
    node->push_idx = 0;
    node->pop_idx = 0;

    // Initial values in buffer are EMPTY_VALUE.
    // memset(node->buffer, EMPTY_VALUE, BUFFER_SIZE); // TODO: Maybe that works
    // the same way.
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        atomic_init(&node->buffer[i], EMPTY_VALUE);
    }

    node->buffer[0] = val;
    // TODO: Zrób coś z buforem
    return node;
}

struct BLQueue {
    AtomicBLNodePtr head;
    AtomicBLNodePtr tail;
    HazardPointer hp;
};

BLQueue *BLQueue_new(void) {
    BLQueue *queue = (BLQueue *)malloc(sizeof(BLQueue));
    assert(queue != NULL);

    BLNode *node = BLQueueNode_new(EMPTY_VALUE);
    queue->head = node;
    queue->tail = node;
    return queue;
}

void BLQueue_delete(BLQueue *queue) {
    BLNode *node = atomic_load(&queue->head);
    while (node != NULL) {
        BLNode *next = atomic_load(&node->next);
        free(node);
        node = next;
    }
    free(queue);
}

void BLQueue_push(BLQueue *queue, Value item) {
    BLNode *last = NULL;
    while (1) {
        last = atomic_load(&queue->tail);

        if (last == atomic_load(&queue->tail)) { // TODO: Maybe not needed
            unsigned long long push_idx = atomic_fetch_add(&last->push_idx, 1);
            if (push_idx < BUFFER_SIZE) {
                Value expected = EMPTY_VALUE;
                if (atomic_compare_exchange_strong(
                        &last->buffer[push_idx % BUFFER_SIZE], &expected,
                        item)) {
                    return;
                } else {
                    continue;
                }
            } else {
                if (last != atomic_load(&queue->tail)) { // New node appeared
                    continue;
                } else { // We are the ones adding the new node
                    BLNode *node = BLQueueNode_new(item);
                    BLNode *expected = NULL;
                    // Successfully added a node
                    if (atomic_compare_exchange_strong(&last->next, &expected,
                                                       node)) {
                        atomic_compare_exchange_strong(&queue->tail, &last,
                                                       node);
                        return;
                    } else { // Didn't add the mode.
                        free(node);
                        continue;
                    }
                }
            }
        }
    }
}

Value BLQueue_pop(BLQueue *queue) {
    BLNode *first = NULL;

    while (1) {
        first = atomic_load(&queue->head);

        if (first == atomic_load(&queue->head)) {
            unsigned long long pop_idx = atomic_fetch_add(&first->pop_idx, 1);

            if (pop_idx < BUFFER_SIZE) {
                Value got = atomic_exchange(
                    &first->buffer[pop_idx % BUFFER_SIZE], TAKEN_VALUE);
                if (got == EMPTY_VALUE) {
                    continue;
                } else {
                    return got;
                }
            } else {
                BLNode *next =
                    atomic_load(&first->next); // TODO: That may not work
                if (next == NULL) {
                    return EMPTY_VALUE;
                } else {
                    atomic_exchange(&queue->head, next);
                }
            }
        }
    }
}

bool BLQueue_is_empty(BLQueue *queue) {
    BLNode *first = atomic_load(&queue->head);
    unsigned long long push_idx = atomic_load(&first->push_idx);
    unsigned long long pop_idx = atomic_load(&first->pop_idx);

    return (first == atomic_load(&queue->head) && push_idx == pop_idx);
}
