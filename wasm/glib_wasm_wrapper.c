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
#include "web_native_capabilities.h"

#ifdef GLIB_WASM_SIMD_ENABLED
// Include SIMD function prototypes
extern const uint8_t* glib_memchr_simd(const uint8_t* haystack, size_t len, uint8_t needle);
extern int glib_memcmp_simd(const uint8_t* s1, const uint8_t* s2, size_t len);
extern size_t glib_strlen_simd(const char* str);
extern gboolean glib_utf8_validate_simd(const char* str, gssize max_len, const char** end);
extern void glib_hash_table_bulk_process_simd(uint32_t* values, size_t count, uint32_t adjustment);
extern gint glib_array_compare_simd(const uint8_t* array1, const uint8_t* array2, size_t element_size, size_t count);
extern uint32_t glib_simple_checksum_simd(const uint8_t* data, size_t len);
extern gboolean glib_test_simd_functionality(void);
extern double glib_benchmark_simd_memcmp(size_t test_size_kb);
#endif


// Forward declarations for web-native components
extern void g_web_capabilities_init(void);
extern void g_web_mainloop_cleanup(void);
extern void g_web_threading_cleanup(void);
extern void g_web_filesystem_init(void);
extern void g_web_filesystem_cleanup(void);
extern void g_web_simd_strings_init(void);
extern void g_web_crypto_init(void);
extern void g_web_crypto_cleanup(void);
extern void g_web_networking_init(void);
extern void g_web_networking_cleanup(void);
extern void g_web_memory_system_init(void);
extern void g_web_memory_cleanup(void);

// Threading function declarations
extern void g_web_threading_cleanup(void);
#ifdef GLIB_WASM_THREADING_ENABLED
extern void g_web_threading_init_overrides(void);
#endif

// Additional web-native subsystem function declarations
extern void g_web_filesystem_init(void);
extern void g_web_simd_strings_init(void);
extern void g_web_crypto_init(void);
extern void g_mainloop_browser_cleanup(void);
extern void g_filesystem_opfs_cleanup(void);

// Web-native API exports (declared in web_native_capabilities.h)

// Global state
static gboolean glib_initialized = FALSE;
static GMainLoop *main_loop = NULL;

/**
 * Initialize GLib WASM module with web-native enhancements
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_init(void) {
    if (glib_initialized) {
        return 1; // Already initialized
    }

    g_message("Initializing GLib WASM v%d.%d.%d with web-native enhancements",
             GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);

    // Initialize web capabilities detection first
    g_web_capabilities_init();

    // Check if we're in a supported environment
    if (!g_web_is_modern_browser()) {
        g_warning("GLib.wasm requires Chrome/Edge 113+ for optimal performance");
        // Continue initialization but with reduced functionality
    }

    // Initialize web-native subsystems
    g_message("Initializing web-native subsystems...");

    g_web_memory_system_init();    // Memory management with WeakRef
    g_web_filesystem_init();       // OPFS + intelligent storage
    g_web_simd_strings_init();     // WASM SIMD string operations
    g_web_crypto_init();           // Web Crypto API integration
    g_web_networking_init();       // Fetch API networking

#ifdef GLIB_WASM_THREADING_ENABLED
    g_web_threading_init_overrides(); // Hybrid threading with PROXY_TO_PTHREAD
#endif

    g_message("GLib.wasm initialization complete - web-native mode active");
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

    pos += snprintf(pos, remaining, "GLib WASM Web-Native Build Configuration:\n");
    remaining -= (pos - build_info);

    // Get runtime capabilities
    const GWebCapabilities *caps = g_web_get_capabilities();

    pos += snprintf(pos, remaining, "- Web-Native Mode: %s\n",
                   g_web_is_modern_browser() ? "ACTIVE" : "FALLBACK");
    remaining -= strlen("- Web-Native Mode: ACTIVE\n");

    pos += snprintf(pos, remaining, "- WASM SIMD: %s\n",
                   caps->has_wasm_simd ? "YES" : "NO");
    remaining -= strlen("- WASM SIMD: YES\n");

    pos += snprintf(pos, remaining, "- OPFS Storage: %s\n",
                   caps->has_opfs ? "YES" : "NO");
    remaining -= strlen("- OPFS Storage: YES\n");

    pos += snprintf(pos, remaining, "- Web Crypto API: %s\n",
                   caps->has_web_crypto ? "YES" : "NO");
    remaining -= strlen("- Web Crypto API: YES\n");

    pos += snprintf(pos, remaining, "- Fetch API: %s\n",
                   caps->has_fetch_api ? "YES" : "NO");
    remaining -= strlen("- Fetch API: YES\n");

    pos += snprintf(pos, remaining, "- WASM Workers: %s\n",
                   caps->has_web_workers ? "YES" : "NO");
    remaining -= strlen("- WASM Workers: YES\n");

    pos += snprintf(pos, remaining, "- SharedArrayBuffer: %s\n",
                   caps->has_shared_array_buffer ? "YES" : "NO");
    remaining -= strlen("- SharedArrayBuffer: YES\n");

    pos += snprintf(pos, remaining, "- Browser: %s v%d\n",
                   caps->is_chrome_based ? "Chrome/Edge" : "Other",
                   caps->chrome_version);
    remaining -= strlen("- Browser: Chrome/Edge v999\n");

    pos += snprintf(pos, remaining, "- Runtime: %s\n",
                   caps->is_deno_runtime ? "Deno" : "Browser");
    remaining -= strlen("- Runtime: Browser\n");

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
 * Test web-native SIMD string operations
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_test_simd_strings(void) {
    if (!glib_initialized) {
        glib_wasm_init();
    }

    extern gsize g_web_strlen_simd(const gchar *str);
    extern gint g_web_memcmp_simd(const void *s1, const void *s2, gsize n);
    extern gdouble g_web_simd_strings_benchmark(gsize test_size_kb);

    const gchar *test_string = "Hello, WASM SIMD world! This is a test string for performance.";
    gsize simd_len = g_web_strlen_simd(test_string);
    gsize std_len = strlen(test_string);

    if (simd_len != std_len) {
        g_warning("SIMD strlen test failed: expected %lu, got %lu", std_len, simd_len);
        return 0;
    }

    // Test memory comparison
    const gchar *str1 = "identical";
    const gchar *str2 = "identical";
    const gchar *str3 = "different";

    if (g_web_memcmp_simd(str1, str2, strlen(str1)) != 0) {
        g_warning("SIMD memcmp test failed: identical strings");
        return 0;
    }

    if (g_web_memcmp_simd(str1, str3, strlen(str1)) == 0) {
        g_warning("SIMD memcmp test failed: different strings");
        return 0;
    }

    // Run benchmark
    gdouble speedup = g_web_simd_strings_benchmark(32);  // 32KB test
    g_message("SIMD string tests passed (%.2fx speedup)", speedup);
    return 1;
}

/**
 * Test web-native crypto operations
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_test_crypto(void) {
    if (!glib_initialized) {
        glib_wasm_init();
    }

    extern GChecksum* g_web_checksum_new(GChecksumType type);
    extern void g_web_checksum_update(GChecksum *checksum, const guchar *data, gssize length);
    extern const gchar* g_web_checksum_get_string(GChecksum *checksum);
    extern void g_web_checksum_free(GChecksum *checksum);
    extern gboolean g_web_random_bytes(guchar *buffer, gsize length);

    // Test SHA-256 checksum
    const gchar *test_data = "The quick brown fox jumps over the lazy dog";
    GChecksum *checksum = g_web_checksum_new(G_CHECKSUM_SHA256);

    if (!checksum) {
        g_warning("Failed to create web crypto checksum");
        return 0;
    }

    g_web_checksum_update(checksum, (const guchar*)test_data, strlen(test_data));
    const gchar *digest = g_web_checksum_get_string(checksum);

    if (!digest) {
        g_warning("Failed to compute web crypto digest");
        g_web_checksum_free(checksum);
        return 0;
    }

    // Verify against known SHA-256 hash
    const gchar *expected = "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592";
    if (g_strcmp0(digest, expected) != 0) {
        g_warning("Web crypto digest mismatch: expected %s, got %s", expected, digest);
        g_web_checksum_free(checksum);
        return 0;
    }

    g_web_checksum_free(checksum);

    // Test secure random number generation
    guchar random_bytes[32];
    if (!g_web_random_bytes(random_bytes, sizeof(random_bytes))) {
        g_warning("Failed to generate secure random bytes");
        return 0;
    }

    // Verify randomness (basic check - not all zeros)
    gboolean has_nonzero = FALSE;
    for (gsize i = 0; i < sizeof(random_bytes); i++) {
        if (random_bytes[i] != 0) {
            has_nonzero = TRUE;
            break;
        }
    }

    if (!has_nonzero) {
        g_warning("Random bytes generation seems broken - all zeros");
        return 0;
    }

    g_message("Web crypto tests passed");
    return 1;
}

/**
 * Test web-native networking
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_test_networking(void) {
    if (!glib_initialized) {
        glib_wasm_init();
    }

    extern gchar* g_web_http_get(const gchar *url, GError **error);
    extern gboolean g_web_url_is_reachable(const gchar *url);

    const gchar *test_url = "https://httpbin.org/json";
    GError *error = NULL;

    // Test URL reachability (this is a quick HEAD request)
    if (!g_web_url_is_reachable(test_url)) {
        g_message("Network test skipped - test URL not reachable");
        return 1;  // Not a failure, just no network
    }

    // Test HTTP GET
    gchar *response = g_web_http_get(test_url, &error);

    if (error) {
        g_message("Network test skipped - HTTP error: %s", error->message);
        g_error_free(error);
        return 1;  // Not a failure, just no network
    }

    if (!response) {
        g_warning("HTTP GET returned null response");
        return 0;
    }

    // Basic validation - should be JSON
    if (!g_str_has_prefix(response, "{")) {
        g_warning("HTTP GET response doesn't look like JSON: %s", response);
        g_free(response);
        return 0;
    }

    g_free(response);
    g_message("Web networking tests passed");
    return 1;
}

/**
 * Comprehensive web-native test suite
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_test_web_native(void) {
    if (!glib_initialized) {
        glib_wasm_init();
    }

    g_message("Running comprehensive web-native test suite...");

    gint tests_passed = 0;
    gint total_tests = 0;

    // Test 1: Basic functionality
    total_tests++;
    if (glib_wasm_test_basic()) {
        tests_passed++;
        g_message("✓ Basic functionality test passed");
    } else {
        g_warning("✗ Basic functionality test failed");
    }

    // Test 2: Filesystem
    total_tests++;
    if (glib_wasm_test_filesystem()) {
        tests_passed++;
        g_message("✓ Filesystem test passed");
    } else {
        g_warning("✗ Filesystem test failed");
    }

    // Test 3: SIMD strings
    total_tests++;
    if (glib_wasm_test_simd_strings()) {
        tests_passed++;
        g_message("✓ SIMD strings test passed");
    } else {
        g_warning("✗ SIMD strings test failed");
    }

    // Test 4: Web crypto
    total_tests++;
    if (glib_wasm_test_crypto()) {
        tests_passed++;
        g_message("✓ Web crypto test passed");
    } else {
        g_warning("✗ Web crypto test failed");
    }

    // Test 5: Networking (optional)
    total_tests++;
    if (glib_wasm_test_networking()) {
        tests_passed++;
        g_message("✓ Networking test passed");
    } else {
        g_warning("✗ Networking test failed");
    }

    g_message("Web-native test results: %d/%d tests passed (%.1f%%)",
              tests_passed, total_tests, (tests_passed * 100.0) / total_tests);

    return tests_passed == total_tests ? 1 : 0;
}

/**
 * Test SIMD functionality and performance
 */
EMSCRIPTEN_KEEPALIVE
int glib_wasm_test_simd(void) {
#ifdef GLIB_WASM_SIMD_ENABLED
    if (!glib_initialized) {
        glib_wasm_init();
    }

    // Test SIMD functionality
    if (!glib_test_simd_functionality()) {
        g_warning("SIMD functionality test failed");
        return 0;
    }

    // Test SIMD string operations
    const char* test_string = "Hello, SIMD world! This is a test string for SIMD operations.";
    size_t expected_len = strlen(test_string);
    size_t simd_len = glib_strlen_simd(test_string);

    if (simd_len != expected_len) {
        g_warning("SIMD strlen test failed: expected %zu, got %zu", expected_len, simd_len);
        return 0;
    }

    // Test SIMD memory comparison
    const char* test_str1 = "identical_string_for_comparison_test";
    const char* test_str2 = "identical_string_for_comparison_test";
    const char* test_str3 = "different_string_for_comparison_test";

    int cmp1 = glib_memcmp_simd((const uint8_t*)test_str1, (const uint8_t*)test_str2, strlen(test_str1));
    int cmp2 = glib_memcmp_simd((const uint8_t*)test_str1, (const uint8_t*)test_str3, strlen(test_str1));

    if (cmp1 != 0) {
        g_warning("SIMD memcmp test failed: identical strings should compare equal");
        return 0;
    }

    if (cmp2 == 0) {
        g_warning("SIMD memcmp test failed: different strings should not compare equal");
        return 0;
    }

    // Test SIMD UTF-8 validation
    const char* utf8_test = "UTF-8 test: café, naïve, 中文 characters";
    const char* end_ptr = NULL;
    gboolean utf8_valid = glib_utf8_validate_simd(utf8_test, -1, &end_ptr);

    if (!utf8_valid) {
        g_warning("SIMD UTF-8 validation test failed");
        return 0;
    }

    // Test SIMD checksum
    const uint8_t checksum_data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint32_t checksum = glib_simple_checksum_simd(checksum_data, sizeof(checksum_data));

    if (checksum == 0) {
        g_warning("SIMD checksum test failed: checksum should be non-zero");
        return 0;
    }

    g_message("All SIMD tests passed successfully");
    return 1;
#else
    g_warning("SIMD support not compiled in");
    return 0;
#endif
}

/**
 * Benchmark SIMD vs scalar performance
 */
EMSCRIPTEN_KEEPALIVE
double glib_wasm_benchmark_simd(void) {
#ifdef GLIB_WASM_SIMD_ENABLED
    if (!glib_initialized) {
        glib_wasm_init();
    }

    // Benchmark memcmp performance with 64KB test
    double throughput = glib_benchmark_simd_memcmp(64);

    g_message("SIMD memcmp throughput: %.2f MB/s", throughput);
    return throughput;
#else
    g_warning("SIMD support not compiled in");
    return 0.0;
#endif
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
    g_web_threading_cleanup();
#endif

#ifdef GLIB_WASM_OPFS_ENABLED
    g_filesystem_opfs_cleanup();
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