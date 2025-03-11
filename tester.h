//
// Created by cloudburst on 20.01.24.
//

#ifndef MIMUW_PW_LOCKFREE_TESTER_H
#define MIMUW_PW_LOCKFREE_TESTER_H

#include <stdbool.h>
#include <string.h>
#include "../common.h"

enum Result {
    PASSED,
    FAILED,
    SKIPPED
};

typedef struct {
    const char *name;

    void *(*new)(void);

    void (*push)(void *queue, Value item);

    Value (*pop)(void *queue);

    bool (*is_empty)(void *queue);

    void (*delete)(void *queue);
} QueueVTable;

#define MakeVTable(name) { #name, name ## _new, name ## _push, name ## _pop, name ## _is_empty, name ## _delete }

#define disable_on(q_name) { \
    if (strcmp(Q.name, #q_name) == 0) { \
        return SKIPPED; \
    } \
}

typedef enum Result (*TestFunction)(QueueVTable Q);

typedef struct TestFunctionPtr {
    TestFunction test;
    char name[128 - sizeof(TestFunction)];
} TestFunctionPtr;

#define ADD_TEST(fn) \
    static TestFunctionPtr fn ## _testptr \
    __attribute((used, section("testptrs"))) = { .test = fn, .name = #fn };

#define foreach_test(elem) \
    for(TestFunctionPtr* elem = \
    ({                     \
        extern TestFunctionPtr __start_testptrs; \
        &__start_testptrs; \
    });                    \
    elem !=                 \
    ({                     \
        extern TestFunctionPtr __stop_testptrs; \
        &__stop_testptrs; \
    });                    \
    ++elem)


#endif //MIMUW_PW_LOCKFREE_TESTER_H
