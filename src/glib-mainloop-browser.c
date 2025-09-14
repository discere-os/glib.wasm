/* GLib WASM - Browser-native main loop implementation
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
#include <emscripten/html5.h>
#include <stdlib.h>
#include <string.h>

// Browser capabilities detection
EM_JS(int, browser_mainloop_is_available, (), {
    return (typeof requestAnimationFrame !== 'undefined' &&
            typeof MessageChannel !== 'undefined' &&
            typeof performance !== 'undefined') ? 1 : 0;
});

EM_JS(void, browser_mainloop_initialize, (), {
    if (typeof Module.glibMainLoop === 'undefined') {
        Module.glibMainLoop = {
            running: false,
            rafId: null,
            sources: {},
            nextSourceId: 1,
            messageChannel: new MessageChannel(),
            highPriorityQueue: [],
            normalPriorityQueue: [],
            idleQueue: [],
            frameCount: 0
        };

        // Set up MessageChannel for immediate dispatch
        Module.glibMainLoop.messageChannel.port2.onmessage = function() {
            Module._g_main_loop_process_immediate_sources();
        };

        console.log('GLib browser main loop initialized');
    }
});

EM_JS(void, browser_mainloop_start_raf, (), {
    if (!Module.glibMainLoop || Module.glibMainLoop.running) return;

    Module.glibMainLoop.running = true;

    function rafCallback(timestamp) {
        if (!Module.glibMainLoop.running) return;

        Module.glibMainLoop.frameCount++;

        // Call into C to process sources
        Module._g_main_loop_process_frame(timestamp);

        // Schedule next frame
        Module.glibMainLoop.rafId = requestAnimationFrame(rafCallback);
    }

    Module.glibMainLoop.rafId = requestAnimationFrame(rafCallback);
});

EM_JS(void, browser_mainloop_stop_raf, (), {
    if (!Module.glibMainLoop) return;

    Module.glibMainLoop.running = false;

    if (Module.glibMainLoop.rafId) {
        cancelAnimationFrame(Module.glibMainLoop.rafId);
        Module.glibMainLoop.rafId = null;
    }
});

EM_JS(int, browser_mainloop_add_timeout, (int interval, int priority, int source_id), {
    if (!Module.glibMainLoop) return -1;

    const source = {
        id: source_id,
        type: 'timeout',
        interval: interval,
        priority: priority,
        lastExecuted: performance.now(),
        active: true
    };

    Module.glibMainLoop.sources[source_id] = source;

    // Add to appropriate priority queue
    if (priority < 0) {
        Module.glibMainLoop.highPriorityQueue.push(source);
    } else if (priority < 100) {
        Module.glibMainLoop.normalPriorityQueue.push(source);
    } else {
        Module.glibMainLoop.idleQueue.push(source);
    }

    return source_id;
});

EM_JS(int, browser_mainloop_add_idle, (int priority, int source_id), {
    if (!Module.glibMainLoop) return -1;

    const source = {
        id: source_id,
        type: 'idle',
        priority: priority,
        active: true
    };

    Module.glibMainLoop.sources[source_id] = source;
    Module.glibMainLoop.idleQueue.push(source);

    return source_id;
});

EM_JS(void, browser_mainloop_remove_source, (int source_id), {
    if (!Module.glibMainLoop || !Module.glibMainLoop.sources[source_id]) return;

    const source = Module.glibMainLoop.sources[source_id];
    source.active = false;

    // Remove from queues
    const removeFromArray = (array) => {
        const index = array.findIndex(s => s.id === source_id);
        if (index > -1) array.splice(index, 1);
    };

    removeFromArray(Module.glibMainLoop.highPriorityQueue);
    removeFromArray(Module.glibMainLoop.normalPriorityQueue);
    removeFromArray(Module.glibMainLoop.idleQueue);

    delete Module.glibMainLoop.sources[source_id];
});

EM_JS(void, browser_mainloop_dispatch_immediate, (), {
    if (Module.glibMainLoop && Module.glibMainLoop.messageChannel) {
        Module.glibMainLoop.messageChannel.port1.postMessage(null);
    }
});

// Main loop structures
struct _GMainLoop {
    GMainContext *context;
    gboolean is_running;
    gint ref_count;
};

struct _GMainContext {
    gint ref_count;
    GPtrArray *sources;
    gint next_source_id;
    gboolean is_running;
};

// Source callback structure
typedef struct {
    guint source_id;
    gint priority;
    GSourceFunc callback;
    gpointer data;
    GDestroyNotify notify;
    gboolean active;
} GLibSourceInfo;

// Global state
static gboolean g_mainloop_initialized = FALSE;
static gboolean g_browser_mainloop_available = FALSE;
static GMainContext *g_default_context = NULL;
static GMainLoop *g_current_loop = NULL;
static GHashTable *g_source_callbacks = NULL;
static guint g_next_source_id = 1;

static void g_mainloop_init(void) {
    if (g_mainloop_initialized) return;

    g_mainloop_initialized = TRUE;
    g_browser_mainloop_available = browser_mainloop_is_available();

    if (g_browser_mainloop_available) {
        browser_mainloop_initialize();
        g_message("GLib main loop initialized with browser integration");
    } else {
        g_message("GLib main loop initialized with fallback implementation");
    }

    // Initialize source callback table
    g_source_callbacks = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                               NULL, g_free);

    // Create default context
    g_default_context = g_main_context_new();
}

// Main context implementation
GMainContext* g_main_context_new(void) {
    g_mainloop_init();

    GMainContext *context = g_new0(GMainContext, 1);
    context->ref_count = 1;
    context->sources = g_ptr_array_new();
    context->next_source_id = 1;
    context->is_running = FALSE;

    return context;
}

GMainContext* g_main_context_ref(GMainContext *context) {
    g_return_val_if_fail(context != NULL, NULL);
    g_atomic_int_inc(&context->ref_count);
    return context;
}

void g_main_context_unref(GMainContext *context) {
    g_return_if_fail(context != NULL);

    if (g_atomic_int_dec_and_test(&context->ref_count)) {
        g_ptr_array_free(context->sources, TRUE);
        g_free(context);
    }
}

GMainContext* g_main_context_default(void) {
    g_mainloop_init();
    return g_default_context;
}

gboolean g_main_context_iteration(GMainContext *context, gboolean may_block) {
    if (context == NULL) {
        context = g_default_context;
    }

    if (!g_browser_mainloop_available) {
        // Fallback: process one emscripten iteration
        emscripten_sleep(may_block ? 16 : 0); // 16ms ≈ 60fps
        return TRUE;
    }

    // Browser main loop handles iteration automatically
    return context->is_running;
}

gboolean g_main_context_pending(GMainContext *context) {
    // In browser context, there are always potential events
    return TRUE;
}

// Main loop implementation
GMainLoop* g_main_loop_new(GMainContext *context, gboolean is_running) {
    g_mainloop_init();

    if (context == NULL) {
        context = g_default_context;
    }

    GMainLoop *loop = g_new0(GMainLoop, 1);
    loop->context = g_main_context_ref(context);
    loop->is_running = is_running;
    loop->ref_count = 1;

    if (is_running) {
        g_current_loop = loop;
        context->is_running = TRUE;

        if (g_browser_mainloop_available) {
            browser_mainloop_start_raf();
        }
    }

    return loop;
}

GMainLoop* g_main_loop_ref(GMainLoop *loop) {
    g_return_val_if_fail(loop != NULL, NULL);
    g_atomic_int_inc(&loop->ref_count);
    return loop;
}

void g_main_loop_unref(GMainLoop *loop) {
    g_return_if_fail(loop != NULL);

    if (g_atomic_int_dec_and_test(&loop->ref_count)) {
        g_main_context_unref(loop->context);
        g_free(loop);
    }
}

void g_main_loop_run(GMainLoop *loop) {
    g_return_if_fail(loop != NULL);

    loop->is_running = TRUE;
    loop->context->is_running = TRUE;
    g_current_loop = loop;

    if (g_browser_mainloop_available) {
        browser_mainloop_start_raf();
    } else {
        // Fallback: use emscripten main loop
        emscripten_set_main_loop(g_main_context_iteration, 60, 1);
    }
}

void g_main_loop_quit(GMainLoop *loop) {
    g_return_if_fail(loop != NULL);

    loop->is_running = FALSE;
    loop->context->is_running = FALSE;

    if (g_browser_mainloop_available) {
        browser_mainloop_stop_raf();
    } else {
        emscripten_cancel_main_loop();
    }

    if (g_current_loop == loop) {
        g_current_loop = NULL;
    }
}

gboolean g_main_loop_is_running(GMainLoop *loop) {
    g_return_val_if_fail(loop != NULL, FALSE);
    return loop->is_running;
}

GMainContext* g_main_loop_get_context(GMainLoop *loop) {
    g_return_val_if_fail(loop != NULL, NULL);
    return loop->context;
}

// Source implementation
guint g_timeout_add_full(gint priority, guint interval, GSourceFunc function,
                        gpointer data, GDestroyNotify notify) {
    g_mainloop_init();

    guint source_id = g_next_source_id++;

    GLibSourceInfo *info = g_new0(GLibSourceInfo, 1);
    info->source_id = source_id;
    info->priority = priority;
    info->callback = function;
    info->data = data;
    info->notify = notify;
    info->active = TRUE;

    g_hash_table_insert(g_source_callbacks, GUINT_TO_POINTER(source_id), info);

    if (g_browser_mainloop_available) {
        browser_mainloop_add_timeout(interval, priority, source_id);
    } else {
        // Fallback timeout implementation would go here
    }

    return source_id;
}

guint g_timeout_add(guint interval, GSourceFunc function, gpointer data) {
    return g_timeout_add_full(G_PRIORITY_DEFAULT, interval, function, data, NULL);
}

guint g_idle_add_full(gint priority, GSourceFunc function, gpointer data, GDestroyNotify notify) {
    g_mainloop_init();

    guint source_id = g_next_source_id++;

    GLibSourceInfo *info = g_new0(GLibSourceInfo, 1);
    info->source_id = source_id;
    info->priority = priority;
    info->callback = function;
    info->data = data;
    info->notify = notify;
    info->active = TRUE;

    g_hash_table_insert(g_source_callbacks, GUINT_TO_POINTER(source_id), info);

    if (g_browser_mainloop_available) {
        browser_mainloop_add_idle(priority, source_id);
    }

    return source_id;
}

guint g_idle_add(GSourceFunc function, gpointer data) {
    return g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, function, data, NULL);
}

gboolean g_source_remove(guint tag) {
    GLibSourceInfo *info = g_hash_table_lookup(g_source_callbacks, GUINT_TO_POINTER(tag));
    if (!info || !info->active) {
        return FALSE;
    }

    info->active = FALSE;

    if (g_browser_mainloop_available) {
        browser_mainloop_remove_source(tag);
    }

    if (info->notify) {
        info->notify(info->data);
    }

    g_hash_table_remove(g_source_callbacks, GUINT_TO_POINTER(tag));
    return TRUE;
}

// Immediate dispatch for high-priority sources
void g_main_context_invoke_full(GMainContext *context, gint priority,
                                GSourceFunc function, gpointer data, GDestroyNotify notify) {
    // Add as immediate source and trigger dispatch
    guint source_id = g_idle_add_full(priority, function, data, notify);

    if (g_browser_mainloop_available) {
        browser_mainloop_dispatch_immediate();
    }
}

void g_main_context_invoke(GMainContext *context, GSourceFunc function, gpointer data) {
    g_main_context_invoke_full(context, G_PRIORITY_DEFAULT, function, data, NULL);
}

// Called from JavaScript to process frame
EMSCRIPTEN_KEEPALIVE
void g_main_loop_process_frame(double timestamp) {
    if (!g_current_loop || !g_current_loop->is_running) return;

    // Process high-priority sources first
    EM_ASM({
        if (!Module.glibMainLoop) return;

        const processQueue = (queue, maxProcessing) => {
            let processed = 0;
            while (queue.length > 0 && processed < maxProcessing) {
                const source = queue.shift();
                if (!source.active) continue;

                if (source.type === 'timeout') {
                    const now = performance.now();
                    if (now - source.lastExecuted >= source.interval) {
                        const shouldContinue = Module._g_main_loop_execute_source(source.id);
                        if (shouldContinue) {
                            source.lastExecuted = now;
                            queue.push(source); // Reschedule
                        }
                        processed++;
                    } else {
                        queue.push(source); // Reschedule for later
                    }
                } else if (source.type === 'idle') {
                    const shouldContinue = Module._g_main_loop_execute_source(source.id);
                    if (shouldContinue) {
                        queue.push(source); // Reschedule
                    }
                    processed++;
                }
            }
        };

        // Process in priority order
        processQueue(Module.glibMainLoop.highPriorityQueue, 5);
        processQueue(Module.glibMainLoop.normalPriorityQueue, 10);

        // Process idle sources if we have spare time (< 10ms frame time)
        const frameStartTime = performance.now();
        if (timestamp - frameStartTime < 10) {
            processQueue(Module.glibMainLoop.idleQueue, 3);
        }
    });
}

// Called from JavaScript to process immediate sources
EMSCRIPTEN_KEEPALIVE
void g_main_loop_process_immediate_sources(void) {
    // Process immediate message queue sources
    // Implementation would depend on specific immediate source requirements
}

// Called from JavaScript to execute a source callback
EMSCRIPTEN_KEEPALIVE
gboolean g_main_loop_execute_source(guint source_id) {
    GLibSourceInfo *info = g_hash_table_lookup(g_source_callbacks, GUINT_TO_POINTER(source_id));
    if (!info || !info->active) {
        return FALSE;
    }

    gboolean should_continue = FALSE;

    if (info->callback) {
        should_continue = info->callback(info->data);
    }

    if (!should_continue) {
        // Remove the source
        g_source_remove(source_id);
    }

    return should_continue;
}

// Cleanup function
void g_mainloop_browser_cleanup(void) {
    if (g_current_loop) {
        g_main_loop_quit(g_current_loop);
    }

    if (g_source_callbacks) {
        g_hash_table_destroy(g_source_callbacks);
        g_source_callbacks = NULL;
    }

    if (g_default_context) {
        g_main_context_unref(g_default_context);
        g_default_context = NULL;
    }

    if (g_browser_mainloop_available) {
        browser_mainloop_stop_raf();
    }

    g_mainloop_initialized = FALSE;
    g_browser_mainloop_available = FALSE;
    g_next_source_id = 1;
}