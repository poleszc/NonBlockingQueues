#include <stdbool.h>
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
    for (int i = 0; i < _num_threads; i++) {
        for (int j = 0; j < MAX_THREADS; j++) {
            if (hp->retired[i][j]) {
                free(hp->retired[i][j]);
                hp->retired[i][j] = NULL;
            }
        }
    }
}

void *HazardPointer_protect(HazardPointer *hp, const _Atomic(void *) *atom) {
    _Atomic(void *) to_protect = NULL;
    while (1) {
        to_protect = atomic_load(atom);
        if (to_protect == atomic_load(atom)) {
            atomic_store(&hp->pointer[_thread_id],
                         to_protect);
            break;
        }
    }
    return to_protect;
}

void HazardPointer_clear(HazardPointer *hp) {
    atomic_store(&hp->pointer[_thread_id], NULL);
}

void HazardPointer_retire(HazardPointer *hp, void *ptr) {
    int retired_count = 0;

    for (int i = 0; i < MAX_THREADS; i++) { // Marking as retired O(n)
        if (!hp->retired[_thread_id][i]) {  // This space is free.
            hp->retired[_thread_id][i] = ptr;
            hp->count[_thread_id]++;
            break;
        }
    }

    if (retired_count >= RETIRED_THRESHOLD) {
        bool protected = false; // Is that ptr protected by anyone.
        for (int i = 0; i < MAX_THREADS; i++) {
            if (hp->retired[_thread_id][i]) {
                // Checking if address is not protected.
                protected = false;
                for (int j = 0; j < _num_threads && !protected; j++) {
                    protected = (hp->pointer[j] == hp->retired[_thread_id][i]);
                }
                if (!protected) {
                    free(hp->retired[_thread_id][i]);
                    hp->retired[_thread_id][i] = NULL;
                }
            }
        }
    }
}