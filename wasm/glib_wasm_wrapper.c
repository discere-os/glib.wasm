/* GLib WASM - Main WASM wrapper and exports
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
#include <gstdio.h>
#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GLIB_WASM_WEBGPU_ENABLED
#include "../src/webgpu-integration.h"
#endif

// Forward declarations for browser integration
#ifdef GLIB_WASM_BROWSER_MAINLOOP_ENABLED
extern void g_mainloop_browser_cleanup(void);
#endif

#ifdef GLIB_WASM_THREADING_ENABLED
extern void g_threading_wasm_cleanup(void);
#endif

#ifdef GLIB_WASM_OPFS_ENABLED
extern void g_filesystem_opfs_cleanup(void);
#endif

#ifdef GLIB_WASM_WEBGPU_ENABLED
extern void g_timing_webgpu_cleanup(void);
#endif

// Global state
static gboolean glib_initialized = FALSE;
static GMainLoop *main_loop = NULL;

/**
 * Initialize GLib WASM module
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_init(void) {
    if (glib_initialized) {
        return 1; // Already initialized
    }

    g_message("Initializing GLib WASM v%d.%d.%d",
             GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);

#ifdef GLIB_WASM_WEBGPU_ENABLED
    // Initialize WebGPU integration if available
    if (g_webgpu_initialize(NULL)) {
        g_message("WebGPU integration initialized");
    }
#endif

    glib_initialized = TRUE;
    return 1;
}

/**
 * Get GLib version information
 */
EMSCRIPTEN_KEEPALIVE
const char* glib_wasm_get_version(void) {
    static char version_str[64];
    snprintf(version_str, sizeof(version_str), "%d.%d.%d-wasm",
             GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
    return version_str;
}

/**
 * Get build configuration
 */
EMSCRIPTEN_KEEPALIVE
const char* glib_wasm_get_build_info(void) {
    static char build_info[512];
    char *pos = build_info;
    int remaining = sizeof(build_info);

    pos += snprintf(pos, remaining, "GLib WASM Build Configuration:\n");
    remaining -= (pos - build_info);

#ifdef GLIB_WASM_SIMD_ENABLED
    pos += snprintf(pos, remaining, "- SIMD: enabled\n");
    remaining -= strlen("- SIMD: enabled\n");
#else
    pos += snprintf(pos, remaining, "- SIMD: disabled\n");
    remaining -= strlen("- SIMD: disabled\n");
#endif

#ifdef GLIB_WASM_THREADING_ENABLED
    pos += snprintf(pos, remaining, "- Threading: enabled\n");
    remaining -= strlen("- Threading: enabled\n");
#else
    pos += snprintf(pos, remaining, "- Threading: disabled\n");
    remaining -= strlen("- Threading: disabled\n");
#endif

#ifdef GLIB_WASM_WEBGPU_ENABLED
    pos += snprintf(pos, remaining, "- WebGPU: enabled\n");
    remaining -= strlen("- WebGPU: enabled\n");
#else
    pos += snprintf(pos, remaining, "- WebGPU: disabled\n");
    remaining -= strlen("- WebGPU: disabled\n");
#endif

#ifdef GLIB_WASM_OPFS_ENABLED
    pos += snprintf(pos, remaining, "- OPFS: enabled\n");
    remaining -= strlen("- OPFS: enabled\n");
#else
    pos += snprintf(pos, remaining, "- OPFS: disabled\n");
    remaining -= strlen("- OPFS: disabled\n");
#endif

#ifdef GLIB_WASM_BROWSER_MAINLOOP_ENABLED
    pos += snprintf(pos, remaining, "- Browser Main Loop: enabled\n");
    remaining -= strlen("- Browser Main Loop: enabled\n");
#else
    pos += snprintf(pos, remaining, "- Browser Main Loop: disabled\n");
    remaining -= strlen("- Browser Main Loop: disabled\n");
#endif

#ifdef HAVE_PCRE2
    pos += snprintf(pos, remaining, "- PCRE2: available\n");
    remaining -= strlen("- PCRE2: available\n");
#else
    pos += snprintf(pos, remaining, "- PCRE2: not available\n");
    remaining -= strlen("- PCRE2: not available\n");
#endif

#ifdef HAVE_LIBFFI
    pos += snprintf(pos, remaining, "- libffi: available\n");
    remaining -= strlen("- libffi: available\n");
#else
    pos += snprintf(pos, remaining, "- libffi: not available\n");
    remaining -= strlen("- libffi: not available\n");
#endif

    return build_info;
}

/**
 * Test basic GLib functionality
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_test_basic(void) {
    if (!glib_initialized) {
        glib_wasm_init();
    }

    // Test basic string operations
    gchar *test_str = g_strdup("Hello, GLib WASM!");
    if (!test_str) {
        g_warning("Failed to allocate test string");
        return 0;
    }

    gboolean test_passed = g_str_has_prefix(test_str, "Hello");
    g_free(test_str);

    if (!test_passed) {
        g_warning("String prefix test failed");
        return 0;
    }

    // Test array operations
    GPtrArray *array = g_ptr_array_new();
    g_ptr_array_add(array, g_strdup("test1"));
    g_ptr_array_add(array, g_strdup("test2"));
    g_ptr_array_add(array, g_strdup("test3"));

    if (array->len != 3) {
        g_warning("Array test failed: expected 3 items, got %u", array->len);
        g_ptr_array_free(array, TRUE);
        return 0;
    }

    g_ptr_array_free(array, TRUE);

    // Test hash table operations
    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(hash, g_strdup("key1"), g_strdup("value1"));
    g_hash_table_insert(hash, g_strdup("key2"), g_strdup("value2"));

    const gchar *value = g_hash_table_lookup(hash, "key1");
    if (!value || strcmp(value, "value1") != 0) {
        g_warning("Hash table test failed");
        g_hash_table_destroy(hash);
        return 0;
    }

    g_hash_table_destroy(hash);

    g_message("Basic GLib functionality tests passed");
    return 1;
}

/**
 * Test file operations
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_test_filesystem(void) {
    if (!glib_initialized) {
        glib_wasm_init();
    }

    const gchar *test_filename = "/tmp/glib_test.txt";
    const gchar *test_content = "This is a test file for GLib WASM filesystem operations.";
    gchar *read_content = NULL;
    gsize read_length;
    GError *error = NULL;

    // Write test file
    if (!g_file_set_contents(test_filename, test_content, -1, &error)) {
        g_warning("Failed to write test file: %s", error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return 0;
    }

    // Read test file back
    if (!g_file_get_contents(test_filename, &read_content, &read_length, &error)) {
        g_warning("Failed to read test file: %s", error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return 0;
    }

    // Verify content
    if (strcmp(test_content, read_content) != 0) {
        g_warning("File content mismatch: expected '%s', got '%s'", test_content, read_content);
        g_free(read_content);
        return 0;
    }

    g_free(read_content);

    // Test file existence
    if (!g_file_test(test_filename, G_FILE_TEST_EXISTS)) {
        g_warning("File existence test failed");
        return 0;
    }

    // Clean up
    g_unlink(test_filename);

    g_message("Filesystem tests passed");
    return 1;
}

/**
 * Cleanup and shutdown
 */
EMSCRIPTEN_KEEPALIVE
void glib_wasm_cleanup(void) {
    if (!glib_initialized) {
        return;
    }

    g_message("Cleaning up GLib WASM module");

    if (main_loop) {
        g_main_loop_quit(main_loop);
        g_main_loop_unref(main_loop);
        main_loop = NULL;
    }

#ifdef GLIB_WASM_BROWSER_MAINLOOP_ENABLED
    g_mainloop_browser_cleanup();
#endif

#ifdef GLIB_WASM_THREADING_ENABLED
    g_threading_wasm_cleanup();
#endif

#ifdef GLIB_WASM_OPFS_ENABLED
    g_filesystem_opfs_cleanup();
#endif

#ifdef GLIB_WASM_WEBGPU_ENABLED
    g_timing_webgpu_cleanup();
    g_webgpu_cleanup();
#endif

    glib_initialized = FALSE;
    g_message("GLib WASM cleanup complete");
}

/**
 * Main entry point (not used in SIDE_MODULE builds)
 */
#ifndef GLIB_WASM_SIDE_MODULE
int main(int argc, char *argv[]) {
    // Initialize GLib
    if (!glib_wasm_init()) {
        fprintf(stderr, "Failed to initialize GLib WASM\n");
        return 1;
    }

    // Run basic tests
    printf("%s\n", glib_wasm_get_build_info());

    if (!glib_wasm_test_basic()) {
        fprintf(stderr, "Basic tests failed\n");
        glib_wasm_cleanup();
        return 1;
    }

    if (!glib_wasm_test_filesystem()) {
        fprintf(stderr, "Filesystem tests failed\n");
        glib_wasm_cleanup();
        return 1;
    }

    printf("All tests passed!\n");

    glib_wasm_cleanup();
    return 0;
}
#endif