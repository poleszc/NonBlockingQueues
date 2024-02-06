#pragma once

#include <stdint.h>

typedef int64_t Value;
static const Value EMPTY_VALUE = 0;
static const Value TAKEN_VALUE = -1;
static const Value MOCK_VALUE = INT64_MIN;

/* Assert that expression evaluates to zero (otherwise use result as error number, as in pthreads). */
#define ASSERT_ZERO(expr)                                                                  \
    do {                                                                                   \
        int errno;                                                                         \
        errno = (expr);                                                                    \
        if (errno != 0)                                                                    \
            syserr(                                                                        \
                "Failed: %s\n\tIn function %s() in %s line %d.\n\tErrno: ",      \
                #expr, __func__, __FILE__, __LINE__                                        \
            );                                                                             \
    } while(0)


/*
    Assert that expression doesn't evaluate to -1 (as almost every system function does in case of error).

    Use as a function, with a semicolon, like: ASSERT_SYS_OK(close(fd));
    (This is implemented with a 'do { ... } while(0)' block so that it can be used between if () and else.)
*/
#define ASSERT_SYS_OK(expr)                                                                \
    do {                                                                                   \
        if ((expr) == -1)                                                                  \
            syserr(                                                                        \
                "System command failed: %s\n\tIn function %s() in %s line %d.\n\tErrno: ", \
                #expr, __func__, __FILE__, __LINE__                                        \
            );                                                                             \
    } while(0)


#define ASSERT_NOT_NULL(expr)                                                  \
    do {                                                                       \
        if ((expr) == NULL)                                                    \
            syserr("value is null: %s\n\tIn function %s() in %s line "         \
                   "%d.\n\tErrno: ",                                           \
                   #expr, __func__, __FILE__, __LINE__);                       \
    } while (0)


/* Prints with information about system error (errno) and quits. */
_Noreturn extern void syserr(const char* fmt, ...);

/* Prints (like printf) and quits. */
_Noreturn extern void fatal(const char* fmt, ...);
