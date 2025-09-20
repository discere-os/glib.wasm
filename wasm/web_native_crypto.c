/* GLib WASM - Web-Native Cryptography Implementation
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This implements GChecksum and crypto functions using Web Crypto API
 * for hardware acceleration and timing-attack resistance.
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <string.h>

// Forward declarations
extern const GWebCapabilities* g_web_get_capabilities(void);

// Web Crypto API integration
EM_JS(int, crypto_digest_sha256, (const char* data, int length, char* output), {
    const inputData = new Uint8Array(HEAPU8.subarray(data, data + length));

    return Asyncify.handleAsync(async () => {
        try {
            const hashBuffer = await crypto.subtle.digest('SHA-256', inputData);
            const hashArray = new Uint8Array(hashBuffer);

            // Copy to output buffer (32 bytes for SHA-256)
            for (let i = 0; i < 32; i++) {
                HEAPU8[output + i] = hashArray[i];
            }

            return 1; // Success
        } catch (error) {
            console.error('Web Crypto SHA-256 error:', error);
            return 0; // Failure
        }
    });
});

EM_JS(int, crypto_digest_sha1, (const char* data, int length, char* output), {
    const inputData = new Uint8Array(HEAPU8.subarray(data, data + length));

    return Asyncify.handleAsync(async () => {
        try {
            const hashBuffer = await crypto.subtle.digest('SHA-1', inputData);
            const hashArray = new Uint8Array(hashBuffer);

            // Copy to output buffer (20 bytes for SHA-1)
            for (let i = 0; i < 20; i++) {
                HEAPU8[output + i] = hashArray[i];
            }

            return 1; // Success
        } catch (error) {
            console.error('Web Crypto SHA-1 error:', error);
            return 0; // Failure
        }
    });
});

EM_JS(int, crypto_digest_sha384, (const char* data, int length, char* output), {
    const inputData = new Uint8Array(HEAPU8.subarray(data, data + length));

    return Asyncify.handleAsync(async () => {
        try {
            const hashBuffer = await crypto.subtle.digest('SHA-384', inputData);
            const hashArray = new Uint8Array(hashBuffer);

            // Copy to output buffer (48 bytes for SHA-384)
            for (let i = 0; i < 48; i++) {
                HEAPU8[output + i] = hashArray[i];
            }

            return 1; // Success
        } catch (error) {
            console.error('Web Crypto SHA-384 error:', error);
            return 0; // Failure
        }
    });
});

EM_JS(int, crypto_digest_sha512, (const char* data, int length, char* output), {
    const inputData = new Uint8Array(HEAPU8.subarray(data, data + length));

    return Asyncify.handleAsync(async () => {
        try {
            const hashBuffer = await crypto.subtle.digest('SHA-512', inputData);
            const hashArray = new Uint8Array(hashBuffer);

            // Copy to output buffer (64 bytes for SHA-512)
            for (let i = 0; i < 64; i++) {
                HEAPU8[output + i] = hashArray[i];
            }

            return 1; // Success
        } catch (error) {
            console.error('Web Crypto SHA-512 error:', error);
            return 0; // Failure
        }
    });
});

EM_JS(int, crypto_random_bytes, (char* output, int length), {
    try {
        const randomArray = new Uint8Array(length);
        crypto.getRandomValues(randomArray);

        // Copy to output buffer
        for (let i = 0; i < length; i++) {
            HEAPU8[output + i] = randomArray[i];
        }

        return 1; // Success
    } catch (error) {
        console.error('Web Crypto random error:', error);
        return 0; // Failure
    }
});

// Enhanced GChecksum structure for web-native crypto
typedef struct {
    GChecksumType checksum_type;
    gboolean closed;

    // Web Crypto specific fields
    guint8 *buffer;
    gsize buffer_size;
    gsize buffer_pos;

    // Cached digest result
    guint8 *digest;
    gchar *digest_string;
    gboolean digest_computed;
} GWebChecksum;

/**
 * Get Web Crypto algorithm name for GChecksum type
 */
static const gchar* g_web_checksum_type_to_algorithm(GChecksumType type) {
    switch (type) {
        case G_CHECKSUM_SHA1:   return "SHA-1";
        case G_CHECKSUM_SHA256: return "SHA-256";
        case G_CHECKSUM_SHA384: return "SHA-384";
        case G_CHECKSUM_SHA512: return "SHA-512";
        default: return NULL;
    }
}

/**
 * Get digest size for checksum type
 */
static gsize g_web_checksum_type_get_length(GChecksumType type) {
    switch (type) {
        case G_CHECKSUM_SHA1:   return 20;
        case G_CHECKSUM_SHA256: return 32;
        case G_CHECKSUM_SHA384: return 48;
        case G_CHECKSUM_SHA512: return 64;
        default: return 0;
    }
}

/**
 * Enhanced g_checksum_new() with Web Crypto support
 */
EMSCRIPTEN_KEEPALIVE
GChecksum* g_web_checksum_new(GChecksumType checksum_type) {
    const GWebCapabilities *caps = g_web_get_capabilities();

    // Fall back to standard GChecksum if Web Crypto not available
    if (!caps->has_web_crypto || !g_web_checksum_type_to_algorithm(checksum_type)) {
        return g_checksum_new(checksum_type);
    }

    GWebChecksum *checksum = g_new0(GWebChecksum, 1);
    checksum->checksum_type = checksum_type;
    checksum->closed = FALSE;
    checksum->buffer_size = 1024;  // Initial buffer size
    checksum->buffer = g_malloc(checksum->buffer_size);
    checksum->buffer_pos = 0;
    checksum->digest_computed = FALSE;

    g_debug("Created web-native checksum (%s)", g_web_checksum_type_to_algorithm(checksum_type));
    return (GChecksum*)checksum;
}

/**
 * Enhanced g_checksum_update() with buffering
 */
EMSCRIPTEN_KEEPALIVE
void g_web_checksum_update(GChecksum *checksum, const guchar *data, gssize length) {
    g_return_if_fail(checksum != NULL);
    g_return_if_fail(length == 0 || data != NULL);

    // Check if this is a web-native checksum
    GWebChecksum *web_checksum = (GWebChecksum*)checksum;
    if (!web_checksum->buffer) {
        // Fall back to standard GChecksum
        g_checksum_update(checksum, data, length);
        return;
    }

    g_return_if_fail(!web_checksum->closed);

    if (length < 0) {
        length = strlen((const gchar*)data);
    }

    if (length == 0) return;

    // Expand buffer if needed
    if (web_checksum->buffer_pos + length > web_checksum->buffer_size) {
        gsize new_size = MAX(web_checksum->buffer_size * 2, web_checksum->buffer_pos + length);
        web_checksum->buffer = g_realloc(web_checksum->buffer, new_size);
        web_checksum->buffer_size = new_size;
    }

    // Append data to buffer
    memcpy(web_checksum->buffer + web_checksum->buffer_pos, data, length);
    web_checksum->buffer_pos += length;

    // Reset digest cache
    if (web_checksum->digest_computed) {
        g_free(web_checksum->digest);
        g_free(web_checksum->digest_string);
        web_checksum->digest = NULL;
        web_checksum->digest_string = NULL;
        web_checksum->digest_computed = FALSE;
    }
}

/**
 * Enhanced g_checksum_get_string() with Web Crypto
 */
EMSCRIPTEN_KEEPALIVE
const gchar* g_web_checksum_get_string(GChecksum *checksum) {
    g_return_val_if_fail(checksum != NULL, NULL);

    GWebChecksum *web_checksum = (GWebChecksum*)checksum;
    if (!web_checksum->buffer) {
        // Fall back to standard GChecksum
        return g_checksum_get_string(checksum);
    }

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_web_crypto) {
        // Shouldn't happen, but safety fallback
        return g_checksum_get_string(checksum);
    }

    // Return cached result if available
    if (web_checksum->digest_computed) {
        return web_checksum->digest_string;
    }

    // Compute digest using Web Crypto API
    gsize digest_length = g_web_checksum_type_get_length(web_checksum->checksum_type);
    web_checksum->digest = g_malloc(digest_length);

    gint result = 0;
    switch (web_checksum->checksum_type) {
        case G_CHECKSUM_SHA1:
            result = crypto_digest_sha1((const char*)web_checksum->buffer,
                                       web_checksum->buffer_pos,
                                       (char*)web_checksum->digest);
            break;
        case G_CHECKSUM_SHA256:
            result = crypto_digest_sha256((const char*)web_checksum->buffer,
                                         web_checksum->buffer_pos,
                                         (char*)web_checksum->digest);
            break;
        case G_CHECKSUM_SHA384:
            result = crypto_digest_sha384((const char*)web_checksum->buffer,
                                         web_checksum->buffer_pos,
                                         (char*)web_checksum->digest);
            break;
        case G_CHECKSUM_SHA512:
            result = crypto_digest_sha512((const char*)web_checksum->buffer,
                                         web_checksum->buffer_pos,
                                         (char*)web_checksum->digest);
            break;
        default:
            g_error("Unsupported checksum type for Web Crypto API");
            return NULL;
    }

    if (result != 1) {
        g_warning("Web Crypto digest computation failed");
        g_free(web_checksum->digest);
        web_checksum->digest = NULL;
        return NULL;
    }

    // Convert binary digest to hex string
    web_checksum->digest_string = g_malloc(digest_length * 2 + 1);
    for (gsize i = 0; i < digest_length; i++) {
        g_snprintf(web_checksum->digest_string + i * 2, 3, "%02x", web_checksum->digest[i]);
    }

    web_checksum->digest_computed = TRUE;
    web_checksum->closed = TRUE;

    g_debug("Computed %s digest: %zu bytes → %s",
            g_web_checksum_type_to_algorithm(web_checksum->checksum_type),
            web_checksum->buffer_pos, web_checksum->digest_string);

    return web_checksum->digest_string;
}

/**
 * Enhanced g_checksum_get_digest() with Web Crypto
 */
EMSCRIPTEN_KEEPALIVE
void g_web_checksum_get_digest(GChecksum *checksum, guint8 *buffer, gsize *digest_len) {
    g_return_if_fail(checksum != NULL);
    g_return_if_fail(digest_len != NULL);

    GWebChecksum *web_checksum = (GWebChecksum*)checksum;
    if (!web_checksum->buffer) {
        // Fall back to standard GChecksum
        g_checksum_get_digest(checksum, buffer, digest_len);
        return;
    }

    gsize expected_len = g_web_checksum_type_get_length(web_checksum->checksum_type);
    if (*digest_len < expected_len) {
        *digest_len = expected_len;
        return;
    }

    // Ensure digest is computed
    if (!web_checksum->digest_computed) {
        g_web_checksum_get_string(checksum);  // This computes the digest
    }

    if (web_checksum->digest && buffer) {
        memcpy(buffer, web_checksum->digest, expected_len);
    }

    *digest_len = expected_len;
}

/**
 * Enhanced g_checksum_free() for web-native checksums
 */
EMSCRIPTEN_KEEPALIVE
void g_web_checksum_free(GChecksum *checksum) {
    if (!checksum) return;

    GWebChecksum *web_checksum = (GWebChecksum*)checksum;
    if (!web_checksum->buffer) {
        // Fall back to standard GChecksum
        g_checksum_free(checksum);
        return;
    }

    g_free(web_checksum->buffer);
    g_free(web_checksum->digest);
    g_free(web_checksum->digest_string);
    g_free(web_checksum);
}

/**
 * Secure random number generation using Web Crypto
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_random_bytes(guchar *buffer, gsize length) {
    g_return_val_if_fail(buffer != NULL, FALSE);
    g_return_val_if_fail(length > 0, FALSE);

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_web_crypto) {
        // Fall back to GRand (less secure)
        GRand *rand = g_rand_new();
        for (gsize i = 0; i < length; i++) {
            buffer[i] = (guchar)g_rand_int_range(rand, 0, 256);
        }
        g_rand_free(rand);
        return TRUE;
    }

    gint result = crypto_random_bytes((char*)buffer, (int)length);
    if (result == 1) {
        g_debug("Generated %zu secure random bytes using Web Crypto", length);
        return TRUE;
    } else {
        g_warning("Web Crypto random generation failed");
        return FALSE;
    }
}

/**
 * Convenience function for computing file checksums
 */
EMSCRIPTEN_KEEPALIVE
gchar* g_web_compute_checksum_for_data(GChecksumType checksum_type,
                                       const guchar *data,
                                       gsize length) {
    GChecksum *checksum = g_web_checksum_new(checksum_type);
    if (!checksum) return NULL;

    g_web_checksum_update(checksum, data, length);
    const gchar *digest_string = g_web_checksum_get_string(checksum);
    gchar *result = g_strdup(digest_string);

    g_web_checksum_free(checksum);
    return result;
}

/**
 * Benchmark Web Crypto vs software crypto
 */
EMSCRIPTEN_KEEPALIVE
gdouble g_web_crypto_benchmark(gsize test_size_kb) {
    if (test_size_kb == 0) test_size_kb = 1024;  // Default 1MB test

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_web_crypto) {
        return 1.0;  // No speedup possible
    }

    gsize test_size = test_size_kb * 1024;
    guchar *test_data = g_malloc(test_size);

    // Fill with test data
    for (gsize i = 0; i < test_size; i++) {
        test_data[i] = (guchar)(i % 256);
    }

    // Time Web Crypto operation
    gint64 web_start = g_get_monotonic_time();
    gchar *web_digest = g_web_compute_checksum_for_data(G_CHECKSUM_SHA256, test_data, test_size);
    gint64 web_end = g_get_monotonic_time();

    // Time standard operation
    gint64 std_start = g_get_monotonic_time();
    gchar *std_digest = g_compute_checksum_for_data(G_CHECKSUM_SHA256, test_data, test_size);
    gint64 std_end = g_get_monotonic_time();

    // Calculate speedup
    gdouble web_time = (web_end - web_start) / 1000.0;  // Convert to ms
    gdouble std_time = (std_end - std_start) / 1000.0;
    gdouble speedup = std_time > 0 ? (std_time / web_time) : 1.0;

    // Verify results match
    gboolean results_match = g_strcmp0(web_digest, std_digest) == 0;

    g_message("Web Crypto benchmark (%lu KB): %.2fx speedup (%.2f ms vs %.2f ms) - %s",
              (unsigned long)test_size_kb, speedup, web_time, std_time,
              results_match ? "PASS" : "FAIL");

    g_free(test_data);
    g_free(web_digest);
    g_free(std_digest);

    return results_match ? speedup : 0.0;
}

/**
 * Initialize web-native crypto system
 */
void g_web_crypto_init(void) {
    const GWebCapabilities *caps = g_web_get_capabilities();

    g_message("Web-native crypto initialized:");
    g_message("  Web Crypto API: %s", caps->has_web_crypto ? "YES" : "NO");

    if (caps->has_web_crypto) {
        g_message("  Algorithms: SHA-1, SHA-256, SHA-384, SHA-512");
        g_message("  Features: Hardware acceleration, timing-attack resistance");

        // Run a quick benchmark
        gdouble speedup = g_web_crypto_benchmark(64);  // 64KB test
        g_message("  Benchmark speedup: %.2fx", speedup);
    }
}

/**
 * Cleanup web crypto resources
 */
void g_web_crypto_cleanup(void) {
    // No specific cleanup needed for Web Crypto API
    g_debug("Web-native crypto cleanup completed");
}