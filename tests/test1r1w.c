#include <stdio.h>
#include <threads.h>
#include "tester.h"
#include "../HazardPointer.h"

const int O1OW_ITERATIONS = 1e6;
const int O1OW_MAX_LOOPS = O1OW_ITERATIONS * 100;

struct O1OWargs {
    QueueVTable Q;
    void* queue;
};

int o1o_reader(void* args)
{
    struct O1OWargs* a = args;
    HazardPointer_register(0, 2);
    for (int i = 1; i < 1000000; ++i) {
        Value v = a->Q.pop(a->queue);
        int loops = 0;
        while (v == EMPTY_VALUE) {
            v = a->Q.pop(a->queue);
            ++loops;
            if (loops > O1OW_MAX_LOOPS) {
                printf("\033[1;31mERROR: reader looped too many times\033[0m\n");
                return 1;
            }
        }
        if (v != i) {
            printf("\033[1;31mERROR: reader got %lu instead of %d\033[0m\n", v, i);
            return 1;
        }

    }
    return 0;
}


int o1o_writer(void* arg)
{
    struct O1OWargs* a = arg;
    HazardPointer_register(1, 2);
    for (int i = 1; i < 1000000; ++i) {
        a->Q.push(a->queue, i);
    }
    return 0;
}


static enum Result one_reader_one_writer(QueueVTable Q)
{
    HazardPointer_register(0, 1);
    void* queue = Q.new();

    struct O1OWargs args = { Q, queue };

    thrd_t reader_thread;
    thrd_t writer_thread;

    thrd_create(&reader_thread, o1o_reader, &args);
    thrd_create(&writer_thread, o1o_writer, &args);

    int reader_ret;

    thrd_join(reader_thread, &reader_ret);
    thrd_join(writer_thread, NULL);

    Q.delete(queue);

    return reader_ret == 0 ? PASSED : FAILED;
}
ADD_TEST(one_reader_one_writer)
