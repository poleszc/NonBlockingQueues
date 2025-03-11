#include <assert.h>
#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>

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
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        atomic_init(&node->buffer[i], EMPTY_VALUE);
    }
    atomic_store(&node->buffer[0], val);

    return node;
}

struct BLQueue {
    AtomicBLNodePtr head;
    AtomicBLNodePtr tail;
    HazardPointer hp;
    _Atomic(int) malloc;
    _Atomic(int) free;
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
    BLNode *node = queue->head;
    while (node) {
        BLNode *next = atomic_load(&node->next);
        free(node);
        node = next;
    }

    HazardPointer_finalize(&queue->hp);
    free(queue);
}

void BLQueue_push(BLQueue *queue, Value item) {
    while (1) {
        BLNode *last = HazardPointer_protect(&queue->hp, (void *)&queue->tail);
        if (last == atomic_load(&queue->tail)) {
            unsigned long long push_idx = atomic_fetch_add(&last->push_idx, 1);
            if (push_idx < BUFFER_SIZE) {
                Value expected = EMPTY_VALUE;
                if (atomic_compare_exchange_strong(
                        &last->buffer[push_idx % BUFFER_SIZE], &expected,
                        item)) {
                    HazardPointer_clear(&queue->hp);
                    return;
                }
            } else {
                BLNode *next = atomic_load(&last->next);
                if (next) { // We make sure that the node changed.
                    atomic_compare_exchange_strong(&queue->tail, &last, next);
                } else {
                    if (last ==
                        atomic_load(&queue->tail)) { // We are the ones adding
                                                     // the new node
                        BLNode *node = BLQueueNode_new(item);
                        BLNode *expected = NULL;
                        // Successfully added a node
                        if (atomic_compare_exchange_strong(&last->next,
                                                           &expected, node)) {
                            atomic_compare_exchange_strong(&queue->tail, &last,
                                                           node);
                            HazardPointer_clear(&queue->hp);
                            return;
                        } else { // Didn't add the node, trying again.
                            free(node);
                            continue;
                        }
                    }
                }
            }
        }
    }
}

Value BLQueue_pop(BLQueue *queue) {
    while (1) {
        BLNode *first = HazardPointer_protect(&queue->hp, (void *)&queue->head);

        if (first == atomic_load(&queue->head)) {
            unsigned long long pop_idx = atomic_fetch_add(&first->pop_idx, 1);

            if (pop_idx < BUFFER_SIZE) {
                Value got = atomic_exchange(
                    &first->buffer[pop_idx % BUFFER_SIZE], TAKEN_VALUE);
                if (got == EMPTY_VALUE) {
                    continue;
                } else {
                    HazardPointer_clear(&queue->hp);
                    return got;
                }
            } else {
                BLNode *next = atomic_load(&first->next);
                if (next == NULL) { // Queue is empty.
                    HazardPointer_clear(&queue->hp);
                    return EMPTY_VALUE;
                } else {
                    if (atomic_compare_exchange_strong(&queue->head, &first,
                                                       next)) {
                        HazardPointer_retire(&queue->hp, first);
                        HazardPointer_clear(&queue->hp);
                    }
                }
            }
        }
    }
}

bool BLQueue_is_empty(BLQueue *queue) {
    BLNode *first = HazardPointer_protect(&queue->hp, (void *)&queue->head);
    BLNode *next = atomic_load(&first->next);

    if (!next) {
        unsigned long long pop_idx = atomic_load(&first->pop_idx);
        if (pop_idx < BUFFER_SIZE) { // Buffer not full.
            Value got = atomic_load(&first->buffer[pop_idx % BUFFER_SIZE]);
            if (got == EMPTY_VALUE) { // Pop idx is empty, thus whole queue is.
                HazardPointer_clear(&queue->hp);
                return true;
            } else {
                HazardPointer_clear(&queue->hp);
                return false;
            }

        } else { // This one full, next one doesn't exist, thus queue is empty.
            HazardPointer_clear(&queue->hp);
            return true;
        }

    } else { // Next one exists, so it's not empty.
        HazardPointer_clear(&queue->hp);
        return false;
    }
}
