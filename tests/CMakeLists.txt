# cmake_minimum_required (VERSION 3.11)
# project (MIMUW-PW-lockfree C)

# set(THREADS_PREFER_PTHREAD_FLAG TRUE)  # prefer -pthread to -lphtread
# find_package (Threads REQUIRED)

# set(CMAKE_C_STANDARD 17)
# set(CMAKE_C_STANDARD_REQUIRED ON)
# set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

# set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-unused-parameter -O3 -march=native -fno-exceptions")

# # add_compile_options(-fsanitize=thread -Og -g)
# # add_link_options(-fsanitize=thread -Og -g)

# # add_compile_options(-fsanitize=address -Og -g)
# # add_link_options(-fsanitize=address -Og -g)

# add_library(queues OBJECT SimpleQueue.c RingsQueue.c LLQueue.c BLQueue.c HazardPointer.c)
# target_link_libraries(queues PRIVATE Threads::Threads atomic)

# add_executable(simpleTester simpleTester.c)
# target_link_libraries(simpleTester PRIVATE queues Threads::Threads atomic)

# add_executable(one_read_one_write one_read_one_write_tester.c)
# target_link_libraries(one_read_one_write PRIVATE queues Threads::Threads atomic)

# add_executable(one_read_one_write_tester_with_isempty one_read_one_write_tester_with_isempty.c)
# target_link_libraries(one_read_one_write_tester_with_isempty PRIVATE queues Threads::Threads atomic)

# add_executable(one_read_multiple_write one_read_multiple_write.c)
# target_link_libraries(one_read_multiple_write PRIVATE queues Threads::Threads atomic)

# add_executable(multiple_read_multiple_write multiple_read_multiple_write.c)
# target_link_libraries(multiple_read_multiple_write PRIVATE queues Threads::Threads atomic)

# add_executable(multiple_read_one_write multiple_read_one_write.c)
# target_link_libraries(multiple_read_one_write PRIVATE queues Threads::Threads atomic)

# SECOND CMAKE LIST
cmake_minimum_required (VERSION 3.11)
project (MIMUW-PW-lockfree C)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)  # prefer -pthread to -lphtread
find_package (Threads REQUIRED)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-unused-parameter -O3 -march=native -fno-exceptions")

#add_compile_options(-fsanitize=thread -Og -g)
#add_link_options(-fsanitize=thread -Og -g)
#
#add_compile_options(-fsanitize=address -Og -g)
#add_link_options(-fsanitize=address -Og -g)

if (DEBUG)
    add_definitions(-DTEST_DEBUG)
endif()

add_library(queues OBJECT SimpleQueue.c RingsQueue.c LLQueue.c BLQueue.c HazardPointer.c)
target_link_libraries(queues PRIVATE Threads::Threads atomic err)

add_executable(simpleTester simpleTester.c)
target_link_libraries(simpleTester PRIVATE queues Threads::Threads atomic)

add_library(err test/err.c
        tests/wow.c)

file(GLOB TEST_SOURCES "tests/*.c")
file(GLOB TO_REMOVE "tests/err.c")
list(REMOVE_ITEM TEST_SOURCES ${TO_REMOVE}) # pliki ktorych nie chcemy kompilowac

function(add_test_executable TEST_SOURCE)
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    set(TEST_NAME "test_${TEST_NAME}")
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE queues Threads::Threads atomic err)
endfunction()

foreach(TEST_SOURCE ${TEST_SOURCES})
    add_test_executable(${TEST_SOURCE})
endforeach()