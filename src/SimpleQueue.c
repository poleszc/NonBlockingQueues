#include <assert.h>
#include <malloc.h>
#include <pthread.h>

#include "HazardPointer.h" // TODO: Remove that
#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

static const Value MOCK_VALUE = INT64_MIN;

struct SimpleQueueNode {
    _Atomic(SimpleQueueNode *) next;
    Value item;
};

SimpleQueueNode *SimpleQueueNode_new(Value item) {
    SimpleQueueNode *node = (SimpleQueueNode *)malloc(sizeof(SimpleQueueNode));
    assert(node != NULL);

    node->item = item;
    node->next = NULL;
    return node;
}

struct SimpleQueue {
    SimpleQueueNode *head;
    SimpleQueueNode *tail;
    pthread_mutex_t head_mtx;
    pthread_mutex_t tail_mtx;
};

SimpleQueue *SimpleQueue_new(void) {
    SimpleQueue *queue = (SimpleQueue *)malloc(sizeof(SimpleQueue));
    assert(queue != NULL);

    SimpleQueueNode *node = SimpleQueueNode_new(MOCK_VALUE);

    queue->head = node;
    queue->tail = node;

    assert(pthread_mutex_init(&queue->head_mtx, NULL) >= 0);
    assert(pthread_mutex_init(&queue->tail_mtx, NULL) >= 0);
    return queue;
}

void SimpleQueue_delete(SimpleQueue *queue) {
    // TODO: Maybe put that in one mutex. Cause it will to the exact same thing.
    SimpleQueueNode* node = queue->head;

    while (node) {
        SimpleQueueNode* next = atomic_load(&node->next);
        free(node);
        node = next;
    }

    assert(pthread_mutex_destroy(&queue->head_mtx) >= 0);
    assert(pthread_mutex_destroy(&queue->tail_mtx) >= 0);
    free(queue);
}

void SimpleQueue_push(SimpleQueue *queue, Value item) {

    SimpleQueueNode *node = SimpleQueueNode_new(item);
    assert(pthread_mutex_lock(&queue->head_mtx) >= 0); // TODO: Maybe before allocation
    queue->head->next = node;
    queue->head = node;
    assert(pthread_mutex_unlock(&queue->head_mtx) >= 0);
}

Value SimpleQueue_pop(SimpleQueue *queue) {
    // TODO: Maybe put that only in the else statement
    assert(pthread_mutex_lock(&queue->tail_mtx) >= 0);
    SimpleQueueNode* last = queue->tail;
    SimpleQueueNode* next = atomic_load(&last->next); 

    if (next == NULL) {
        assert(pthread_mutex_unlock(&queue->tail_mtx) >= 0);
        return EMPTY_VALUE;
    } else {
        Value item = next->item;
        queue->tail = next;
        last->next = NULL; // TODO: Maybe not needed

        assert(pthread_mutex_unlock(&queue->tail_mtx) >= 0);
        free(last);
        return item;
    }
}

bool SimpleQueue_is_empty(SimpleQueue *queue) {
    pthread_mutex_lock(&queue->tail_mtx);
    bool isEmpty = (atomic_load(&queue->tail->next) == NULL);
    pthread_mutex_unlock(&queue->tail_mtx);
    return isEmpty;
}
