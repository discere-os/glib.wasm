/* GLib WASM - Web-Native File System Implementation
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This implements intelligent storage tiering using browser-native APIs:
 * - /tmp/* → WASM memory (fastest, temporary)
 * - /home/* → OPFS (persistent, fast)
 * - /cache/* → Cache API (network-aware)
 * - http://, https:// → Fetch API (streaming)
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <string.h>

// Forward declarations
#include "web_native_capabilities.h"

// Storage backend types
typedef enum {
    G_WEB_STORAGE_MEMORY,    // WASM linear memory (fast, temporary)
    G_WEB_STORAGE_OPFS,      // Origin Private File System (persistent)
    G_WEB_STORAGE_CACHE,     // Cache API (network-aware)
    G_WEB_STORAGE_FETCH      // Fetch API (HTTP/HTTPS)
} GWebStorageBackend;

// File operation context
typedef struct {
    GWebStorageBackend backend;
    gchar *original_path;
    gchar *resolved_path;
    gboolean async_operation;
} GWebFileContext;

// OPFS operations via JavaScript
EM_JS(int, opfs_file_exists, (const char* path), {
    const filePath = UTF8ToString(path);

    return Asyncify.handleAsync(async () => {
        try {
            const root = await navigator.storage.getDirectory();
            const fileHandle = await root.getFileHandle(filePath);
            return 1; // File exists
        } catch (error) {
            return 0; // File doesn't exist
        }
    });
});

EM_JS(int, opfs_write_file, (const char* path, const char* data, int length), {
    const filePath = UTF8ToString(path);
    const fileData = new Uint8Array(HEAPU8.subarray(data, data + length));

    return Asyncify.handleAsync(async () => {
        try {
            const root = await navigator.storage.getDirectory();

            // Create directory structure if needed
            const pathParts = filePath.split('/').filter(p => p);
            const fileName = pathParts.pop();

            let currentDir = root;
            for (const dirName of pathParts) {
                try {
                    currentDir = await currentDir.getDirectoryHandle(dirName);
                } catch {
                    currentDir = await currentDir.getDirectoryHandle(dirName, { create: true });
                }
            }

            // Write file
            const fileHandle = await currentDir.getFileHandle(fileName, { create: true });
            const writable = await fileHandle.createWritable();
            await writable.write(fileData);
            await writable.close();

            return 1; // Success
        } catch (error) {
            console.error('OPFS write error:', error);
            return 0; // Failure
        }
    });
});

EM_JS(char*, opfs_read_file, (const char* path, int* length), {
    const filePath = UTF8ToString(path);

    return Asyncify.handleAsync(async () => {
        try {
            const root = await navigator.storage.getDirectory();
            const fileHandle = await root.getFileHandle(filePath);
            const file = await fileHandle.getFile();

            const arrayBuffer = await file.arrayBuffer();
            const uint8Array = new Uint8Array(arrayBuffer);

            // Allocate memory in WASM heap
            const ptr = Module._malloc(uint8Array.length + 1);
            HEAPU8.set(uint8Array, ptr);
            HEAPU8[ptr + uint8Array.length] = 0; // Null terminate

            // Set length
            HEAP32[length >> 2] = uint8Array.length;

            return ptr;
        } catch (error) {
            console.error('OPFS read error:', error);
            HEAP32[length >> 2] = 0;
            return 0;
        }
    });
});

EM_JS(int, opfs_delete_file, (const char* path), {
    const filePath = UTF8ToString(path);

    return Asyncify.handleAsync(async () => {
        try {
            const root = await navigator.storage.getDirectory();
            await root.removeEntry(filePath);
            return 1; // Success
        } catch (error) {
            return 0; // Failure or file not found
        }
    });
});

// Fetch API operations
EM_JS(char*, fetch_read_url, (const char* url, int* length, int* status_code), {
    const urlString = UTF8ToString(url);

    return Asyncify.handleAsync(async () => {
        try {
            const response = await fetch(urlString);
            HEAP32[status_code >> 2] = response.status;

            if (!response.ok) {
                HEAP32[length >> 2] = 0;
                return 0;
            }

            const arrayBuffer = await response.arrayBuffer();
            const uint8Array = new Uint8Array(arrayBuffer);

            // Allocate memory in WASM heap
            const ptr = Module._malloc(uint8Array.length + 1);
            HEAPU8.set(uint8Array, ptr);
            HEAPU8[ptr + uint8Array.length] = 0; // Null terminate

            HEAP32[length >> 2] = uint8Array.length;
            return ptr;
        } catch (error) {
            console.error('Fetch error:', error);
            HEAP32[length >> 2] = 0;
            HEAP32[status_code >> 2] = 0;
            return 0;
        }
    });
});

/**
 * Determine appropriate storage backend based on path
 */
static GWebStorageBackend g_web_get_storage_backend(const gchar *path) {
    if (!path) return G_WEB_STORAGE_MEMORY;

    // HTTP/HTTPS URLs
    if (g_str_has_prefix(path, "http://") || g_str_has_prefix(path, "https://")) {
        return G_WEB_STORAGE_FETCH;
    }

    // File system paths
    if (g_str_has_prefix(path, "/tmp/")) {
        return G_WEB_STORAGE_MEMORY;
    }

    if (g_str_has_prefix(path, "/home/")) {
        const GWebCapabilities *caps = g_web_get_capabilities();
        return caps->has_opfs ? G_WEB_STORAGE_OPFS : G_WEB_STORAGE_MEMORY;
    }

    if (g_str_has_prefix(path, "/cache/")) {
        return G_WEB_STORAGE_CACHE;
    }

    // Default to memory storage
    return G_WEB_STORAGE_MEMORY;
}

/**
 * Create file operation context
 */
static GWebFileContext* g_web_file_context_new(const gchar *path) {
    GWebFileContext *context = g_new0(GWebFileContext, 1);
    context->backend = g_web_get_storage_backend(path);
    context->original_path = g_strdup(path);

    // Resolve path for specific backend
    switch (context->backend) {
        case G_WEB_STORAGE_OPFS:
            // Remove /home/ prefix for OPFS
            if (g_str_has_prefix(path, "/home/")) {
                context->resolved_path = g_strdup(path + 6);
            } else {
                context->resolved_path = g_strdup(path);
            }
            break;
        case G_WEB_STORAGE_MEMORY:
            context->resolved_path = g_strdup(path);
            break;
        default:
            context->resolved_path = g_strdup(path);
            break;
    }

    return context;
}

/**
 * Free file operation context
 */
static void g_web_file_context_free(GWebFileContext *context) {
    if (context) {
        g_free(context->original_path);
        g_free(context->resolved_path);
        g_free(context);
    }
}

/**
 * Enhanced g_file_set_contents() with intelligent storage
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_file_set_contents(const gchar *filename,
                                 const gchar *contents,
                                 gssize length,
                                 GError **error) {
    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(contents != NULL, FALSE);

    if (length < 0) {
        length = strlen(contents);
    }

    GWebFileContext *ctx = g_web_file_context_new(filename);
    gboolean success = FALSE;

    switch (ctx->backend) {
        case G_WEB_STORAGE_OPFS: {
            const GWebCapabilities *caps = g_web_get_capabilities();
            if (!caps->has_opfs) {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOSYS,
                           "OPFS not available in this browser");
                break;
            }

            gint result = opfs_write_file(ctx->resolved_path, contents, (int)length);
            if (result == 1) {
                success = TRUE;
                g_debug("Wrote %ld bytes to OPFS: %s", (long)length, filename);
            } else {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           "Failed to write to OPFS: %s", filename);
            }
            break;
        }

        case G_WEB_STORAGE_MEMORY:
        default:
            // Fall back to standard GLib implementation for memory storage
            success = g_file_set_contents(filename, contents, length, error);
            if (success) {
                g_debug("Wrote %ld bytes to memory: %s", (long)length, filename);
            }
            break;
    }

    g_web_file_context_free(ctx);
    return success;
}

/**
 * Enhanced g_file_get_contents() with intelligent storage
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_file_get_contents(const gchar *filename,
                                 gchar **contents,
                                 gsize *length,
                                 GError **error) {
    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(contents != NULL, FALSE);

    GWebFileContext *ctx = g_web_file_context_new(filename);
    gboolean success = FALSE;

    switch (ctx->backend) {
        case G_WEB_STORAGE_OPFS: {
            const GWebCapabilities *caps = g_web_get_capabilities();
            if (!caps->has_opfs) {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOSYS,
                           "OPFS not available in this browser");
                break;
            }

            int read_length = 0;
            char *data = opfs_read_file(ctx->resolved_path, &read_length);

            if (data && read_length > 0) {
                *contents = g_memdup(data, read_length + 1);  // Include null terminator
                if (length) *length = read_length;
                free(data);  // Free the WASM heap allocation
                success = TRUE;
                g_debug("Read %d bytes from OPFS: %s", read_length, filename);
            } else {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                           "Failed to read from OPFS: %s", filename);
            }
            break;
        }

        case G_WEB_STORAGE_FETCH: {
            if (!g_web_get_capabilities()->has_fetch_api) {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOSYS,
                           "Fetch API not available");
                break;
            }

            int read_length = 0;
            int status_code = 0;
            char *data = fetch_read_url(filename, &read_length, &status_code);

            if (data && read_length > 0 && status_code >= 200 && status_code < 300) {
                *contents = g_memdup(data, read_length + 1);
                if (length) *length = read_length;
                free(data);
                success = TRUE;
                g_debug("Fetched %d bytes from URL: %s (HTTP %d)",
                        read_length, filename, status_code);
            } else {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           "HTTP request failed: %s (status: %d)", filename, status_code);
                if (data) free(data);
            }
            break;
        }

        case G_WEB_STORAGE_MEMORY:
        default:
            // Fall back to standard GLib implementation
            success = g_file_get_contents(filename, contents, length, error);
            if (success) {
                g_debug("Read %lu bytes from memory: %s",
                        length ? (unsigned long)*length : 0UL, filename);
            }
            break;
    }

    g_web_file_context_free(ctx);
    return success;
}

/**
 * Enhanced g_file_test() with multi-tier storage support
 */
EMSCRIPTEN_KEEPALIVE
gboolean g_web_file_test(const gchar *filename, GFileTest test) {
    g_return_val_if_fail(filename != NULL, FALSE);

    GWebFileContext *ctx = g_web_file_context_new(filename);
    gboolean result = FALSE;

    // For now, only support G_FILE_TEST_EXISTS
    if (test & G_FILE_TEST_EXISTS) {
        switch (ctx->backend) {
            case G_WEB_STORAGE_OPFS: {
                const GWebCapabilities *caps = g_web_get_capabilities();
                if (caps->has_opfs) {
                    result = opfs_file_exists(ctx->resolved_path);
                }
                break;
            }

            case G_WEB_STORAGE_FETCH:
                // For HTTP URLs, we'd need to do a HEAD request
                // For now, assume they exist (will fail on actual read if not)
                result = TRUE;
                break;

            case G_WEB_STORAGE_MEMORY:
            default:
                result = g_file_test(filename, test);
                break;
        }
    }

    g_web_file_context_free(ctx);
    return result;
}

/**
 * Get file system statistics
 */
EMSCRIPTEN_KEEPALIVE
void g_web_filesystem_get_stats(gint *opfs_files, gint *memory_files, gint *fetch_requests) {
    // This would track filesystem usage statistics
    // For now, return placeholder values
    if (opfs_files) *opfs_files = 0;
    if (memory_files) *memory_files = 0;
    if (fetch_requests) *fetch_requests = 0;
}

/**
 * Initialize web-native filesystem
 */
void g_web_filesystem_init(void) {
    const GWebCapabilities *caps = g_web_get_capabilities();

    g_message("Web-native filesystem initialized:");
    g_message("  OPFS support: %s", caps->has_opfs ? "YES" : "NO");
    g_message("  Fetch API support: %s", caps->has_fetch_api ? "YES" : "NO");
    g_message("  Storage mapping:");
    g_message("    /tmp/* → WASM memory (temporary)");
    g_message("    /home/* → %s (persistent)", caps->has_opfs ? "OPFS" : "memory fallback");
    g_message("    /cache/* → Cache API (future)");
    g_message("    http(s):// → Fetch API");
}

/**
 * Cleanup web filesystem resources
 */
void g_web_filesystem_cleanup(void) {
    // Cleanup any resources if needed
    g_debug("Web-native filesystem cleanup completed");
}