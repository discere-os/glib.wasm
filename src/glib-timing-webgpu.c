/* GLib WASM - WebGPU-enhanced timing implementation
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

#include "webgpu-integration.h"
#include <glib.h>
#include <emscripten/emscripten.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

// GTimer structure with WebGPU enhancement
struct _GTimer {
    guint64 start;
    guint64 end;
    guint active : 1;
    gboolean use_webgpu_timing;
};

// Global timing context
static gboolean g_timing_initialized = FALSE;
static gboolean g_webgpu_timing_available = FALSE;

// Initialize timing subsystem
static void g_timing_init(void) {
    if (g_timing_initialized) return;

    g_timing_initialized = TRUE;

    // Try to initialize WebGPU for high-precision timing
    // This will work standalone or with external orchestrator
    if (g_webgpu_initialize(NULL)) {
        g_webgpu_timing_available = g_webgpu_has_timestamp_queries();

        if (g_webgpu_timing_available) {
            g_message("GLib timing initialized with WebGPU precision");
        } else {
            g_message("GLib timing initialized with WebGPU (no timestamp queries)");
        }
    } else {
        g_message("GLib timing initialized with standard precision");
    }
}

// Enhanced g_get_monotonic_time with WebGPU support
gint64 g_get_monotonic_time(void) {
    g_timing_init();

    if (g_webgpu_timing_available) {
        // Use WebGPU high-precision timing
        double ns_time = g_get_monotonic_time_ns();
        return (gint64)(ns_time / 1000.0); // Convert to microseconds
    }

    // Fallback to JavaScript performance.now()
    double ms_time = EM_ASM_DOUBLE({
        return performance.now();
    });

    return (gint64)(ms_time * 1000.0); // Convert to microseconds
}

// Enhanced g_get_real_time with WebGPU coordination
gint64 g_get_real_time(void) {
    g_timing_init();

    if (g_webgpu_has_shared_context()) {
        // Coordinate with external orchestrator for synchronized time
        void* sync_time_resource = NULL;
        if (g_webgpu_coordinate_resource("synchronized_time", sizeof(gint64), &sync_time_resource)) {
            // Use orchestrator's synchronized time if available
            return *(gint64*)sync_time_resource;
        }
    }

    // Standard implementation
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((gint64) tv.tv_sec) * 1000000) + tv.tv_usec;
}

// Enhanced GTimer implementation
GTimer* g_timer_new(void) {
    g_timing_init();

    GTimer* timer = g_new0(GTimer, 1);

    timer->use_webgpu_timing = g_webgpu_timing_available;
    timer->active = FALSE;

    return timer;
}

void g_timer_start(GTimer *timer) {
    g_return_if_fail(timer != NULL);

    timer->active = TRUE;

    if (timer->use_webgpu_timing) {
        timer->start = (guint64)g_get_monotonic_time_ns();
    } else {
        timer->start = (guint64)g_get_monotonic_time();
    }
}

void g_timer_stop(GTimer *timer) {
    g_return_if_fail(timer != NULL);

    timer->active = FALSE;

    if (timer->use_webgpu_timing) {
        timer->end = (guint64)g_get_monotonic_time_ns();
    } else {
        timer->end = (guint64)g_get_monotonic_time();
    }
}

gdouble g_timer_elapsed(GTimer *timer, gulong *microseconds) {
    gdouble total;
    guint64 elapsed;

    g_return_val_if_fail(timer != NULL, 0);

    if (timer->active) {
        if (timer->use_webgpu_timing) {
            guint64 now = (guint64)g_get_monotonic_time_ns();
            elapsed = now - timer->start;
            // Convert from nanoseconds to microseconds
            elapsed = elapsed / 1000;
        } else {
            guint64 now = (guint64)g_get_monotonic_time();
            elapsed = now - timer->start;
        }
    } else {
        if (timer->use_webgpu_timing) {
            elapsed = (timer->end - timer->start) / 1000; // ns to μs
        } else {
            elapsed = timer->end - timer->start;
        }
    }

    total = elapsed / 1e6;

    if (microseconds) {
        *microseconds = elapsed % 1000000;
    }

    return total;
}

void g_timer_continue(GTimer *timer) {
    guint64 elapsed;

    g_return_if_fail(timer != NULL);
    g_return_if_fail(timer->active == FALSE);

    if (timer->use_webgpu_timing) {
        elapsed = (timer->end - timer->start) / 1000; // Convert ns to μs
        timer->start = (guint64)g_get_monotonic_time_ns() - (elapsed * 1000); // Convert back to ns
    } else {
        elapsed = timer->end - timer->start;
        timer->start = (guint64)g_get_monotonic_time() - elapsed;
    }

    timer->active = TRUE;
}

void g_timer_reset(GTimer *timer) {
    g_return_if_fail(timer != NULL);

    if (timer->use_webgpu_timing) {
        timer->start = (guint64)g_get_monotonic_time_ns();
    } else {
        timer->start = (guint64)g_get_monotonic_time();
    }

    timer->active = TRUE;
}

void g_timer_destroy(GTimer *timer) {
    g_return_if_fail(timer != NULL);
    g_free(timer);
}

// Enhanced g_usleep with WebGPU coordination
void g_usleep(gulong microseconds) {
    g_timing_init();

    if (g_webgpu_has_shared_context()) {
        // Coordinate with external orchestrator for efficient sleeping
        EM_ASM({
            const microseconds = $0;

            if (Module.externalWebGPUContext &&
                Module.externalWebGPUContext.coordinatedSleep) {
                // Use orchestrator's sleep coordination if available
                Module.externalWebGPUContext.coordinatedSleep(microseconds);
                return;
            }

            // Fallback to regular sleep
            if (microseconds >= 16000) { // >= 16ms, use requestAnimationFrame
                let remaining = microseconds / 1000; // Convert to ms

                function waitFrame() {
                    if (remaining <= 16) {
                        setTimeout(() => {}, remaining);
                    } else {
                        remaining -= 16;
                        requestAnimationFrame(waitFrame);
                    }
                }
                requestAnimationFrame(waitFrame);
            } else {
                setTimeout(() => {}, microseconds / 1000);
            }
        }, microseconds);
    } else {
        // Standard Emscripten sleep implementation
        emscripten_sleep(microseconds / 1000);
    }
}

// Timeout source integration with WebGPU timing
static guint g_timeout_source_id = 1;

typedef struct {
    guint id;
    guint interval;
    GSourceFunc function;
    gpointer data;
    GDestroyNotify notify;
    gboolean active;
    gboolean use_webgpu_coordination;
} GTimeoutSource;

static GHashTable* g_timeout_sources = NULL;

static void ensure_timeout_table(void) {
    if (!g_timeout_sources) {
        g_timeout_sources = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                                  NULL, g_free);
    }
}

guint g_timeout_add_full(gint priority,
                        guint interval,
                        GSourceFunc function,
                        gpointer data,
                        GDestroyNotify notify) {
    g_timing_init();
    ensure_timeout_table();

    guint source_id = g_timeout_source_id++;

    GTimeoutSource* source = g_new0(GTimeoutSource, 1);
    source->id = source_id;
    source->interval = interval;
    source->function = function;
    source->data = data;
    source->notify = notify;
    source->active = TRUE;
    source->use_webgpu_coordination = g_webgpu_has_shared_context();

    g_hash_table_insert(g_timeout_sources, GUINT_TO_POINTER(source_id), source);

    // Set up JavaScript timeout with WebGPU coordination
    EM_ASM({
        const sourceId = $0;
        const interval = $1;
        const useCoordination = $2;

        function timeoutCallback() {
            // Call back into C to execute the timeout function
            const shouldContinue = Module._g_timeout_source_execute(sourceId);

            if (shouldContinue) {
                // Reschedule with potential orchestrator coordination
                if (useCoordination && Module.externalWebGPUContext &&
                    Module.externalWebGPUContext.scheduleTimeout) {
                    Module.externalWebGPUContext.scheduleTimeout(timeoutCallback, interval);
                } else {
                    setTimeout(timeoutCallback, interval);
                }
            }
        }

        // Initial scheduling
        if (useCoordination && Module.externalWebGPUContext &&
            Module.externalWebGPUContext.scheduleTimeout) {
            Module.externalWebGPUContext.scheduleTimeout(timeoutCallback, interval);
        } else {
            setTimeout(timeoutCallback, interval);
        }
    }, source_id, interval, source->use_webgpu_coordination);

    return source_id;
}

guint g_timeout_add(guint interval, GSourceFunc function, gpointer data) {
    return g_timeout_add_full(G_PRIORITY_DEFAULT, interval, function, data, NULL);
}

// Called from JavaScript to execute timeout source
EMSCRIPTEN_KEEPALIVE
gboolean g_timeout_source_execute(guint source_id) {
    if (!g_timeout_sources) return FALSE;

    GTimeoutSource* source = g_hash_table_lookup(g_timeout_sources, GUINT_TO_POINTER(source_id));
    if (!source || !source->active) return FALSE;

    gboolean should_continue = source->function(source->data);

    if (!should_continue) {
        // Remove source
        g_hash_table_remove(g_timeout_sources, GUINT_TO_POINTER(source_id));
    }

    return should_continue;
}

gboolean g_source_remove(guint tag) {
    if (!g_timeout_sources) return FALSE;

    GTimeoutSource* source = g_hash_table_lookup(g_timeout_sources, GUINT_TO_POINTER(tag));
    if (source) {
        source->active = FALSE;
        if (source->notify) {
            source->notify(source->data);
        }
        return g_hash_table_remove(g_timeout_sources, GUINT_TO_POINTER(tag));
    }

    return FALSE;
}

// Module cleanup
void g_timing_webgpu_cleanup(void) {
    if (g_timeout_sources) {
        g_hash_table_destroy(g_timeout_sources);
        g_timeout_sources = NULL;
    }

    g_webgpu_cleanup();
    g_timing_initialized = FALSE;
    g_webgpu_timing_available = FALSE;
}