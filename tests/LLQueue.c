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

    HazardPointer_initialize(&queue->hp); // TODO: Does that belong here???

    // Mock node to avoid edge cases
    LLNode *node = LLQueueNode_new(EMPTY_VALUE);

    atomic_init(&queue->head, node); // TODO: Co robi init?
    atomic_init(&queue->tail, node);

    return queue;
}

void LLQueue_delete(LLQueue *queue) {
    pid_t tid = syscall(SYS_gettid); // TODO: Remove that
    printf("LLQueueDelete from: %d\n", tid);

    AtomicLLNodePtr node = atomic_load(&queue->head);
    void *node_void_ptr = (void *)node;
    HazardPointer_protect(&queue->hp, (_Atomic(void *) *)&node_void_ptr);

    while (node) {
        printf("A\n");
        AtomicLLNodePtr next = atomic_load(&node->next);

        void *next_void_ptr = (void *)node;
        HazardPointer_protect(&queue->hp, (_Atomic(void *) *)&next_void_ptr);

        HazardPointer_retire(&queue->hp, node);
        node = next;
    }
    HazardPointer_clear(&queue->hp);
    HazardPointer_finalize(&queue->hp);

    free(queue); // TODO: Free that memory?
}

void LLQueue_push(LLQueue *queue, Value item) {
    pid_t tid = syscall(SYS_gettid); // TODO: Remove that
    printf("LLQueuePush from: %d\n", tid);

    AtomicLLNodePtr node = LLQueueNode_new(item);
    LLNode *last = NULL;
    LLNode *next = NULL;

    // HazardPointer_protect(&queue->hp, node);

    while (1) {
        last = atomic_load(&queue->tail);

        void *last_void_ptr = (void *)last;
        HazardPointer_protect(&queue->hp, (_Atomic(void *) *)&last_void_ptr);

        if (last == atomic_load(&queue->tail)) {
            next = atomic_load(&last->next);
            void *next_void_ptr = (void *)next;
            HazardPointer_protect(&queue->hp, (_Atomic(void *) *)&next_void_ptr);

            if (next == NULL) {
                if (atomic_compare_exchange_strong(&last->next, &next, node)) {
                    atomic_compare_exchange_strong(&queue->tail, &last, node);
                    HazardPointer_clear(&queue->hp);
                    return;
                }
            } else {
                atomic_compare_exchange_strong(&queue->tail, &last, next);
                HazardPointer_clear(&queue->hp);
            }
        }
    }
}

Value LLQueue_pop(LLQueue *queue) {
    pid_t tid = syscall(SYS_gettid); // TODO: Remove that
    printf("LLQueuePop from: %d\n", tid);

    LLNode *first = NULL;
    LLNode *last = NULL;
    LLNode *next = NULL;

    while (1) {
        first = atomic_load(&queue->head);
        last = atomic_load(&queue->tail);
        next = atomic_load(&first->next);

        if (first == atomic_load(&queue->head)) {
            if (first == last) {
                if (next == NULL) { // Queue is empty
                    return EMPTY_VALUE;
                }
                atomic_compare_exchange_strong(&queue->tail, &last, next);
            } else {
                Value value = next->item;
                if (atomic_compare_exchange_strong(&queue->head, &first, next)) {
                    HazardPointer_retire(&queue->hp, first);
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
