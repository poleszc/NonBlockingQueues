#include <malloc.h>
#include <pthread.h>

#include "HazardPointer.h"
#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

struct SimpleQueueNode {
    SimpleQueueNode* next;
    Value item;
};

SimpleQueueNode* SimpleQueueNode_new(Value item)
{
    SimpleQueueNode* node = (SimpleQueueNode*)malloc(sizeof(SimpleQueueNode));
    // TODO
    return node;
}

struct SimpleQueue {
    SimpleQueueNode* head;
    SimpleQueueNode* tail;
    pthread_mutex_t head_mtx;
    pthread_mutex_t tail_mtx;
};

SimpleQueue* SimpleQueue_new(void)
{
    SimpleQueue* queue = (SimpleQueue*)malloc(sizeof(SimpleQueue));
    // TODO
    return queue;
}

void SimpleQueue_delete(SimpleQueue* queue)
{
    // TODO
    free(queue);
}

void SimpleQueue_push(SimpleQueue* queue, Value item)
{
    // TODO
}

Value SimpleQueue_pop(SimpleQueue* queue)
{
    return EMPTY_VALUE; // TODO
}

bool SimpleQueue_is_empty(SimpleQueue* queue)
{
    return false; // TODO
}
