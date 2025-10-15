#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "qemu/log.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_QUEUE 1024
typedef enum {
    EVENT_SDA,  // Symbolic Data Access (load/store)
    EVENT_SCB   // Symbolic Control Branch
} event_type_t;

typedef struct {
    event_type_t type;
    uint64_t pc;
    uint64_t addr;   // for load/store
} cache_event_t;

 cache_event_t queue[MAX_QUEUE];
 int qhead = 0, qtail = 0;
 pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
 void enqueue_event(const cache_event_t *e);
 void *solver_worker(void *arg);
 void init_worker(void);

// solver worker thread
 void *solver_worker(void *arg) {
    while (1) {
        pthread_mutex_lock(&qlock);
        if (qhead != qtail) {
            cache_event_t e = queue[qtail];
            qtail = (qtail + 1) % MAX_QUEUE;
            pthread_mutex_unlock(&qlock);

            // call solver (pseudo code)
            // if new testcase generated:
            char fname[128];
            snprintf(fname, sizeof(fname),
                     "results/cache_tests/case_%llu.bin", (unsigned long long)e.pc);
            FILE *f = fopen(fname, "wb");
            if (f) {
                // write generated input (placeholder)
                fwrite("FAKE_INPUT", 1, 10, f);
                fclose(f);
            }
        } else {
            pthread_mutex_unlock(&qlock);
            usleep(1000); // avoid busy loop
        }
    }
    return NULL;
}

void enqueue_event(const cache_event_t *e) {
    pthread_mutex_lock(&qlock);
    queue[qhead] = *e;
    qhead = (qhead + 1) % MAX_QUEUE;
    pthread_mutex_unlock(&qlock);
}

// init worker thread (call this once at startup)
__attribute__((constructor))
void init_worker(void) {
    pthread_t tid;
    pthread_create(&tid, NULL, solver_worker, NULL);
}


// load instrumentation
void HELPER(sym_ld)(CPUArchState *env, target_ulong addr, target_ulong pc) {
    // if (!expr_is_concrete(addr)) {s
        cache_event_t e = { .type = EVENT_SDA, .pc = pc, .addr = addr };
        enqueue_event(&e);
    // }
}

// store instrumentation
void HELPER(sym_st)(CPUArchState *env, target_ulong addr, target_ulong pc) {
    // if (!expr_is_concrete(addr)) {
        cache_event_t e = { .type = EVENT_SDA, .pc = pc, .addr = addr };
        enqueue_event(&e);
    // }
}

// branch instrumentation
void HELPER(sym_branch)(CPUArchState *env, target_ulong cond, target_ulong pc) {
    // if (!sym_is_symbolic(cond)) {
        cache_event_t e = { .type = EVENT_SCB, .pc = pc, .addr = 0 };
        enqueue_event(&e);
    // }
}