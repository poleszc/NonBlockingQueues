#include <assert.h>
#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "HazardPointer.h"
#include "LLQueue.h"

struct LLNode;
typedef struct LLNode LLNode;
typedef _Atomic(LLNode *) AtomicLLNodePtr;

struct LLNode {
    AtomicLLNodePtr next;
    _Atomic(Value) item;
};

LLNode *LLQueueNode_new(Value item) {
    LLNode *node = (LLNode *)malloc(sizeof(LLNode));
    assert(node != NULL);

    node->item = item;
    node->next = NULL;

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

    HazardPointer_initialize(&queue->hp); // TODO: Does that belong here???


    // Mock node to avoid edge cases
    LLNode *node = LLQueueNode_new(EMPTY_VALUE); 

    atomic_init(&queue->head, node); // TODO: Co robi init?
    atomic_init(&queue->tail, node);

    return queue;
}

void LLQueue_delete(LLQueue *queue) {
    LLNode *node = atomic_load(&queue->head);
    while (node) {
        LLNode *next = atomic_load(&node->next);
        HazardPointer_retire(&queue->hp, node);
        node = next;
    }

    HazardPointer_finalize(&queue->hp);

    free(queue); // TODO: Free that memory?
}

void LLQueue_push(LLQueue *queue, Value item) {
    _Atomic(LLNode *) node = LLQueueNode_new(item);
    LLNode *last = NULL;
    LLNode *next = NULL;

    while (1) {
        last = atomic_load(&queue->tail);
        next = atomic_load(&last->next);
        if (last == atomic_load(&queue->tail)) {
            if (next == NULL) {
                if (atomic_compare_exchange_strong(&last->next, &next, node)) {
                    atomic_compare_exchange_strong(&queue->tail, &last, node);
                    return;
                }
            } else {
                atomic_compare_exchange_strong(&queue->tail, &last, next);
            }
        }
    }
}

Value LLQueue_pop(LLQueue *queue) {
    LLNode *first = NULL;
    LLNode *last = NULL;
    LLNode *next = NULL;

    while (1) {
        first = atomic_load(&queue->head);
        last = atomic_load(&queue->tail);
        next = atomic_load(&first->next);

        if (first == atomic_load(&queue->head)) {
            if (first == last) {
                if (next == NULL) {
                    return EMPTY_VALUE;
                }
                atomic_compare_exchange_strong(&queue->tail, &last, next);
            } else {
                Value value = next->item;
                if (atomic_compare_exchange_strong(&queue->head, &first,
                                                   next)) {
                    return value;
                }
            }
        }
    }
}

bool LLQueue_is_empty(LLQueue *queue) {
    LLNode *head = atomic_load(&queue->head);
    LLNode *tail = atomic_load(&queue->tail);
    LLNode *next = atomic_load(&head->next);

    return (head == tail && next == NULL);
}
