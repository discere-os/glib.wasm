/* GLib WASM - Web-Native Main Loop Implementation
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This implements GMainLoop using browser's native event loop
 * for perfect 60fps rendering and optimal performance.
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// Forward declarations
#include "web_native_capabilities.h"

// Web-native main loop state
typedef struct {
    GMainContext *context;
    gboolean is_running;
    gboolean simulate_infinite_loop;
    gint64 last_frame_time;
    gint frame_count;
    gdouble fps_target;

    // Performance tracking
    gint64 total_iterations;
    gint64 total_time_spent;
} GWebMainLoop;

static GWebMainLoop *web_main_loop = NULL;
static gboolean web_mainloop_initialized = FALSE;

// JavaScript integration for browser event loop
EM_JS(void, setup_request_animation_frame, (void* loop_ptr), {
    Module.glibMainLoop = Module.glibMainLoop || {};
    Module.glibMainLoop.loopPtr = loop_ptr;
    Module.glibMainLoop.frameId = null;

    Module.glibMainLoop.iterate = function() {
        if (Module.glibMainLoop.loopPtr) {
            Module._g_web_main_context_iteration(Module.glibMainLoop.loopPtr);
        }
    };

    Module.glibMainLoop.start = function() {
        function frame() {
            Module.glibMainLoop.iterate();
            Module.glibMainLoop.frameId = requestAnimationFrame(frame);
        }
        Module.glibMainLoop.frameId = requestAnimationFrame(frame);
    };

    Module.glibMainLoop.stop = function() {
        if (Module.glibMainLoop.frameId) {
            cancelAnimationFrame(Module.glibMainLoop.frameId);
            Module.glibMainLoop.frameId = null;
        }
    };

    console.log('[GLib.wasm] Browser main loop integration ready');
});

EM_JS(void, start_browser_main_loop, (), {
    if (Module.glibMainLoop && Module.glibMainLoop.start) {
        Module.glibMainLoop.start();
    }
});

EM_JS(void, stop_browser_main_loop, (), {
    if (Module.glibMainLoop && Module.glibMainLoop.stop) {
        Module.glibMainLoop.stop();
    }
});

EM_JS(double, get_performance_now, (), {
    return typeof performance !== 'undefined' ? performance.now() : Date.now();
});

/**
 * Get current time with high precision
 */
static gint64 g_web_get_monotonic_time_us(void) {
    return (gint64)(get_performance_now() * 1000.0);  // Convert ms to μs
}

/**
 * Web-native main context iteration - called by browser RAF
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_main_context_iteration(void *loop_ptr) {
    GWebMainLoop *web_loop = (GWebMainLoop*)loop_ptr;
    if (!web_loop || !web_loop->context) {
        return FALSE;
    }

    gint64 start_time = g_web_get_monotonic_time_us();

    // Run one iteration of the GLib main context
    gboolean something_dispatched = g_main_context_iteration(web_loop->context, FALSE);

    // Update performance metrics
    gint64 end_time = g_web_get_monotonic_time_us();
    web_loop->total_iterations++;
    web_loop->total_time_spent += (end_time - start_time);
    web_loop->last_frame_time = end_time;
    web_loop->frame_count++;

    // Log performance every 5 seconds
    if (web_loop->frame_count % 300 == 0) {  // Assuming 60fps
        gdouble avg_iteration_time = web_loop->total_time_spent / (gdouble)web_loop->total_iterations;
        g_debug("[MainLoop] %d frames, avg iteration: %.2f μs",
                web_loop->frame_count, avg_iteration_time);
    }

    return web_loop->is_running;
}

/**
 * Initialize web-native main loop system
 */
static void g_web_mainloop_init(void) {
    if (web_mainloop_initialized) return;

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->is_chrome_based || caps->chrome_version < 113) {
        g_warning("Web-native main loop requires Chrome/Edge 113+");
        return;
    }

    web_main_loop = g_new0(GWebMainLoop, 1);
    web_main_loop->fps_target = 60.0;
    web_main_loop->is_running = FALSE;

    web_mainloop_initialized = TRUE;
    g_debug("Web-native main loop system initialized");
}

/**
 * Enhanced g_main_loop_run() - uses browser RAF for optimal performance
 */
void g_web_main_loop_run(GMainLoop *loop) {
    g_return_if_fail(loop != NULL);

    if (!web_mainloop_initialized) {
        g_web_mainloop_init();
    }

    if (!web_main_loop) {
        g_error("Failed to initialize web-native main loop");
        return;
    }

    GMainContext *context = g_main_loop_get_context(loop);
    web_main_loop->context = context;
    web_main_loop->is_running = TRUE;

    g_message("Starting web-native main loop with browser RAF integration");

    // Setup browser integration
    setup_request_animation_frame(web_main_loop);
    start_browser_main_loop();

    // For Emscripten, we need to throw an exception to simulate infinite loop
    // This prevents the C++ stack from unwinding while the browser loop runs
    emscripten_set_main_loop_arg(
        (em_arg_callback_func)g_web_main_context_iteration,
        web_main_loop,
        0,      // Use requestAnimationFrame (0 fps)
        TRUE    // Simulate infinite loop
    );
}

/**
 * Enhanced g_main_loop_quit() - stops browser RAF loop
 */
void g_web_main_loop_quit(GMainLoop *loop) {
    g_return_if_fail(loop != NULL);

    if (web_main_loop) {
        web_main_loop->is_running = FALSE;
        stop_browser_main_loop();

        g_message("Web-native main loop stopped after %ld iterations (avg: %.2f μs/iter)",
                  web_main_loop->total_iterations,
                  web_main_loop->total_time_spent / (gdouble)web_main_loop->total_iterations);
    }

    emscripten_cancel_main_loop();
}

/**
 * Check if we're using web-native main loop
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_main_loop_is_web_native(void) {
    return web_mainloop_initialized && web_main_loop != NULL;
}

/**
 * Get main loop performance statistics
 */
EMSCRIPTEN_KEEPALIVE
void g_web_main_loop_get_stats(gint64 *iterations, gint64 *total_time, gdouble *avg_time) {
    if (!web_main_loop) {
        if (iterations) *iterations = 0;
        if (total_time) *total_time = 0;
        if (avg_time) *avg_time = 0.0;
        return;
    }

    if (iterations) *iterations = web_main_loop->total_iterations;
    if (total_time) *total_time = web_main_loop->total_time_spent;
    if (avg_time) {
        *avg_time = web_main_loop->total_iterations > 0 ?
                   web_main_loop->total_time_spent / (gdouble)web_main_loop->total_iterations : 0.0;
    }
}

/**
 * Set target FPS (for future use with variable rate)
 */
EMSCRIPTEN_KEEPALIVE
void g_web_main_loop_set_fps_target(gdouble fps) {
    if (web_main_loop) {
        web_main_loop->fps_target = CLAMP(fps, 1.0, 120.0);
        g_debug("Main loop FPS target set to %.1f", fps);
    }
}

/**
 * Cleanup web main loop resources
 */
void g_web_mainloop_cleanup(void) {
    if (web_main_loop) {
        stop_browser_main_loop();
        g_free(web_main_loop);
        web_main_loop = NULL;
    }
    web_mainloop_initialized = FALSE;
}

/**
 * Override GLib's main loop functions to use web-native implementation
 */
#ifdef GLIB_WASM_BROWSER_MAINLOOP_ENABLED

// Function pointer replacements for web-native behavior
void (*g_main_loop_run_original)(GMainLoop *loop) = NULL;
void (*g_main_loop_quit_original)(GMainLoop *loop) = NULL;

/**
 * Initialize main loop function overrides
 */
void g_web_mainloop_override_functions(void) {
    // In a real implementation, we would use function interception
    // For now, this serves as a placeholder for the override mechanism
    g_message("Main loop function overrides initialized - use g_web_main_loop_*() functions");
}

#endif /* GLIB_WASM_BROWSER_MAINLOOP_ENABLED */