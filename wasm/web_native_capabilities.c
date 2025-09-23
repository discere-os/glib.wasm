/* GLib WASM - Web-Native Capability Detection
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <stdbool.h>
#include "web_native_capabilities.h"

static GWebCapabilities g_web_caps = {0};
static gboolean g_caps_detected = FALSE;

// JavaScript capability detection functions
EM_JS(int, detect_opfs_support, (), {
    return typeof navigator !== 'undefined' &&
           'storage' in navigator &&
           'getDirectory' in navigator.storage ? 1 : 0;
});

EM_JS(int, detect_shared_array_buffer, (), {
    return typeof SharedArrayBuffer !== 'undefined' ? 1 : 0;
});

EM_JS(int, detect_wasm_simd, (), {
    try {
        // Test for WASM SIMD support by validating a minimal SIMD module
        const wasmSIMDTest = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,  // WASM header
            0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b,        // Type section: () -> v128
            0x03, 0x02, 0x01, 0x00,                            // Function section
            0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x00, 0xfd,   // Code section: i32.const 0, i8x16.splat
            0x0f, 0x0b
        ]);
        return WebAssembly.validate(wasmSIMDTest) ? 1 : 0;
    } catch (e) {
        return 0;
    }
});

EM_JS(int, detect_web_crypto, (), {
    return typeof crypto !== 'undefined' &&
           'subtle' in crypto &&
           typeof crypto.subtle.digest === 'function' ? 1 : 0;
});

EM_JS(int, detect_intl_apis, (), {
    return typeof Intl !== 'undefined' &&
           'DateTimeFormat' in Intl &&
           'supportedValuesOf' in Intl ? 1 : 0;
});

EM_JS(int, detect_fetch_api, (), {
    return typeof fetch !== 'undefined' &&
           typeof Request !== 'undefined' &&
           typeof Response !== 'undefined' ? 1 : 0;
});

EM_JS(int, detect_web_workers, (), {
    return typeof Worker !== 'undefined' ? 1 : 0;
});

EM_JS(int, detect_deno_runtime, (), {
    return typeof Deno !== 'undefined' ? 1 : 0;
});

EM_JS(int, get_chrome_version, (), {
    if (typeof navigator === 'undefined') return 0;

    const ua = navigator.userAgent;
    const chromeMatch = ua.match(/Chrome\\/(\\d+)/);
    const edgeMatch = ua.match(/Edg\\/(\\d+)/);

    if (chromeMatch) return parseInt(chromeMatch[1]);
    if (edgeMatch) return parseInt(edgeMatch[1]);
    return 0;
});

EM_JS(int, is_chrome_based_browser, (), {
    if (typeof navigator === 'undefined') return 0;

    const ua = navigator.userAgent;
    return (ua.includes('Chrome') || ua.includes('Edg')) &&
           !ua.includes('Firefox') && !ua.includes('Safari') ? 1 : 0;
});

/**
 * Detect all web capabilities - called once at startup
 */
static void g_web_detect_all_capabilities(void) {
    if (g_caps_detected) return;

    g_web_caps.has_opfs = detect_opfs_support();
    g_web_caps.has_shared_array_buffer = detect_shared_array_buffer();
    g_web_caps.has_wasm_simd = detect_wasm_simd();
    g_web_caps.has_web_crypto = detect_web_crypto();
    g_web_caps.has_intl_apis = detect_intl_apis();
    g_web_caps.has_fetch_api = detect_fetch_api();
    g_web_caps.has_web_workers = detect_web_workers();
    g_web_caps.is_deno_runtime = detect_deno_runtime();
    g_web_caps.is_chrome_based = is_chrome_based_browser();
    g_web_caps.chrome_version = get_chrome_version();

    g_caps_detected = TRUE;

    g_message("Web capabilities detected:");
    g_message("  OPFS: %s", g_web_caps.has_opfs ? "YES" : "NO");
    g_message("  SharedArrayBuffer: %s", g_web_caps.has_shared_array_buffer ? "YES" : "NO");
    g_message("  WASM SIMD: %s", g_web_caps.has_wasm_simd ? "YES" : "NO");
    g_message("  Web Crypto: %s", g_web_caps.has_web_crypto ? "YES" : "NO");
    g_message("  Intl APIs: %s", g_web_caps.has_intl_apis ? "YES" : "NO");
    g_message("  Fetch API: %s", g_web_caps.has_fetch_api ? "YES" : "NO");
    g_message("  Web Workers: %s", g_web_caps.has_web_workers ? "YES" : "NO");
    g_message("  Deno Runtime: %s", g_web_caps.is_deno_runtime ? "YES" : "NO");
    g_message("  Chrome-based: %s (v%d)", g_web_caps.is_chrome_based ? "YES" : "NO",
              g_web_caps.chrome_version);
}

/**
 * Get web capabilities - public API
 */
EMSCRIPTEN_KEEPALIVE
const GWebCapabilities* g_web_get_capabilities(void) {
    if (!g_caps_detected) {
        g_web_detect_all_capabilities();
    }
    return &g_web_caps;
}

/**
 * Check if we're running in a modern web environment
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_is_modern_browser(void) {
    const GWebCapabilities* caps = g_web_get_capabilities();

    // Require Chrome 113+ with essential web APIs
    return caps->is_chrome_based &&
           caps->chrome_version >= 113 &&
           caps->has_wasm_simd &&
           caps->has_fetch_api &&
           caps->has_web_crypto;
}

/**
 * Check if OPFS is available for persistent storage
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_has_persistent_storage(void) {
    const GWebCapabilities* caps = g_web_get_capabilities();
    return caps->has_opfs;
}

/**
 * Check if we can use SIMD optimizations
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_has_simd_support(void) {
    const GWebCapabilities* caps = g_web_get_capabilities();
    return caps->has_wasm_simd;
}

/**
 * Check if we can use web workers for threading
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_has_threading_support(void) {
    const GWebCapabilities* caps = g_web_get_capabilities();
    return caps->has_web_workers && caps->has_shared_array_buffer;
}

/**
 * Check if Web Crypto API is available
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_has_crypto_support(void) {
    const GWebCapabilities* caps = g_web_get_capabilities();
    return caps->has_web_crypto;
}

/**
 * Initialize web capabilities on module startup
 */
void g_web_capabilities_init(void) {
    g_web_detect_all_capabilities();

    // Warn if running in unsuitable environment
    if (!g_web_is_modern_browser()) {
        g_warning("GLib.wasm requires Chrome/Edge 113+ with WebGPU support");
        g_warning("Current browser: %s (version %d)",
                  g_web_caps.is_chrome_based ? "Chrome-based" : "Other",
                  g_web_caps.chrome_version);
    }
}