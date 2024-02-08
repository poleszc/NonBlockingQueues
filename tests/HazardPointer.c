#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "HazardPointer.h"

thread_local int _thread_id = -1;
int _num_threads = -1;

void HazardPointer_register(int thread_id, int num_threads) {
    _thread_id = thread_id;
    _num_threads = num_threads;
}

void HazardPointer_initialize(HazardPointer *hp) {
    for (int i = 0; i < MAX_THREADS; i++) {
        for (int j = 0; j < MAX_THREADS; j++) {
            hp->retired[i][j] = NULL;
        }
    }
}

void HazardPointer_finalize(HazardPointer *hp) {
    // Each thread free's it's own retired set.
    for (int i = 0; i < MAX_THREADS; i++) {
        if (hp->retired[_thread_id][i]) {
            free(hp->retired[_thread_id][i]);
            hp->retired[_thread_id][i] = NULL;
        }
    }
}

void *HazardPointer_protect(HazardPointer *hp, const _Atomic(void *) *atom) {
    void *ptr_to_add = NULL;
    while (1) {
        ptr_to_add = atomic_load(atom);
        atomic_store(&hp->pointer[_thread_id], ptr_to_add);
        if (ptr_to_add == atomic_load(atom)) {
            break;
        }
    }
    return ptr_to_add;
}

void HazardPointer_clear(HazardPointer *hp) {
    atomic_store(&hp->pointer[_thread_id], NULL);
}

void HazardPointer_retire(HazardPointer *hp, void *ptr) {
    int retired_count = 0; // TODO: Optimise that
    bool found = false;     // Was the place for ptr found.
    bool protected = false; // Is that ptr protected bo anyone.

    for (int i = 0; i < MAX_THREADS; i++) { // Marking as retired O(n)
        if (!found &&
            hp->retired[_thread_id][i] == NULL) { // This space is free.
            hp->retired[_thread_id][i] = ptr;
        } else if (hp->retired[_thread_id][i]) {
            retired_count++;
        }
    }

    if (retired_count >= RETIRED_THRESHOLD) {
        for (int i = 0; i < _num_threads; i++) {
            for (int j = 0; j < MAX_THREADS; j++) {
                if (hp->retired[i][j]) {
                    protected = false;
                    // Checking is it's not protected
                    for (int k = 0; k < _num_threads && !protected; k++) {
                        protected = (hp->pointer[k] == hp->retired[i][j]);
                    }
                    if (!protected) {
                        free(hp->retired[i][j]);
                        hp->retired[i][j] = NULL;
                    }
                }
            }
        }
    }
}
