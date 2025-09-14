/* GLib WASM - Threading implementation using SharedArrayBuffer
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/atomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// Threading capabilities detection
EM_JS(int, threading_is_available, (), {
    return (typeof SharedArrayBuffer !== 'undefined' &&
            typeof Worker !== 'undefined' &&
            typeof Atomics !== 'undefined') ? 1 : 0;
});

EM_JS(void, threading_initialize_shared_memory, (), {
    if (typeof Module.sharedThreadingMemory === 'undefined') {
        // 64KB shared memory for thread coordination
        Module.sharedThreadingMemory = new SharedArrayBuffer(65536);
        Module.threadingData = new Int32Array(Module.sharedThreadingMemory);

        // Memory layout:
        // 0-1023: Mutexes (256 mutexes * 4 bytes each)
        // 1024-2047: Condition variables (256 conditions * 4 bytes each)
        // 2048-4095: Thread status (512 threads * 4 bytes each)
        // 4096+: General purpose atomic variables

        console.log('GLib threading shared memory initialized');
    }
});

EM_JS(int, threading_create_worker, (const char* worker_data), {
    if (!Module.sharedThreadingMemory) return -1;

    const workerCode = `
        let sharedMemory, threadingData;

        self.onmessage = function(e) {
            const { type, sharedBuffer, threadId } = e.data;

            if (type === 'init') {
                sharedMemory = sharedBuffer;
                threadingData = new Int32Array(sharedMemory);
                self.postMessage({ type: 'ready', threadId });
            } else if (type === 'execute') {
                // Execute thread function here
                // This would be expanded for actual thread execution
                self.postMessage({ type: 'finished', threadId });
            }
        };
    `;

    try {
        const blob = new Blob([workerCode], { type: 'application/javascript' });
        const worker = new Worker(URL.createObjectURL(blob));

        const threadId = Module.nextThreadId || 1;
        Module.nextThreadId = threadId + 1;

        Module.threadWorkers = Module.threadWorkers || {};
        Module.threadWorkers[threadId] = worker;

        worker.postMessage({
            type: 'init',
            sharedBuffer: Module.sharedThreadingMemory,
            threadId: threadId
        });

        return threadId;
    } catch (e) {
        console.error('Worker creation failed:', e);
        return -1;
    }
});

// Global threading state
static gboolean g_threading_initialized = FALSE;
static gboolean g_threading_available = FALSE;
static int g_next_mutex_id = 1;
static int g_next_cond_id = 1;

// Mutex structure
struct _GMutex {
    int id;
    int memory_index;
    gboolean is_shared;
};

// Condition variable structure
struct _GCond {
    int id;
    int memory_index;
    gboolean is_shared;
};

// Thread structure
struct _GThread {
    int id;
    int memory_index;
    pthread_t pthread_id;
    GThreadFunc func;
    gpointer data;
    gboolean joinable;
    gpointer result;
};

static void g_threading_init(void) {
    if (g_threading_initialized) return;

    g_threading_initialized = TRUE;
    g_threading_available = threading_is_available();

    if (g_threading_available) {
        threading_initialize_shared_memory();
        g_message("GLib threading initialized with SharedArrayBuffer support");
    } else {
        g_message("GLib threading initialized with single-thread fallback");
    }
}

// Mutex implementation
void g_mutex_init(GMutex *mutex) {
    g_return_if_fail(mutex != NULL);
    g_threading_init();

    mutex->id = g_next_mutex_id++;
    mutex->memory_index = mutex->id % 256; // Map to shared memory slot
    mutex->is_shared = g_threading_available;

    if (mutex->is_shared) {
        // Initialize mutex in shared memory (0 = unlocked, 1 = locked)
        EM_ASM({
            if (Module.threadingData) {
                Atomics.store(Module.threadingData, $0, 0);
            }
        }, mutex->memory_index);
    }
}

void g_mutex_clear(GMutex *mutex) {
    g_return_if_fail(mutex != NULL);

    if (mutex->is_shared) {
        // Clear mutex in shared memory
        EM_ASM({
            if (Module.threadingData) {
                Atomics.store(Module.threadingData, $0, 0);
            }
        }, mutex->memory_index);
    }
}

void g_mutex_lock(GMutex *mutex) {
    g_return_if_fail(mutex != NULL);

    if (!mutex->is_shared) {
        // Single-threaded fallback - no-op
        return;
    }

    // Atomic compare-and-swap lock
    while (TRUE) {
        int result = EM_ASM_INT({
            if (!Module.threadingData) return 1;

            // Try to acquire lock (compare 0 with 1)
            const oldValue = Atomics.compareExchange(Module.threadingData, $0, 0, 1);
            return oldValue; // Returns previous value
        }, mutex->memory_index);

        if (result == 0) {
            // Successfully acquired lock
            break;
        }

        // Wait for lock to become available
        EM_ASM({
            if (Module.threadingData) {
                Atomics.wait(Module.threadingData, $0, 1, 1); // Wait 1ms
            }
        }, mutex->memory_index);
    }
}

gboolean g_mutex_trylock(GMutex *mutex) {
    g_return_val_if_fail(mutex != NULL, FALSE);

    if (!mutex->is_shared) {
        return TRUE; // Single-threaded always succeeds
    }

    int result = EM_ASM_INT({
        if (!Module.threadingData) return 0;

        const oldValue = Atomics.compareExchange(Module.threadingData, $0, 0, 1);
        return (oldValue == 0) ? 1 : 0; // Return success/failure
    }, mutex->memory_index);

    return result == 1;
}

void g_mutex_unlock(GMutex *mutex) {
    g_return_if_fail(mutex != NULL);

    if (!mutex->is_shared) {
        return; // Single-threaded no-op
    }

    EM_ASM({
        if (Module.threadingData) {
            Atomics.store(Module.threadingData, $0, 0);
            Atomics.notify(Module.threadingData, $0, 1);
        }
    }, mutex->memory_index);
}

// Condition variable implementation
void g_cond_init(GCond *cond) {
    g_return_if_fail(cond != NULL);
    g_threading_init();

    cond->id = g_next_cond_id++;
    cond->memory_index = 1024 + (cond->id % 256); // Offset into cond section
    cond->is_shared = g_threading_available;

    if (cond->is_shared) {
        EM_ASM({
            if (Module.threadingData) {
                Atomics.store(Module.threadingData, $0, 0);
            }
        }, cond->memory_index);
    }
}

void g_cond_clear(GCond *cond) {
    g_return_if_fail(cond != NULL);

    if (cond->is_shared) {
        EM_ASM({
            if (Module.threadingData) {
                Atomics.store(Module.threadingData, $0, 0);
            }
        }, cond->memory_index);
    }
}

void g_cond_wait(GCond *cond, GMutex *mutex) {
    g_return_if_fail(cond != NULL);
    g_return_if_fail(mutex != NULL);

    if (!cond->is_shared || !mutex->is_shared) {
        return; // Single-threaded no-op
    }

    // Unlock mutex
    g_mutex_unlock(mutex);

    // Wait for condition signal
    EM_ASM({
        if (Module.threadingData) {
            while (Atomics.load(Module.threadingData, $0) == 0) {
                Atomics.wait(Module.threadingData, $0, 0, 1000); // Wait 1 second max
            }
            // Consume the signal
            Atomics.compareExchange(Module.threadingData, $0, 1, 0);
        }
    }, cond->memory_index);

    // Reacquire mutex
    g_mutex_lock(mutex);
}

gboolean g_cond_wait_until(GCond *cond, GMutex *mutex, gint64 end_time) {
    g_return_val_if_fail(cond != NULL, FALSE);
    g_return_val_if_fail(mutex != NULL, FALSE);

    if (!cond->is_shared || !mutex->is_shared) {
        return TRUE; // Single-threaded always succeeds
    }

    gint64 now = g_get_monotonic_time();
    if (now >= end_time) {
        return FALSE; // Already timed out
    }

    // Unlock mutex
    g_mutex_unlock(mutex);

    // Wait with timeout
    gboolean signaled = EM_ASM_INT({
        if (!Module.threadingData) return 1;

        const timeout = $1 - $2; // end_time - now (in microseconds)
        const timeoutMs = timeout / 1000;

        const result = Atomics.wait(Module.threadingData, $0, 0, timeoutMs);

        if (result === 'ok') {
            // Signal received, consume it
            Atomics.compareExchange(Module.threadingData, $0, 1, 0);
            return 1;
        }

        return 0; // Timeout or error
    }, cond->memory_index, end_time, now);

    // Reacquire mutex
    g_mutex_lock(mutex);

    return signaled;
}

void g_cond_signal(GCond *cond) {
    g_return_if_fail(cond != NULL);

    if (!cond->is_shared) {
        return; // Single-threaded no-op
    }

    EM_ASM({
        if (Module.threadingData) {
            Atomics.store(Module.threadingData, $0, 1);
            Atomics.notify(Module.threadingData, $0, 1);
        }
    }, cond->memory_index);
}

void g_cond_broadcast(GCond *cond) {
    g_return_if_fail(cond != NULL);

    if (!cond->is_shared) {
        return; // Single-threaded no-op
    }

    EM_ASM({
        if (Module.threadingData) {
            Atomics.store(Module.threadingData, $0, 1);
            Atomics.notify(Module.threadingData, $0); // Notify all waiters
        }
    }, cond->memory_index);
}

// Thread implementation using pthread + Web Workers
GThread* g_thread_new(const gchar *name, GThreadFunc func, gpointer data) {
    g_threading_init();

    GThread *thread = g_new0(GThread, 1);
    thread->func = func;
    thread->data = data;
    thread->joinable = TRUE;

    if (g_threading_available) {
        // Create Web Worker for thread
        thread->id = threading_create_worker(name);
        thread->memory_index = 2048 + (thread->id % 512);

        if (thread->id > 0) {
            // Mark thread as running in shared memory
            EM_ASM({
                if (Module.threadingData) {
                    Atomics.store(Module.threadingData, $0, 1); // 1 = running
                }
            }, thread->memory_index);
        }
    } else {
        // Single-threaded fallback - execute immediately
        thread->result = func(data);
        thread->id = 1;
    }

    return thread;
}

GThread* g_thread_try_new(const gchar *name, GThreadFunc func, gpointer data, GError **error) {
    GThread *thread = g_thread_new(name, func, data);

    if (!thread || (g_threading_available && thread->id <= 0)) {
        if (error) {
            g_set_error(error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN,
                       "Failed to create thread '%s'", name ? name : "unnamed");
        }
        if (thread) {
            g_free(thread);
        }
        return NULL;
    }

    return thread;
}

gpointer g_thread_join(GThread *thread) {
    g_return_val_if_fail(thread != NULL, NULL);
    g_return_val_if_fail(thread->joinable, NULL);

    if (!g_threading_available) {
        // Single-threaded - result already available
        gpointer result = thread->result;
        g_free(thread);
        return result;
    }

    // Wait for thread completion
    while (TRUE) {
        int status = EM_ASM_INT({
            if (Module.threadingData) {
                return Atomics.load(Module.threadingData, $0);
            }
            return 2; // Assume finished if no shared memory
        }, thread->memory_index);

        if (status == 2) { // 2 = finished
            break;
        }

        // Wait for status change
        EM_ASM({
            if (Module.threadingData) {
                Atomics.wait(Module.threadingData, $0, 1, 100); // Wait 100ms
            }
        }, thread->memory_index);
    }

    gpointer result = thread->result;
    g_free(thread);
    return result;
}

GThread* g_thread_self(void) {
    // In browser context, always return main thread
    static GThread main_thread = { .id = 0, .joinable = FALSE };
    return &main_thread;
}

void g_thread_exit(gpointer retval) {
    // Set return value and exit
    emscripten_force_exit(0);
}

void g_thread_yield(void) {
    if (g_threading_available) {
        // Yield to other threads
        emscripten_sleep(0);
    }
}

// Atomic operations
gint g_atomic_int_get(volatile gint *atomic) {
    if (g_threading_available) {
        return EM_ASM_INT({
            if (Module.threadingData) {
                return Atomics.load(Module.HEAP32, $0 >> 2);
            }
            return getValue($0, 'i32');
        }, atomic);
    }
    return *atomic;
}

void g_atomic_int_set(volatile gint *atomic, gint newval) {
    if (g_threading_available) {
        EM_ASM({
            if (Module.threadingData) {
                Atomics.store(Module.HEAP32, $0 >> 2, $1);
            } else {
                setValue($0, $1, 'i32');
            }
        }, atomic, newval);
    } else {
        *atomic = newval;
    }
}

gint g_atomic_int_add(volatile gint *atomic, gint val) {
    if (g_threading_available) {
        return EM_ASM_INT({
            if (Module.threadingData) {
                return Atomics.add(Module.HEAP32, $0 >> 2, $1);
            }
            const oldVal = getValue($0, 'i32');
            setValue($0, oldVal + $1, 'i32');
            return oldVal;
        }, atomic, val);
    }

    gint old_val = *atomic;
    *atomic += val;
    return old_val;
}

gboolean g_atomic_int_compare_and_exchange(volatile gint *atomic, gint oldval, gint newval) {
    if (g_threading_available) {
        return EM_ASM_INT({
            if (Module.threadingData) {
                const result = Atomics.compareExchange(Module.HEAP32, $0 >> 2, $1, $2);
                return (result === $1) ? 1 : 0;
            }
            const current = getValue($0, 'i32');
            if (current === $1) {
                setValue($0, $2, 'i32');
                return 1;
            }
            return 0;
        }, atomic, oldval, newval);
    }

    if (*atomic == oldval) {
        *atomic = newval;
        return TRUE;
    }
    return FALSE;
}

// Cleanup function
void g_threading_wasm_cleanup(void) {
    // Cleanup threads and shared memory
    if (g_threading_available) {
        EM_ASM({
            if (Module.threadWorkers) {
                for (const threadId in Module.threadWorkers) {
                    Module.threadWorkers[threadId].terminate();
                }
                delete Module.threadWorkers;
            }
            delete Module.sharedThreadingMemory;
            delete Module.threadingData;
        });
    }

    g_threading_initialized = FALSE;
    g_threading_available = FALSE;
    g_next_mutex_id = 1;
    g_next_cond_id = 1;
}