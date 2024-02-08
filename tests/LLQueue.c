#include <assert.h>
#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <syscall.h>
#include <threads.h>
#include <unistd.h>

#include "HazardPointer.h"
#include "LLQueue.h"

struct LLNode;
typedef struct LLNode LLNode;
typedef _Atomic(LLNode *) AtomicLLNodePtr;

struct LLNode {
    AtomicLLNodePtr next;
    Value item; // TODO: This shouldn't be atomic???
};

LLNode *LLQueueNode_new(Value item) {
    LLNode *node = (LLNode *)malloc(sizeof(LLNode));
    assert(node != NULL);

    node->item = item;
    atomic_init(&node->next, NULL);

    return node;
}

struct LLQueue {
    AtomicLLNodePtr head;
    AtomicLLNodePtr tail;
    HazardPointer hp;
};

LLQueue *LLQueue_new(void) {
    LLQueue *queue = (LLQueue *)malloc(sizeof(LLQueue));
    assert(queue != NULL);

    HazardPointer_initialize(&queue->hp);

    // Mock node to avoid edge cases
    LLNode *node = LLQueueNode_new(EMPTY_VALUE);

    atomic_init(&queue->head, node);
    atomic_init(&queue->tail, node);

    return queue;
}

void LLQueue_delete(LLQueue *queue) {
    LLNode* node = HazardPointer_protect(&queue->hp, &queue->head);
    while (node) {
        LLNode* next = HazardPointer_protect(&queue->hp, &node->next);
        HazardPointer_retire(&queue->hp, node);
        node = next;
    }
    HazardPointer_clear(&queue->hp);
    HazardPointer_finalize(&queue->hp);

    free(queue); // TODO: Free that memory?
}

void LLQueue_push(LLQueue *queue, Value item) {
    LLNode *node = LLQueueNode_new(item);
    while (1) {
        LLNode *last = HazardPointer_protect(&queue->hp, &queue->tail);

        if (last == atomic_load(&queue->tail)) {
            LLNode *next = HazardPointer_protect(&queue->hp, &last->next);
            if (next == NULL) {
                if (atomic_compare_exchange_strong(&last->next, &next, node)) {
                    atomic_compare_exchange_strong(&queue->tail, &last, node);
                    HazardPointer_clear(&queue->hp);
                    return;
                }
            } else {
                atomic_compare_exchange_strong(&queue->tail, &last, next);
            }
        }
    }
}

Value LLQueue_pop(LLQueue *queue) {
    while (1) {
        LLNode *first = HazardPointer_protect(&queue->hp, &queue->head);
        LLNode *last = atomic_load(&queue->tail); // Safe
        LLNode *next = HazardPointer_protect(
            &queue->hp, &first->next); // first is protected

        if (first == atomic_load(&queue->head)) {
            if (first == last) {
                if (next == NULL) { // Queue is empty
                    return EMPTY_VALUE;
                }
                atomic_compare_exchange_strong(&queue->tail, &last,
                                               next); // Safe
            } else {
                Value value = next->item; // Next is protected so this is safe.
                if (atomic_compare_exchange_strong(&queue->head, &first,
                                                   next)) {
                    HazardPointer_clear(&queue->hp);
                    HazardPointer_retire(&queue->hp, first);
                    return value;
                }
            }
        }
    }
}

bool LLQueue_is_empty(LLQueue *queue) { // Basically same as LLQueue_pop
    while (1) {
        LLNode *first = HazardPointer_protect(&queue->hp, &queue->head);
        LLNode *last = atomic_load(&queue->tail); // Safe
        LLNode *next = HazardPointer_protect(
            &queue->hp, &first->next); // first is protected

        if (first == atomic_load(&queue->head)) {
            if (first == last) {
                if (next == NULL) { // Queue is empty
                    return true;
                }
                atomic_compare_exchange_strong(&queue->tail, &last,
                                               next); // Safe
            } else {
                Value value = next->item; // Next is protected so this is safe.
                if (atomic_compare_exchange_strong(&queue->head, &first,
                                                   next)) {
                    HazardPointer_clear(&queue->hp);
                    HazardPointer_retire(&queue->hp, first);
                    return false;
                }
            }
        }
    }
}
