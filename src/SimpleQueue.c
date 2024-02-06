#include <malloc.h>
#include <pthread.h>

#include "HazardPointer.h" // TODO: Remove that
#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

struct SimpleQueueNode {
    SimpleQueueNode *next;
    Value item;
};

SimpleQueueNode *SimpleQueueNode_new(Value item) {
    SimpleQueueNode *node = (SimpleQueueNode *)malloc(sizeof(SimpleQueueNode));
    ASSERT_NOT_NULL(node);

    node->item = item;
    node->next = NULL;
    return node;
}

struct SimpleQueue {
    SimpleQueueNode *head;
    SimpleQueueNode *tail;
    pthread_mutex_t head_mtx;
    pthread_mutex_t tail_mtx;
    size_t q_size;
};

SimpleQueue *SimpleQueue_new(void) {
    // TODO: Check malloc fail.
    SimpleQueue *queue = (SimpleQueue *)malloc(sizeof(SimpleQueue));
    ASSERT_NOT_NULL(queue);

    SimpleQueueNode *node = SimpleQueueNode_new(MOCK_VALUE);

    queue->q_size = 0;
    queue->head = node;
    queue->tail = node;

    ASSERT_SYS_OK(pthread_mutex_init(&queue->head_mtx, NULL));
    ASSERT_SYS_OK(pthread_mutex_init(&queue->tail_mtx, NULL));
    return queue;
}

void SimpleQueue_delete(SimpleQueue *queue) {
    // TODO: Maybe put that in one mutex. Cause it will to the exact same thing.
    while (!SimpleQueue_is_empty(queue)) {
        // TODO: Do we need to free value. I don't think so.
        Value temp = SimpleQueue_pop(queue);
    }

    ASSERT_SYS_OK(pthread_mutex_destroy(&queue->head_mtx));
    ASSERT_SYS_OK(pthread_mutex_destroy(&queue->tail_mtx));
    free(queue);
}

void SimpleQueue_push(SimpleQueue *queue, Value item) {
    ASSERT_SYS_OK(pthread_mutex_lock(&queue->head_mtx));

    SimpleQueueNode *node = SimpleQueueNode_new(item);
    queue->q_size++;
    queue->head->next = node;
    queue->head = node;

    ASSERT_SYS_OK(pthread_mutex_unlock(&queue->head_mtx));
}

Value SimpleQueue_pop(SimpleQueue *queue) {
    // TODO: Maybe put that only in the else statement
    ASSERT_SYS_OK(pthread_mutex_lock(&queue->tail_mtx));

    if (queue->q_size == 0) {
        ASSERT_SYS_OK(pthread_mutex_unlock(&queue->tail_mtx));
        return EMPTY_VALUE;
    } else {
        SimpleQueueNode *node = queue->tail;
        queue->q_size--;
        queue->tail = node->next;
        node->next = NULL; // TODO: maybe not needed
        Value item = node->item;

        free(node); // TODO: free assertions

        ASSERT_SYS_OK(pthread_mutex_unlock(&queue->tail_mtx));
        return item;
    }
}

bool SimpleQueue_is_empty(SimpleQueue *queue) { return (queue->q_size == 0); }
