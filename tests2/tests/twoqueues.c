#include <stdio.h>
#include <threads.h>
#include <stdlib.h>
#include "tester.h"
#include "../HazardPointer.h"

const int Q2_ITERATIONS = 1e5;

struct Q2args {
    QueueVTable Q;
    void* queue1;
    void* queue2;
    int tid;
    int threads;
    int producers;
};

volatile bool checker_done = false;

int producer(void* args) {
    struct Q2args* a = args;
    HazardPointer_register(a->tid, a->threads);

    for (int i = 1; i < Q2_ITERATIONS; ++i) {
        a->Q.push(a->queue1, i);
    }

    return 0;
}

int consumer(void* args) {
    struct Q2args* a = args;
    HazardPointer_register(a->tid, a->threads);

    while(!checker_done) {
        Value v = a->Q.pop(a->queue1);
        if (v != EMPTY_VALUE)
            a->Q.push(a->queue2, v);
    }

    return 0;
}

int checker(void* args) {
    struct Q2args* a = args;
    HazardPointer_register(a->tid, a->threads);

    int* seen = calloc(Q2_ITERATIONS, sizeof(int));
    int seen_count = 0;
    while (seen_count < (Q2_ITERATIONS-1) * a->producers) {
        Value v = a->Q.pop(a->queue2);
        while (v == EMPTY_VALUE) {
            v = a->Q.pop(a->queue2);
        }
        seen[v] += 1;
        seen_count++;
    }

    checker_done = true;

    for (int i = 1; i < Q2_ITERATIONS; ++i) {
        if (seen[i] != a->producers) {
            printf("\033[1;31mERROR: checker saw %d %d times instead of %d\033[0m\n", i, seen[i], a->producers);
            free(seen);
            return 1;
        }
    }

    free(seen);

    return 0;
}


static enum Result two_queues(QueueVTable Q, int producers, int consumers) {
    void* queue1 = Q.new();
    void* queue2 = Q.new();

    struct Q2args args_base = { Q, queue1, queue2, 0, producers + consumers + 1, producers };

    thrd_t threads[producers + consumers + 1];

    for (int i = 0; i < producers; ++i) {
        struct Q2args args = args_base;
        args.tid = i + 1;
        thrd_create(&threads[i], producer, &args);
    }

    for (int i = 0; i < consumers; ++i) {
        struct Q2args args = args_base;
        args.tid = i + producers + 1;
        thrd_create(&threads[i + producers], consumer, &args);
    }

    thrd_create(&threads[producers + consumers], checker, &args_base);

    bool ok = true;

    for (int i = 0; i < producers + consumers + 1; ++i) {
        int status;
        thrd_join(threads[i], &status);
        if (status != 0) {
            ok = false;
        }
    }

    Q.delete(queue1);
    Q.delete(queue2);

    return ok ? PASSED : FAILED;
}

static enum Result two_queues_1p_1c(QueueVTable Q) {
    return two_queues(Q, 1, 1);
}
ADD_TEST(two_queues_1p_1c)

static enum Result two_queues_2p_1c(QueueVTable Q) {
    disable_on(RingsQueue);
    return two_queues(Q, 2, 1);
}
ADD_TEST(two_queues_2p_1c)

static enum Result two_queues_1p_2c(QueueVTable Q) {
    disable_on(RingsQueue);
    return two_queues(Q, 1, 2);
}
ADD_TEST(two_queues_1p_2c)

static enum Result two_queues_2p_2c(QueueVTable Q) {
    disable_on(RingsQueue);
    return two_queues(Q, 2, 2);
}
ADD_TEST(two_queues_2p_2c)

static enum Result two_queues_2p_4c(QueueVTable Q) {
    disable_on(RingsQueue);
    return two_queues(Q, 2, 4);
}
ADD_TEST(two_queues_2p_4c)

static enum Result two_queues_4p_2c(QueueVTable Q) {
    disable_on(RingsQueue);
    return two_queues(Q, 4, 2);
}
ADD_TEST(two_queues_4p_2c)
