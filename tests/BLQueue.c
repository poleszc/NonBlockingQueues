#include <assert.h>
#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "BLQueue.h"
#include "HazardPointer.h"

struct BLNode;
typedef struct BLNode BLNode;
typedef _Atomic(BLNode *) AtomicBLNodePtr;

struct BLNode {
    AtomicBLNodePtr next;
    _Atomic(Value) buffer[BUFFER_SIZE];
    _Atomic(unsigned long long) push_idx; // Increments on every push(successful or not).
    _Atomic(unsigned long long) pop_idx;  // Increments on every pop(successful or not).
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

    HazardPointer_initialize(&queue->hp);

    BLNode *node = BLQueueNode_new(EMPTY_VALUE);
    queue->head = node;
    queue->tail = node;
    return queue;
}

void BLQueue_delete(BLQueue *queue) {
    // BLNode *node = atomic_load(&queue->head);
    BLNode *node = HazardPointer_protect(&queue->hp, &queue->head);
    while (node) {
        // BLNode *next = atomic_load(&node->next);
        BLNode *next = HazardPointer_protect(&queue->hp, &node->next);
        HazardPointer_retire(&queue->hp, node);
        node = next;
    }
    HazardPointer_clear(&queue->hp);
    HazardPointer_finalize(&queue->hp);
    free(queue);
}

void BLQueue_push(BLQueue *queue, Value item) {
    BLNode *node = BLQueueNode_new(item);
    while (1) {
        // BLNode* last = atomic_load(&queue->tail);
        BLNode* last = HazardPointer_protect(&queue->hp, &queue->tail);

        if (last == atomic_load(&queue->tail)) { // TODO: Maybe not needed
            unsigned long long push_idx = atomic_fetch_add(&last->push_idx, 1);
            if (push_idx < BUFFER_SIZE) {
                Value expected = EMPTY_VALUE;
                if (atomic_compare_exchange_strong(&last->buffer[push_idx % BUFFER_SIZE], &expected, item)) {
                    HazardPointer_clear(&queue->hp);
                    return;
                } else {
                    // HazardPointer_clear(&queue->hp); // TODO: Is that needed here?
                    continue;
                }
            } else {
                if (last != atomic_load(&queue->tail)) { // New node appeared
                    continue;
                } else { // We are the ones adding the new node
                    BLNode *expected = NULL;
                    // Successfully added a node
                    if (atomic_compare_exchange_strong(&last->next, &expected, node)) {
                        atomic_compare_exchange_strong(&queue->tail, &last, node);
                        HazardPointer_clear(&queue->hp);
                        return;
                    } else { // Didn't add the mode.
                        continue;
                    }
                }
            }
        }
    }
}

Value BLQueue_pop(BLQueue *queue) {
    while (1) {
        BLNode* first =  HazardPointer_protect(&queue->hp, &queue->head);

        if (first == atomic_load(&queue->head)) {
            unsigned long long pop_idx = atomic_fetch_add(&first->pop_idx, 1);

            if (pop_idx < BUFFER_SIZE) {
                Value got = atomic_exchange(&first->buffer[pop_idx % BUFFER_SIZE], TAKEN_VALUE);
                if (got == EMPTY_VALUE) {
                    continue;
                } else {
                    HazardPointer_clear(&queue->hp);
                    return got;
                }
            } else {
                BLNode *next = atomic_load(&first->next); // TODO: That may not work
                if (next == NULL) { // Queue is empty.
                    HazardPointer_clear(&queue->hp);
                    return EMPTY_VALUE;
                } else {
                    atomic_exchange(&queue->head, next);
                    HazardPointer_retire(&queue->hp, first);
                    HazardPointer_clear(&queue->hp);
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
