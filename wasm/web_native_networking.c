/* GLib WASM - Web-Native Networking Implementation
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This implements networking functions using Fetch API
 * for HTTP/2 multiplexing and modern web capabilities.
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <string.h>

// Forward declarations
extern const GWebCapabilities* g_web_get_capabilities(void);

// Web-native networking context
typedef struct {
    gchar *url;
    gchar *method;
    GHashTable *headers;
    guchar *request_body;
    gsize request_body_length;

    // Response data
    guchar *response_data;
    gsize response_length;
    gint status_code;
    GHashTable *response_headers;

    // State
    gboolean completed;
    GError *error;
} GWebNetworkRequest;

// Fetch API integration
EM_JS(int, fetch_http_request, (const char* url, const char* method,
                               const char* headers_json, const char* body, int body_len,
                               int* status_code, char** response_data, int* response_len), {
    const urlStr = UTF8ToString(url);
    const methodStr = UTF8ToString(method);
    const headersJson = UTF8ToString(headers_json);

    let requestBody = null;
    if (body && body_len > 0) {
        requestBody = new Uint8Array(HEAPU8.subarray(body, body + body_len));
    }

    return Asyncify.handleAsync(async () => {
        try {
            const headers = JSON.parse(headersJson);

            const fetchOptions = {
                method: methodStr,
                headers: headers
            };

            if (requestBody) {
                fetchOptions.body = requestBody;
            }

            const response = await fetch(urlStr, fetchOptions);

            // Set status code
            HEAP32[status_code >> 2] = response.status;

            // Get response data
            const responseBuffer = await response.arrayBuffer();
            const responseArray = new Uint8Array(responseBuffer);

            // Allocate memory for response
            const ptr = Module._malloc(responseArray.length);
            HEAPU8.set(responseArray, ptr);

            // Set response data and length
            HEAP32[response_data >> 2] = ptr;
            HEAP32[response_len >> 2] = responseArray.length;

            return 1; // Success
        } catch (error) {
            console.error('Fetch error:', error);
            HEAP32[status_code >> 2] = 0;
            HEAP32[response_data >> 2] = 0;
            HEAP32[response_len >> 2] = 0;
            return 0; // Failure
        }
    });
});

EM_JS(char*, fetch_get_response_headers, (const char* url, const char* method), {
    const urlStr = UTF8ToString(url);
    const methodStr = UTF8ToString(method);

    return Asyncify.handleAsync(async () => {
        try {
            const response = await fetch(urlStr, { method: methodStr });

            const headersObj = {};
            for (const [key, value] of response.headers.entries()) {
                headersObj[key] = value;
            }

            const headersJson = JSON.stringify(headersObj);
            return stringToNewUTF8(headersJson);
        } catch (error) {
            console.error('Headers fetch error:', error);
            return stringToNewUTF8('{}');
        }
    });
});

EM_JS(int, fetch_download_progress, (const char* url, int callback_ptr), {
    const urlStr = UTF8ToString(url);

    return Asyncify.handleAsync(async () => {
        try {
            const response = await fetch(urlStr);

            if (!response.ok) {
                return 0;
            }

            const contentLength = response.headers.get('content-length');
            const total = contentLength ? parseInt(contentLength) : 0;

            const reader = response.body.getReader();
            let receivedLength = 0;
            const chunks = [];

            while (true) {
                const { done, value } = await reader.read();

                if (done) break;

                chunks.push(value);
                receivedLength += value.length;

                // Call progress callback
                if (callback_ptr) {
                    Module.dynCall_vii(callback_ptr, receivedLength, total);
                }
            }

            // Combine chunks
            const responseArray = new Uint8Array(receivedLength);
            let position = 0;
            for (const chunk of chunks) {
                responseArray.set(chunk, position);
                position += chunk.length;
            }

            // Allocate and return data
            const ptr = Module._malloc(receivedLength);
            HEAPU8.set(responseArray, ptr);
            return ptr;

        } catch (error) {
            console.error('Download progress error:', error);
            return 0;
        }
    });
});

/**
 * Create new web network request
 */
static GWebNetworkRequest* g_web_network_request_new(const gchar *url, const gchar *method) {
    GWebNetworkRequest *request = g_new0(GWebNetworkRequest, 1);
    request->url = g_strdup(url);
    request->method = g_strdup(method ? method : "GET");
    request->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    request->completed = FALSE;
    return request;
}

/**
 * Free web network request
 */
static void g_web_network_request_free(GWebNetworkRequest *request) {
    if (!request) return;

    g_free(request->url);
    g_free(request->method);
    g_free(request->request_body);
    g_free(request->response_data);

    if (request->headers) {
        g_hash_table_destroy(request->headers);
    }
    if (request->response_headers) {
        g_hash_table_destroy(request->response_headers);
    }
    if (request->error) {
        g_error_free(request->error);
    }

    g_free(request);
}

/**
 * Set request header
 */
EMSCRIPTEN_KEEPALIVE
void g_web_network_request_set_header(GWebNetworkRequest *request,
                                      const gchar *name, const gchar *value) {
    g_return_if_fail(request != NULL);
    g_return_if_fail(name != NULL);
    g_return_if_fail(value != NULL);

    g_hash_table_insert(request->headers, g_strdup(name), g_strdup(value));
}

/**
 * Set request body
 */
EMSCRIPTEN_KEEPALIVE
void g_web_network_request_set_body(GWebNetworkRequest *request,
                                    const guchar *data, gsize length) {
    g_return_if_fail(request != NULL);
    g_return_if_fail(data != NULL || length == 0);

    g_free(request->request_body);

    if (length > 0) {
        request->request_body = g_memdup(data, length);
        request->request_body_length = length;
    } else {
        request->request_body = NULL;
        request->request_body_length = 0;
    }
}

/**
 * Convert headers to JSON string for JavaScript
 */
static gchar* g_web_headers_to_json(GHashTable *headers) {
    GString *json = g_string_new("{");

    if (headers && g_hash_table_size(headers) > 0) {
        GHashTableIter iter;
        gpointer key, value;
        gboolean first = TRUE;

        g_hash_table_iter_init(&iter, headers);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            if (!first) {
                g_string_append(json, ",");
            }

            g_string_append_printf(json, "\"%s\":\"%s\"",
                                  (const gchar*)key, (const gchar*)value);
            first = FALSE;
        }
    }

    g_string_append(json, "}");
    return g_string_free(json, FALSE);
}

/**
 * Execute HTTP request using Fetch API
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_network_request_execute(GWebNetworkRequest *request) {
    g_return_val_if_fail(request != NULL, FALSE);
    g_return_val_if_fail(!request->completed, FALSE);

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_fetch_api) {
        g_set_error(&request->error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Fetch API not available");
        return FALSE;
    }

    // Convert headers to JSON
    gchar *headers_json = g_web_headers_to_json(request->headers);

    // Execute fetch request
    gint status_code = 0;
    gchar *response_data = NULL;
    gint response_len = 0;

    gint result = fetch_http_request(request->url, request->method, headers_json,
                                    (const char*)request->request_body,
                                    (int)request->request_body_length,
                                    &status_code, &response_data, &response_len);

    g_free(headers_json);

    if (result == 1) {
        request->status_code = status_code;
        if (response_data && response_len > 0) {
            request->response_data = g_memdup(response_data, response_len);
            request->response_length = response_len;
            free(response_data);  // Free the malloc'd buffer from JS
        }
        request->completed = TRUE;

        g_debug("HTTP %s %s → %d (%zu bytes)", request->method, request->url,
                status_code, request->response_length);
        return TRUE;
    } else {
        g_set_error(&request->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "HTTP request failed");
        return FALSE;
    }
}

/**
 * Get response status code
 */
EMSCRIPTEN_KEEPALIVE
gint g_web_network_request_get_status(GWebNetworkRequest *request) {
    g_return_val_if_fail(request != NULL, 0);
    return request->status_code;
}

/**
 * Get response data
 */
EMSCRIPTEN_KEEPALIVE
const guchar* g_web_network_request_get_data(GWebNetworkRequest *request, gsize *length) {
    g_return_val_if_fail(request != NULL, NULL);

    if (length) {
        *length = request->response_length;
    }

    return request->response_data;
}

/**
 * Convenience function for simple HTTP GET
 */
EMSCRIPTEN_KEEPALIVE
gchar* g_web_http_get(const gchar *url, GError **error) {
    g_return_val_if_fail(url != NULL, NULL);

    GWebNetworkRequest *request = g_web_network_request_new(url, "GET");

    if (!g_web_network_request_execute(request)) {
        if (error) {
            *error = g_error_copy(request->error);
        }
        g_web_network_request_free(request);
        return NULL;
    }

    gchar *result = NULL;
    if (request->response_data && request->response_length > 0) {
        // Ensure null termination for string data
        result = g_malloc(request->response_length + 1);
        memcpy(result, request->response_data, request->response_length);
        result[request->response_length] = '\0';
    }

    g_web_network_request_free(request);
    return result;
}

/**
 * Convenience function for HTTP POST with JSON
 */
EMSCRIPTEN_KEEPALIVE
gchar* g_web_http_post_json(const gchar *url, const gchar *json_data, GError **error) {
    g_return_val_if_fail(url != NULL, NULL);
    g_return_val_if_fail(json_data != NULL, NULL);

    GWebNetworkRequest *request = g_web_network_request_new(url, "POST");
    g_web_network_request_set_header(request, "Content-Type", "application/json");
    g_web_network_request_set_body(request, (const guchar*)json_data, strlen(json_data));

    if (!g_web_network_request_execute(request)) {
        if (error) {
            *error = g_error_copy(request->error);
        }
        g_web_network_request_free(request);
        return NULL;
    }

    gchar *result = NULL;
    if (request->response_data && request->response_length > 0) {
        result = g_malloc(request->response_length + 1);
        memcpy(result, request->response_data, request->response_length);
        result[request->response_length] = '\0';
    }

    g_web_network_request_free(request);
    return result;
}

/**
 * Download file with progress callback
 */
typedef struct {
    void (*callback)(gsize received, gsize total, gpointer user_data);
    gpointer user_data;
} GWebProgressData;

static void progress_callback_wrapper(gsize received, gsize total) {
    // This would be called from JavaScript
    // For now, it's a placeholder for the progress mechanism
}

EMSCRIPTEN_KEEPALIVE
gboolean g_web_download_file(const gchar *url, const gchar *local_path,
                            void (*progress_callback)(gsize, gsize, gpointer),
                            gpointer user_data, GError **error) {
    g_return_val_if_fail(url != NULL, FALSE);
    g_return_val_if_fail(local_path != NULL, FALSE);

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_fetch_api) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Fetch API not available");
        return FALSE;
    }

    // For now, use simple fetch without progress
    // In a real implementation, we'd use the fetch_download_progress function
    GWebNetworkRequest *request = g_web_network_request_new(url, "GET");

    if (!g_web_network_request_execute(request)) {
        if (error) {
            *error = g_error_copy(request->error);
        }
        g_web_network_request_free(request);
        return FALSE;
    }

    // Write response to file
    gboolean success = FALSE;
    if (request->response_data && request->response_length > 0) {
        success = g_file_set_contents(local_path, (const gchar*)request->response_data,
                                     request->response_length, error);
    }

    g_web_network_request_free(request);
    return success;
}

/**
 * Check if URL is reachable
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_url_is_reachable(const gchar *url) {
    g_return_val_if_fail(url != NULL, FALSE);

    GWebNetworkRequest *request = g_web_network_request_new(url, "HEAD");
    gboolean result = g_web_network_request_execute(request);

    if (result) {
        result = (request->status_code >= 200 && request->status_code < 400);
    }

    g_web_network_request_free(request);
    return result;
}

/**
 * Get network statistics
 */
EMSCRIPTEN_KEEPALIVE
void g_web_networking_get_stats(gint *requests_made, gint *bytes_downloaded,
                               gint *bytes_uploaded) {
    // Placeholder for network statistics tracking
    if (requests_made) *requests_made = 0;
    if (bytes_downloaded) *bytes_downloaded = 0;
    if (bytes_uploaded) *bytes_uploaded = 0;
}

/**
 * Benchmark network performance
 */
EMSCRIPTEN_KEEPALIVE
gdouble g_web_networking_benchmark(const gchar *test_url) {
    if (!test_url) {
        test_url = "https://httpbin.org/bytes/1024";  // 1KB test
    }

    const GWebCapabilities *caps = g_web_get_capabilities();
    if (!caps->has_fetch_api) {
        return 0.0;
    }

    gint64 start_time = g_get_monotonic_time();

    GWebNetworkRequest *request = g_web_network_request_new(test_url, "GET");
    gboolean success = g_web_network_request_execute(request);

    gint64 end_time = g_get_monotonic_time();

    gdouble duration_ms = (end_time - start_time) / 1000.0;
    gdouble throughput_kbps = 0.0;

    if (success && request->response_length > 0) {
        throughput_kbps = (request->response_length / 1024.0) / (duration_ms / 1000.0);
        g_message("Network benchmark: %zu bytes in %.2f ms (%.1f KB/s)",
                  request->response_length, duration_ms, throughput_kbps);
    } else {
        g_warning("Network benchmark failed");
    }

    g_web_network_request_free(request);
    return throughput_kbps;
}

/**
 * Initialize web-native networking
 */
void g_web_networking_init(void) {
    const GWebCapabilities *caps = g_web_get_capabilities();

    g_message("Web-native networking initialized:");
    g_message("  Fetch API: %s", caps->has_fetch_api ? "YES" : "NO");

    if (caps->has_fetch_api) {
        g_message("  Features: HTTP/2 multiplexing, CORS handling, streaming");
        g_message("  Supported methods: GET, POST, PUT, DELETE, HEAD, PATCH");

        // Quick connectivity test
        gdouble speed = g_web_networking_benchmark(NULL);
        if (speed > 0) {
            g_message("  Benchmark throughput: %.1f KB/s", speed);
        }
    }
}

/**
 * Cleanup web networking resources
 */
void g_web_networking_cleanup(void) {
    // No specific cleanup needed for Fetch API
    g_debug("Web-native networking cleanup completed");
}