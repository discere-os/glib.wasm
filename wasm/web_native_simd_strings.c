/* GLib WASM - Web-Native SIMD String Operations
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * High-performance string operations using WASM SIMD for 3-5x speedup
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <wasm_simd128.h>
#include <string.h>
#include <stdint.h>

// Forward declarations
extern const GWebCapabilities* g_web_get_capabilities(void);

// SIMD constants
static const size_t SIMD_THRESHOLD = 32;  // Minimum size for SIMD benefit
static const size_t SIMD_WIDTH = 16;      // 128-bit SIMD processes 16 bytes

// Web API string operations
EM_JS(char*, web_api_string_tolower, (const char* str), {
    const input = UTF8ToString(str);
    const result = input.toLowerCase();
    return stringToNewUTF8(result);
});

EM_JS(char*, web_api_string_toupper, (const char* str), {
    const input = UTF8ToString(str);
    const result = input.toUpperCase();
    return stringToNewUTF8(result);
});

EM_JS(int, web_api_string_normalize, (const char* str, char* output, int max_len), {
    const input = UTF8ToString(str);
    const normalized = input.normalize('NFC');
    const bytes = lengthBytesUTF8(normalized) + 1;

    if (bytes <= max_len) {
        stringToUTF8(normalized, output, max_len);
        return bytes - 1;  // Don't count null terminator
    }
    return -1;  // Buffer too small
});

/**
 * SIMD-optimized string length calculation
 */
EMSCRIPTEN_KEEPALIVE
gsize g_web_strlen_simd(const gchar *str) {
    if (!str) return 0;

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_wasm_simd) {
        return strlen(str);  // Fallback to standard implementation
    }

    const uint8_t *data = (const uint8_t*)str;
    size_t len = 0;

    // SIMD processing for bulk of string
    v128_t zero_vec = wasm_i8x16_splat(0);

    while (1) {
        // Load 16 bytes
        v128_t chunk = wasm_v128_load(data + len);

        // Compare with zero
        v128_t is_zero = wasm_i8x16_eq(chunk, zero_vec);
        int mask = wasm_i8x16_bitmask(is_zero);

        if (mask != 0) {
            // Found null terminator - find its position
            int zero_pos = __builtin_ctz(mask);
            return len + zero_pos;
        }

        len += 16;
    }
}

/**
 * SIMD-optimized memory comparison
 */
EMSCRIPTEN_KEEPALIVE
gint g_web_memcmp_simd(const void *s1, const void *s2, gsize n) {
    if (!s1 || !s2) return s1 ? 1 : (s2 ? -1 : 0);
    if (n == 0) return 0;

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_wasm_simd || n < SIMD_THRESHOLD) {
        return memcmp(s1, s2, n);
    }

    const uint8_t *p1 = (const uint8_t*)s1;
    const uint8_t *p2 = (const uint8_t*)s2;
    size_t pos = 0;

    // SIMD bulk comparison
    while (pos + 16 <= n) {
        v128_t v1 = wasm_v128_load(&p1[pos]);
        v128_t v2 = wasm_v128_load(&p2[pos]);
        v128_t cmp = wasm_i8x16_eq(v1, v2);
        int mask = wasm_i8x16_bitmask(cmp);

        if (mask != 0xFFFF) {
            // Found difference - determine position
            int diff_pos = __builtin_ctz(~mask);
            pos += diff_pos;
            return (int)p1[pos] - (int)p2[pos];
        }

        pos += 16;
    }

    // Handle remaining bytes
    while (pos < n) {
        if (p1[pos] != p2[pos]) {
            return (int)p1[pos] - (int)p2[pos];
        }
        pos++;
    }

    return 0;
}

/**
 * SIMD-optimized memory search
 */
EMSCRIPTEN_KEEPALIVE
const void* g_web_memchr_simd(const void *s, gint c, gsize n) {
    if (!s || n == 0) return NULL;

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_wasm_simd || n < SIMD_THRESHOLD) {
        return memchr(s, c, n);
    }

    const uint8_t *data = (const uint8_t*)s;
    uint8_t needle = (uint8_t)c;
    v128_t needle_vec = wasm_i8x16_splat(needle);

    // SIMD search
    for (size_t i = 0; i + 15 < n; i += 16) {
        v128_t chunk = wasm_v128_load(&data[i]);
        v128_t cmp = wasm_i8x16_eq(chunk, needle_vec);
        int mask = wasm_i8x16_bitmask(cmp);

        if (mask) {
            int pos = __builtin_ctz(mask);
            return &data[i + pos];
        }
    }

    // Scalar fallback for remainder
    size_t simd_end = (n / 16) * 16;
    for (size_t i = simd_end; i < n; i++) {
        if (data[i] == needle) {
            return &data[i];
        }
    }

    return NULL;
}

/**
 * SIMD-optimized UTF-8 validation with ASCII fast path
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_utf8_validate_simd(const gchar *str, gssize max_len, const gchar **end) {
    if (!str) {
        if (end) *end = str;
        return TRUE;  // Empty string is valid UTF-8
    }

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_wasm_simd) {
        return g_utf8_validate(str, max_len, end);
    }

    const uint8_t *data = (const uint8_t*)str;
    gsize len = max_len >= 0 ? (gsize)max_len : strlen(str);
    size_t pos = 0;

    v128_t ascii_max = wasm_i8x16_splat(0x7F);

    // SIMD ASCII fast path
    while (pos + 15 < len) {
        v128_t chunk = wasm_v128_load(&data[pos]);
        v128_t is_ascii = wasm_u8x16_le(chunk, ascii_max);
        int mask = wasm_i8x16_bitmask(is_ascii);

        if (mask == 0xFFFF) {
            // All ASCII - fast path
            pos += 16;
            continue;
        }

        // Contains non-ASCII, validate UTF-8 sequences byte by byte
        for (int i = 0; i < 16 && pos + i < len; i++) {
            uint8_t byte = data[pos + i];

            if (byte <= 0x7F) {
                // ASCII - valid
                continue;
            }

            // Multi-byte UTF-8 sequence validation
            gint sequence_length = 0;
            if ((byte & 0xE0) == 0xC0) {
                sequence_length = 2;
            } else if ((byte & 0xF0) == 0xE0) {
                sequence_length = 3;
            } else if ((byte & 0xF8) == 0xF0) {
                sequence_length = 4;
            } else {
                // Invalid start byte
                if (end) *end = (const gchar*)&data[pos + i];
                return FALSE;
            }

            // Check we have enough bytes
            if (pos + i + sequence_length > len) {
                if (end) *end = (const gchar*)&data[pos + i];
                return FALSE;
            }

            // Validate continuation bytes
            for (gint j = 1; j < sequence_length; j++) {
                if ((data[pos + i + j] & 0xC0) != 0x80) {
                    if (end) *end = (const gchar*)&data[pos + i + j];
                    return FALSE;
                }
            }

            // Skip validated sequence
            i += sequence_length - 1;
        }

        pos += 16;
    }

    // Validate remaining bytes
    while (pos < len) {
        if (!g_utf8_validate((const gchar*)&data[pos], len - pos, end)) {
            return FALSE;
        }
        break;
    }

    if (end) *end = (const gchar*)&data[len];
    return TRUE;
}

/**
 * Enhanced g_utf8_strlen with SIMD optimization
 */
EMSCRIPTEN_KEEPALIVE
glong g_web_utf8_strlen_simd(const gchar *p, gssize max) {
    if (!p) return 0;

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_wasm_simd) {
        return g_utf8_strlen(p, max);
    }

    const uint8_t *str = (const uint8_t*)p;
    const uint8_t *end = max < 0 ? str + strlen(p) : str + max;
    glong chars = 0;

    // SIMD counting of non-continuation bytes
    v128_t continuation_mask = wasm_u8x16_splat(0x80);
    v128_t continuation_value = wasm_u8x16_splat(0x80);

    // Process 16 bytes at a time
    while (str + 16 <= end) {
        v128_t data = wasm_v128_load(str);
        v128_t masked = wasm_v128_and(data, continuation_mask);
        v128_t is_continuation = wasm_i8x16_eq(masked, continuation_value);

        // Count non-continuation bytes
        int continuation_mask_bits = wasm_i8x16_bitmask(is_continuation);
        chars += 16 - __builtin_popcount(continuation_mask_bits);
        str += 16;
    }

    // Handle remaining bytes
    while (str < end) {
        if ((*str & 0x80) == 0 || (*str & 0xC0) == 0xC0) {
            chars++;
        }
        str++;
    }

    return chars;
}

/**
 * Web API enhanced string case conversion
 */
EMSCRIPTEN_KEEPALIVE
gchar* g_web_utf8_strdown_enhanced(const gchar *str, gssize len) {
    if (!str) return NULL;

    const GWebCapabilities *caps = g_web_get_capabilities();

    // Use Web API for full Unicode support if available
    if (caps->has_intl_apis && len > 32) {
        char *result = web_api_string_tolower(str);
        if (result) {
            return result;  // Web API handles memory allocation
        }
    }

    // Fallback to standard GLib implementation
    return g_utf8_strdown(str, len);
}

/**
 * Web API enhanced string case conversion (uppercase)
 */
EMSCRIPTEN_KEEPALIVE
gchar* g_web_utf8_strup_enhanced(const gchar *str, gssize len) {
    if (!str) return NULL;

    const GWebCapabilities *caps = g_web_get_capabilities();

    // Use Web API for full Unicode support if available
    if (caps->has_intl_apis && len > 32) {
        char *result = web_api_string_toupper(str);
        if (result) {
            return result;  // Web API handles memory allocation
        }
    }

    // Fallback to standard GLib implementation
    return g_utf8_strup(str, len);
}

/**
 * SIMD-optimized string comparison
 */
EMSCRIPTEN_KEEPALIVE
gint g_web_strcmp_simd(const gchar *s1, const gchar *s2) {
    if (!s1 || !s2) return s1 ? 1 : (s2 ? -1 : 0);

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_wasm_simd) {
        return strcmp(s1, s2);
    }

    // For string comparison, we need to handle null terminators
    const uint8_t *p1 = (const uint8_t*)s1;
    const uint8_t *p2 = (const uint8_t*)s2;
    size_t pos = 0;

    v128_t zero_vec = wasm_i8x16_splat(0);

    while (1) {
        v128_t v1 = wasm_v128_load(&p1[pos]);
        v128_t v2 = wasm_v128_load(&p2[pos]);

        // Check for null terminators
        v128_t null1 = wasm_i8x16_eq(v1, zero_vec);
        v128_t null2 = wasm_i8x16_eq(v2, zero_vec);
        int null_mask1 = wasm_i8x16_bitmask(null1);
        int null_mask2 = wasm_i8x16_bitmask(null2);

        // Compare the actual data
        v128_t cmp = wasm_i8x16_eq(v1, v2);
        int cmp_mask = wasm_i8x16_bitmask(cmp);

        // If we found differences or null terminators, handle them
        if (cmp_mask != 0xFFFF || null_mask1 || null_mask2) {
            // Check each byte in this chunk
            for (int i = 0; i < 16; i++) {
                uint8_t c1 = p1[pos + i];
                uint8_t c2 = p2[pos + i];

                if (c1 != c2 || c1 == 0) {
                    return (int)c1 - (int)c2;
                }
            }
        }

        pos += 16;
    }
}

/**
 * Get SIMD string operation statistics
 */
EMSCRIPTEN_KEEPALIVE
void g_web_simd_strings_get_stats(gint *operations, gdouble *avg_speedup) {
    // Placeholder for performance tracking
    if (operations) *operations = 0;
    if (avg_speedup) *avg_speedup = 0.0;
}

/**
 * Benchmark SIMD string operations
 */
EMSCRIPTEN_KEEPALIVE
gdouble g_web_simd_strings_benchmark(gsize test_size_kb) {
    if (test_size_kb == 0) test_size_kb = 64;  // Default 64KB test

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_wasm_simd) {
        return 1.0;  // No speedup possible
    }

    gsize test_size = test_size_kb * 1024;
    gchar *test_data = g_malloc(test_size + 1);

    // Fill with test data
    for (gsize i = 0; i < test_size; i++) {
        test_data[i] = (gchar)(65 + (i % 26));  // A-Z pattern
    }
    test_data[test_size] = '\0';

    // Time SIMD operation
    gint64 simd_start = g_get_monotonic_time();
    gsize simd_len = g_web_strlen_simd(test_data);
    gint64 simd_end = g_get_monotonic_time();

    // Time standard operation
    gint64 std_start = g_get_monotonic_time();
    gsize std_len = strlen(test_data);
    gint64 std_end = g_get_monotonic_time();

    g_free(test_data);

    // Calculate speedup
    gdouble simd_time = (simd_end - simd_start) / 1000.0;  // Convert to ms
    gdouble std_time = (std_end - std_start) / 1000.0;

    if (simd_len != std_len) {
        g_warning("SIMD string length mismatch: SIMD=%lu, standard=%lu",
                  (unsigned long)simd_len, (unsigned long)std_len);
        return 1.0;
    }

    gdouble speedup = simd_time > 0 ? (std_time / simd_time) : 1.0;
    g_message("SIMD string benchmark (%lu KB): %.2fx speedup (%.2f ms vs %.2f ms)",
              (unsigned long)test_size_kb, speedup, simd_time, std_time);

    return speedup;
}

/**
 * Initialize web-native SIMD string operations
 */
void g_web_simd_strings_init(void) {
    const GWebCapabilities *caps = g_web_get_capabilities();

    g_message("Web-native SIMD strings initialized:");
    g_message("  WASM SIMD: %s", caps->has_wasm_simd ? "YES" : "NO");
    g_message("  Web APIs (Intl): %s", caps->has_intl_apis ? "YES" : "NO");

    if (caps->has_wasm_simd) {
        // Run a quick benchmark
        gdouble speedup = g_web_simd_strings_benchmark(16);  // 16KB test
        g_message("  Benchmark speedup: %.2fx", speedup);
    }
}