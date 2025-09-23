/* GLib WASM - Threading Implementation Test
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include "wasm/web_native_capabilities.h"

// Forward declarations from web_native_threading.c
extern GThread* g_web_thread_new_intelligent(const gchar *name, GThreadFunc func, gpointer data);
extern gpointer g_web_thread_join_intelligent(GThread *thread);
extern gint g_web_get_threading_strategy(void);
extern gdouble g_web_threading_benchmark(gint thread_count);
extern void g_web_threading_get_stats(gint *active_threads, gint *total_created, gint *strategy, gdouble *avg_creation_time);
extern gboolean g_web_is_main_thread(void);
extern void g_web_threading_force_warmup(gint num_threads);

/**
 * Simple worker function for testing
 */
static gpointer test_worker(gpointer data) {
    gint work_id = GPOINTER_TO_INT(data);

    g_debug("Worker %d starting work", work_id);

    // Do some simple computation
    volatile gint sum = 0;
    for (gint i = 0; i < 1000; i++) {
        sum += i * work_id;
    }

    g_usleep(10000); // 10ms of work

    g_debug("Worker %d completed (result: %d)", work_id, sum);

    return GINT_TO_POINTER(sum);
}

/**
 * Test basic threading functionality
 */
EMSCRIPTEN_KEEPALIVE
gboolean test_threading_basic(void) {
    g_message("Testing basic GLib threading functionality...");

    // Test capability detection
    const GWebCapabilities* caps = g_web_get_capabilities();
    g_message("Threading capabilities:");
    g_message("  Pthread Support: %s", caps->has_pthread_support ? "YES" : "NO");
    g_message("  PROXY_TO_PTHREAD: %s", caps->has_proxy_to_pthread ? "YES" : "NO");
    g_message("  Web Workers: %s", caps->has_web_workers ? "YES" : "NO");
    g_message("  SharedArrayBuffer: %s", caps->has_shared_array_buffer ? "YES" : "NO");
    g_message("  Max Worker Threads: %d", caps->max_worker_threads);

    // Test strategy selection
    gint strategy = g_web_get_threading_strategy();
    g_message("Selected threading strategy: %d", strategy);

    // Test main thread detection
    gboolean is_main = g_web_is_main_thread();
    g_message("Is main thread: %s", is_main ? "YES" : "NO");

    // Test thread creation and joining
    g_message("Creating test worker threads...");

    const gint num_threads = 3;
    GPtrArray *threads = g_ptr_array_new();

    // Create worker threads
    for (gint i = 0; i < num_threads; i++) {
        gchar *name = g_strdup_printf("test-worker-%d", i);
        GThread *thread = g_web_thread_new_intelligent(name, test_worker, GINT_TO_POINTER(i + 1));

        if (thread) {
            g_ptr_array_add(threads, thread);
            g_message("Created thread: %s", name);
        } else {
            g_warning("Failed to create thread: %s", name);
        }
        g_free(name);
    }

    // Join all threads and collect results
    gint total_result = 0;
    for (guint i = 0; i < threads->len; i++) {
        GThread *thread = g_ptr_array_index(threads, i);
        gpointer result = g_web_thread_join_intelligent(thread);
        gint worker_result = GPOINTER_TO_INT(result);
        total_result += worker_result;
        g_message("Thread %u joined with result: %d", i, worker_result);
    }

    g_ptr_array_free(threads, TRUE);

    g_message("All threads completed. Total result: %d", total_result);

    return TRUE;
}

/**
 * Test threading statistics
 */
EMSCRIPTEN_KEEPALIVE
void test_threading_stats(void) {
    g_message("Testing threading statistics...");

    gint active_threads = 0;
    gint total_created = 0;
    gint strategy = 0;
    gdouble avg_creation_time = 0.0;

    g_web_threading_get_stats(&active_threads, &total_created, &strategy, &avg_creation_time);

    g_message("Threading Statistics:");
    g_message("  Active threads: %d", active_threads);
    g_message("  Total created: %d", total_created);
    g_message("  Strategy: %d", strategy);
    g_message("  Avg creation time: %.2f ms", avg_creation_time);
}

/**
 * Test thread pool warmup
 */
EMSCRIPTEN_KEEPALIVE
void test_thread_warmup(void) {
    g_message("Testing thread pool warmup...");

    // Force warmup with 4 threads
    g_web_threading_force_warmup(4);

    g_message("Thread pool warmup completed");
}

/**
 * Test threading performance benchmark
 */
EMSCRIPTEN_KEEPALIVE
gdouble test_threading_benchmark(void) {
    g_message("Running threading performance benchmark...");

    gdouble avg_time = g_web_threading_benchmark(4);

    g_message("Benchmark completed - average creation time: %.2f ms", avg_time);

    return avg_time;
}

/**
 * Main test entry point
 */
EMSCRIPTEN_KEEPALIVE
gboolean test_all_threading(void) {
    g_message("=== GLib WASM Threading Implementation Test ===");

    // Initialize GLib
    extern int glib_wasm_init(void);
    if (!glib_wasm_init()) {
        g_error("Failed to initialize GLib WASM");
        return FALSE;
    }

    // Run all tests
    gboolean basic_success = test_threading_basic();
    test_threading_stats();
    test_thread_warmup();
    gdouble benchmark_time = test_threading_benchmark();

    g_message("=== Threading Test Summary ===");
    g_message("Basic threading: %s", basic_success ? "PASSED" : "FAILED");
    g_message("Benchmark time: %.2f ms", benchmark_time);
    g_message("=== Test Complete ===");

    return basic_success;
}

// For testing from browser console
int main() {
    return test_all_threading() ? 0 : 1;
}